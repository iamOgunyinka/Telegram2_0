#include "account.hpp"
#include <QDebug>

Account::Account( int const index , std::shared_ptr<LoginInformation> login_info , QObject* parent):
  QObject( parent ), login_info_{ login_info }, index_{ index }
{
}

Account::~Account()
{
  bg_search_results_.clear();
  thread_is_running = false;
  Cleanup();
  if( bg_search_timer_ptr_ ){
    bg_search_timer_ptr_->stop();
    bg_search_timer_ptr_.reset();
  }

  login_info_->is_logged_in_ = false;
  client_.reset();
}

void Account::Cleanup()
{
  if( background_thread_ ){
    if( background_thread_->isRunning() ){
      background_thread_->quit();
      background_thread_->wait();
    }
    background_thread_.reset();
  }
  if( background_worker_ ){
    delete background_worker_;
    background_worker_ = nullptr;
  }
  authentication_query_id_ = current_request_id = 0;
  request_handlers_.clear();
}

void Account::InitiateLoginSequence()
{
  login_info_->is_logged_in_ = true;
  Cleanup();
  if( !client_ ) {
    client_ = std::make_shared<td::Client>();
  }
  background_thread_ = std::make_shared<QThread>( this );
  background_worker_ = new BackgroundWorker( client_, request_handlers_, thread_is_running,
                                             this->parent(), authentication_query_id_ );
  QObject::connect( background_worker_, &BackgroundWorker::requested_phone_number, [=]{
    auto const & phone_number { login_info_->phone_number_ };
    background_worker_->SendRequest( NextID(),
                                     td_api::make_object<td_api::setAuthenticationPhoneNumber>(
                                       phone_number.toStdString(), nullptr),
                                     CreateAuthenticationHandler() );
  });
  QObject::connect( background_worker_, &BackgroundWorker::requested_app_paramters,
                    [this]
  {
    auto parameters = td_api::make_object<td_api::tdlibParameters>();
    parameters->database_directory_ = login_info_->phone_number_.toStdString();
    parameters->use_message_database_ = true;
    parameters->use_secret_chats_ = true;
    parameters->api_id_ = AppID;
    parameters->api_hash_ = "7ea9bdf786f0fd19bf511edef0159e4c";
    parameters->system_language_code_ = "en";
    parameters->device_model_ = "Desktop";
    parameters->system_version_ = "Windows 10";
    parameters->application_version_ = "1.5";
    parameters->enable_storage_optimizer_ = true;
    background_worker_->SendRequest( NextID(), td_api::make_object<td_api::setTdlibParameters>(
                                       std::move(parameters)), CreateAuthenticationHandler() );
  });
  QObject::connect( background_worker_, &BackgroundWorker::requested_encryption_code, [=]{
    background_worker_->SendRequest( NextID(),
                                     td_api::make_object<td_api::checkDatabaseEncryptionKey>(
                                       login_info_->encryption_key_.toStdString() ),
                                     CreateAuthenticationHandler() );
  });
  QObject::connect( background_worker_, &BackgroundWorker::requested_authorization_code, [=]{
    emit requested_authorization_code( index_ );
  });
  QObject::connect( background_worker_, &BackgroundWorker::requested_authorization_password, [=]{
    emit requested_authorization_password( index_ );
  });
  QObject::connect( background_worker_, &BackgroundWorker::handshake_completed,
                    [=]( bool const has_error ){
    login_info_->is_logged_in_ = !has_error;
    if( login_info_->is_logged_in_ ){
      emit handshake_completed( index_ );
      RequestForChats();
      background_worker_->StartPolling();
    }
  });

  QObject::connect( background_thread_.get(), &QThread::started, background_worker_,
                    &BackgroundWorker::InitiateLoginSequence );
  QObject::connect( background_thread_.get(), &QThread::finished, background_thread_.get(),
                    &QThread::deleteLater );
  background_worker_->moveToThread( background_thread_.get() );
  background_thread_->start();
}

void Account::SendRequest( FunctionPtr request, CustomRequestHandler handler )
{
  background_worker_->SendRequest( NextID(), std::move( request ), std::move( handler ) );
}

void Account::SendAuthorizationRequest( FunctionPtr request )
{
  SendRequest( std::move( request ), CreateAuthenticationHandler() );
}

void Account::RequestForChats()
{
  background_worker_->SendRequest( NextID(), td_api::make_object<td_api::getChats>(
                                     std::numeric_limits<std::int64_t>::max(), 0,
                                     MaxResultAllowed ), []( ObjectPtr response_ptr )
  {
    qDebug() << "We are here";
    (void) response_ptr;
    /*
    if (response_ptr->get_id() == td_api::error::ID) {
      return;
    }
    auto chats = td::move_tl_object_as<td_api::chats>(response_ptr);
    for (auto chat_id : chats->chat_ids_) {
      qDebug() << "[id:" << chat_id << "] [title:" << chat_title_[chat_id] << "]" << std::endl;
    }
    */
  });
}

unsigned long long Account::NextID()
{
  return ++current_request_id;
}

CustomRequestHandler Account::CreateAuthenticationHandler()
{
  return [this, id = authentication_query_id_]( ObjectPtr object )
  {
    if (id == authentication_query_id_) {
      CheckAuthenticationError( std::move( object ) );
    }
  };
}

void Account::CheckAuthenticationError( ObjectPtr object )
{
  if ( object->get_id() == td_api::error::ID ){
    background_worker_->SetHasError();
    auto error = to_string( td::move_tl_object_as<td_api::error>(object) );
    auto error_message{ tr( "%1\n\n%2" ).arg( login_info_->phone_number_ )
          .arg( QString::fromStdString( error )) };
    QMetaObject::invokeMethod( parent(), "ShowError", Qt::QueuedConnection,
                               Q_ARG( int, index_ ),
                               Q_ARG( QString, error_message ));
  }
}

decltype (BackgroundWorker::users_)& Account::GetUsers(){
  return background_worker_->users_;
}

std::map<std::int64_t, std::string>& Account::ChatTitles()
{
  return background_worker_->ChatTitles();
}

SearchResultList& Account::GetSearchResult()
{
  return bg_search_results_;
}

void Account::PerformSearch(FunctionPtr request, CustomRequestHandler handler)
{
  bg_search_results_.clear();
  bg_messages_extracted_ = 0;
  SendRequest( std::move( request ), std::move( handler ) );
}

void Account::StartBackgroundSearch( std::string const &text, int const timeout,
                                     CustomRequestHandler && handler )
{
  if( bg_search_timer_ptr_ && bg_search_timer_ptr_->isActive() ){
    bg_search_timer_ptr_->stop();
  }
  bg_search_results_.clear();
  bg_messages_extracted_ = 0;
  bg_search_timer_ptr_ = std::make_unique<QTimer>();
  auto callback = [=]{
    RequestForChats();
    if( thread_is_running ){
      background_worker_->SendRequest( NextID(), td_api::make_object<td::td_api::searchMessages>(
                                         text, 0, 0, 0, MaxResultAllowed ), handler );
    }
  };
  QObject::connect( bg_search_timer_ptr_.get(), &QTimer::timeout, callback );
  callback();
  bg_search_timer_ptr_->start( ThousandMilliseconds * timeout );
}

void Account::StopBackgroundSearch()
{
  bg_search_timer_ptr_->stop();
}
