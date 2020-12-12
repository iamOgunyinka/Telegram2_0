#include "backgroundworker.hpp"
#include <QThread>
#include <QDebug>
#include <td/telegram/td_api.h>

BackgroundWorker::BackgroundWorker(
    std::shared_ptr<td::Client>& client, CustomRequestHandlerMap& handlers_,
    bool& running, QObject* parent_window, unsigned long long & auth_id, QObject* parent ):
  QObject{ parent }, client_{ client }, request_handlers_{ handlers_ }, running_{ running },
  authentication_query_id_{ auth_id }, parent_window_{ parent_window }
{
}

BackgroundWorker::~BackgroundWorker()
{
  authorization_granted_ = false;
  error_set_ = false;
  need_restart_ = false;
  request_sent_ = false;
  running_ = false;
  authorization_state_ptr_.reset();
  chat_title_.clear();
}

void BackgroundWorker::InitiateLoginSequence()
{
  int const initial_request = 0;
  td::Client::execute({ initial_request, td_api::make_object<td_api::setLogVerbosityLevel>(1) });
  while(!authorization_granted_){
    if( error_set_ ){
      break;
    }
    if( need_restart_ ){
      Restart();
    }
    auto response = std::make_shared<td::Client::Response>( client_->receive( MaxTimeout ));
    if( !response->object ){
      error_set_ = true;
      break;
    }
    ProcessResponse( response );
  }
  emit handshake_completed( error_set_ );
}

void BackgroundWorker::Restart()
{
  error_set_ = false;
  need_restart_ = false;
  authorization_granted_ = false;
  request_handlers_.clear();
  client_ = std::make_shared<td::Client>();
}

void BackgroundWorker::ProcessResponse( ResponsePtr response_ptr )
{
  if( !response_ptr->object ){
    return;
  }

  static int const initial_request = 0;

  if( response_ptr->id == initial_request ){
    ProcessUpdate( std::move( response_ptr->object ) );
  } else {
    auto it = request_handlers_.find( response_ptr->id );
    if (it != request_handlers_.end()) {
      it->second( std::move( response_ptr->object ) );
    }
  }
}

void BackgroundWorker::ProcessUpdate( ObjectPtr ptr )
{
  td_api::downcast_call( *ptr,
                         overloaded(
                           [this](td_api::updateAuthorizationState& update_authorization_state)
                         {
                           authorization_state_ptr_ = std::move(update_authorization_state.authorization_state_);
                           OnAuthorizationStateUpdate();
                         }, [this](td_api::updateUser& update_user)
  {
    qDebug() << update_user.user_->username_.c_str();
    auto user_id = update_user.user_->id_;
    users_[user_id] = std::move(update_user.user_);
  }, [this](td_api::updateNewChat& chat)
  {
    qDebug() << chat.chat_->title_.c_str();
    chat_title_[chat.chat_->id_] = chat.chat_->title_;
  },
  [=]( auto& ){}));
}

void BackgroundWorker::OnAuthorizationStateUpdate()
{
  ++authentication_query_id_;
  td_api::downcast_call( *authorization_state_ptr_, overloaded(
                           [this]( td_api::authorizationStateReady& ) {
                           authorization_granted_ = true;
                         },
                         [this](td_api::authorizationStateLoggingOut&) {
    authorization_granted_ = false;
  },
  [this](td_api::authorizationStateClosing&) {
    authorization_granted_ = false;
  },
  [this](td_api::authorizationStateClosed&) {
    authorization_granted_ = false;
    need_restart_ = true;
  },
  [this](td_api::authorizationStateWaitCode&) {
    emit requested_authorization_code();
  },
  []( td_api::authorizationStateWaitRegistration & ){},
  [this](td_api::authorizationStateWaitPassword&) {
    emit requested_authorization_password();
  },
  [this](td_api::authorizationStateWaitPhoneNumber&) {
    emit requested_phone_number();
  },
  [this](td_api::authorizationStateWaitEncryptionKey&) {
    emit requested_encryption_code();
  },
  [this](td_api::authorizationStateWaitTdlibParameters&) {
    emit requested_app_paramters();
  }));
}

void BackgroundWorker::SendRequest(unsigned long long query_id, FunctionPtr request,
                                   CustomRequestHandler handler)
{
  if( handler ){
    request_handlers_.emplace( query_id, std::move( handler ) );
  }
  client_->send( { query_id, std::move( request ) } );
  request_sent_ = true;
}

void BackgroundWorker::SetHasError()
{
  error_set_ = true;
}

std::map<std::int64_t, std::string>& BackgroundWorker::ChatTitles()
{
  return chat_title_;
}

void BackgroundWorker::StartPolling()
{
  while( running_ )
  {
    if( !request_sent_ ) {
      QThread::sleep( 1 );
      continue;
    }
    // Qt mandates that meta-objects be copyable, std::unique_ptr aren't, std::shared_ptr is
    auto response = std::make_shared<td::Client::Response>( client_->receive( MaxTimeout ) );
    request_sent_ = response->object != nullptr;
    ProcessResponse( response );
  }
  authorization_granted_ = false;
}
