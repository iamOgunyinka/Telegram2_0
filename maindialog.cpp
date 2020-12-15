#include "maindialog.hpp"
#include "ui_maindialog.h"
#include <QTextStream>
#include <QStandardItem>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#include <QAction>
#include <QFileDialog>
#include <QMenu>
#include <QStyle>
#include <xlsxdocument.h>
#include "accountwidget.hpp"
#include "registrationdialog.hpp"
#include "schedulerdialog.hpp"

char const * const MainDialog::startup_filename{ "config.ini" };

MainDialog::MainDialog(QWidget *parent): QDialog(parent), ui(new Ui::MainDialog)
{
  ui->setupUi( this );
  QObject::connect( ui->new_user_button, &QPushButton::clicked, this,
                    &MainDialog::OnCreateUserButtonClicked );
  QObject::connect( ui->login_user_button, &QPushButton::clicked, [this]
  {
    LogUserIn();
  });
  QObject::connect( ui->logout_button, &QPushButton::clicked, this,
                    &MainDialog::OnLogoutButtonClicked );
  QObject::connect( ui->remove_user_button, &QPushButton::clicked, this,
                    &MainDialog::OnRemoveAccountButtonClicked );
  QObject::connect( ui->schedule_start_button, &QPushButton::clicked, this,
                    &MainDialog::OnStartScheduledSearchClicked );
  QObject::connect( ui->search_button, &QPushButton::clicked, this,
                    &MainDialog::OnSearchButtonClicked );

  QObject::connect( this, &MainDialog::search_done, [=]( SearchResultType t, int const index )
  {
    QMetaObject::invokeMethod(this, "OnAccountSearchDone", Qt::QueuedConnection,
                              Q_ARG( SearchResultType, t ), Q_ARG( int, index ));
  });

  QObject::connect( ui->select_all_checkbox, &QCheckBox::toggled, [this]( bool const is_checked )
  {
    for( int i = 0; i < ui->flayout->count(); ++i ){
      auto widget = qobject_cast<AccountWidget*>( ui->flayout->itemAt( i )->widget() );
      widget->SetChecked( is_checked );
    }
  });

  QObject::connect( ui->schedule_stop_button, &QPushButton::clicked, this,
                    &MainDialog::OnStopScheduledSearchClicked );
  QObject::connect( ui->export_result_button, &QPushButton::clicked, [=]{
    if( !background_search_scheduled_ ){
      StartExport();
    }
  });
  setWindowIcon( qApp->style()->standardPixmap( QStyle::SP_DesktopIcon ));
  DisableAllButtons();
  LoadStartupFile();
}

void MainDialog::DisableAllButtons()
{
  ui->search_button->setEnabled( false );
  ui->export_result_button->setEnabled( false );
  ui->schedule_stop_button->setEnabled( false );
  ui->schedule_start_button->setEnabled( false );
  ui->timer_display_button->setEnabled( false );
  ui->remove_user_button->setEnabled( false );
  ui->login_user_button->setEnabled( false );
  ui->working_time_button->setEnabled( false );
}

MainDialog::~MainDialog()
{
  delete ui;
}

void MainDialog::OnAccountSearchDone( SearchResultType type, int const index )
{
  ui->search_button->setEnabled( true );
  ++requests_responded_to_;
  if( type == SearchResultType::NoResult ){
    QMessageBox::information( this, tr("Search"), tr("No result found"));
    return;
  } else if( type == SearchResultType::ServerError ){
    QMessageBox::critical( this, tr("Search"), tr("There was an error on the server-side, "
                                                  "so search could not be completed"));
    return;
  }

  auto const & result{ telegram_accounts_[index]->GetSearchResult() };

  if( !background_search_scheduled_ && proposed_requests_ == 1 ){
    ui->textEdit->clear();
    for( auto const & message: result ){
      ui->textEdit->append( tr( "%1(%2)(%3): %4" ).arg( message.sender_firstname )
                            .arg( message.sender_username ).arg( message.chat_name )
                            .arg( message.text ) );
    }
  } else {
    ui->textEdit->append( tr( "%1 items found for %2" ).arg( result.size() )
                          .arg( logins_[index]->phone_number_ ) );
  }
  if( !background_search_scheduled_ && requests_responded_to_ == proposed_requests_ ){
    ui->export_result_button->setEnabled( true );
  }
}

void MainDialog::LoadStartupFile()
{
  if( !QFileInfo::exists( startup_filename ) ){
    return;
  }
  QFile file{ startup_filename };
  if( !file.open( QIODevice::ReadOnly ) ){
    QMessageBox::critical( this, tr("Error"), file.errorString() );
    return;
  }

  QTextStream text_stream{ &file };
  QString line{};
  int index = 0;
  bool const is_logged_in = false;

  telegram_accounts_.clear();
  logins_.clear();

  while( text_stream.readLineInto( &line ) ){
    QStringList splits{ line.split( ":" ) };
    if( splits.size() < 2 ){
      continue;
    }
    auto& phone_number = splits[0];
    auto& encryption_key = splits[1];
    auto login_info = LoginInformation{ is_logged_in, phone_number, encryption_key };
    auto login_info_ptr = std::make_shared<LoginInformation>( std::move( login_info ) );
    logins_.push_back( login_info_ptr );
    telegram_accounts_.push_back( std::make_shared<Account>(index, login_info_ptr, this ) );
    AddLoginInformation( phone_number );
    ++index;
  }

  if( !logins_.isEmpty() ){
    ui->login_user_button->setVisible( true );
    ui->remove_user_button->setVisible( true );
    ui->login_user_button->setEnabled( true );
    ui->remove_user_button->setEnabled( true );
  }
}

bool MainDialog::RemoveDir( QString const & dir_name )
{
  bool result = true;
  QDir dir(dir_name);

  if (dir.exists()) {
    Q_FOREACH(QFileInfo info, dir.entryInfoList( QDir::NoDotAndDotDot | QDir::System |
                                                 QDir::Hidden | QDir::AllDirs |
                                                 QDir::Files, QDir::DirsFirst ) )
    {
      if (info.isDir()){
        result = RemoveDir( info.absoluteFilePath() );
      } else {
        result = QFile::remove( info.absoluteFilePath() );
      }
      if (!result) {
        return result;
      }
    }
    result = QDir().rmdir(dir_name);
  }
  return result;
}

void MainDialog::OnSearchButtonClicked()
{
  proposed_requests_ = requests_responded_to_ = 0;
  ui->export_result_button->setEnabled( false );
  ui->textEdit->clear();
  if( background_search_scheduled_ ){
    QMessageBox::critical( this, tr("Task"), tr("You have a background search ongoing, suspend "
                                                "it first" ));
    return;
  }
  search_text_ = ui->query_line->text().trimmed().toStdString();
  for( int i = 0; i != ui->flayout->count(); ++i ){
    auto account_widget = qobject_cast<AccountWidget*>( ui->flayout->itemAt( i )->widget() );
    if( !account_widget->IsSelected() || !logins_[i]->is_logged_in_ ){
      continue;
    }
    telegram_accounts_[i]->PerformSearch( td_api::make_object<td_api::searchMessages>(
                                            search_text_, 0, 0, 0, MaxResultAllowed ),
                                          [=]( ObjectPtr result )
    {
      OnSearchResultObtained( i, search_text_,
                              std::make_shared<ObjectPtr>( std::move( result ) ));
    });
    ++proposed_requests_;
  }
  ui->search_button->setEnabled( proposed_requests_ != 0 );
}

void MainDialog::OnStartScheduledSearchClicked()
{
  SchedulerDialog scheduler_dialog{ this };
  if( scheduler_dialog.exec() != QDialog::Accepted ) return;
  search_text_ = scheduler_dialog.GetText().toStdString();

  bg_elapsed_timer_ = requests_responded_to_ = proposed_requests_ = 0;
  int const timeout{ scheduler_dialog.GetTime() };
  for( int i = 0; i != ui->flayout->count(); ++i ){
    auto account_widget = qobject_cast<AccountWidget*>( ui->flayout->itemAt( i )->widget() );
    if( !account_widget->IsSelected() || !logins_[i]->is_logged_in_ ){
      continue;
    }
    telegram_accounts_[i]->StartBackgroundSearch( search_text_, timeout, [=]( ObjectPtr ptr )
    {
      OnSearchResultObtained( i, search_text_,
                              std::make_shared<ObjectPtr>( std::move( ptr ) ) );
    });
    ++proposed_requests_;
  }
  if( proposed_requests_ == 0 ){
    QMessageBox::information( this, tr("Search"),
                              tr("Choose an account(online) to conduct a search on" ));
    return;
  }
  bg_search_elapsed_timer_ = std::make_unique<QTimer>();
  QObject::connect( bg_search_elapsed_timer_.get(), &QTimer::timeout, [this]
  {
    auto& seconds = ++bg_elapsed_timer_;
    int minutes = seconds / Mins, hours = minutes / Mins;
    ui->working_time_button->setText( tr( "%1h %2m %3s" ).arg( hours ).arg( minutes % Mins )
                                      .arg( seconds % Mins ));
  });
  background_search_scheduled_ = true;
  ui->schedule_start_button->setEnabled( false );
  ui->schedule_stop_button->setEnabled( true );
  ui->export_result_button->setEnabled( false );
  ui->textEdit->clear();
  ui->timer_display_button->setText( tr( "Interval: %1s" ).arg( timeout ) );
  bg_search_elapsed_timer_->start( ThousandMilliseconds );
}

void MainDialog::OnSearchResultObtained( int const index, std::string const & text,
                                         std::shared_ptr<ObjectPtr> ptr )
{
  if( (*ptr)->get_id() == td_api::error::ID ){
    if( !background_search_scheduled_ ){
      emit search_done( SearchResultType::ServerError, index );
    }
    return;
  }
  auto messages_ptr = td::move_tl_object_as<td_api::messages>( *ptr );
  auto& messages = messages_ptr->messages_;
  auto& messages_extracted{ telegram_accounts_[index]->bg_messages_extracted_ };
  if( ( messages_extracted >= messages_ptr->total_count_ ) || messages.empty() ){
    if( !background_search_scheduled_ ){
      emit search_done( SearchResultType::NoResult, index );
    }
    return;
  }
  auto& users { telegram_accounts_[index]->GetUsers() };
  auto& search_results{ telegram_accounts_[index]->GetSearchResult() };
  auto& chat_titles{ telegram_accounts_[index]->ChatTitles() };
  for( std::size_t i = 0; i != messages.size(); ++i ){
    auto& message = messages[i];
    int const message_type_id = message->content_->get_id();
    if ( message_type_id == td_api::messageText::ID ) {
      auto const content = td::move_tl_object_as<td_api::messageText>(message->content_);
      auto const sender_iter = users.find( message->sender_user_id_ );
      bool const has_sender_info = sender_iter != users.end();

      if( has_sender_info ){
        auto& sender = sender_iter->second;
        auto const sender_first_name = QString::fromStdString( sender->first_name_ );
        auto const sender_username = QString::fromStdString( sender->username_ );
        auto const text_sent = QString::fromStdString( content->text_->text_ );
        auto const chat_name = QString::fromStdString( chat_titles[message->chat_id_] );
        std::int64_t chat_id = message->chat_id_;
        std::int64_t message_id = message->id_;
        std::int32_t date_id = message->date_;
        search_results.emplace_back( SearchResult{ chat_name, sender_first_name, sender_username,
                                                   text_sent, message_id, chat_id, date_id } );
      } else if( !has_sender_info && message->is_channel_post_ ){
        QString text_sent = QString::fromStdString( content->text_->text_ );
        QString chat_name = QString::fromStdString( chat_titles[message->chat_id_] );
        std::int64_t chat_id = message->chat_id_;
        std::int64_t message_id = message->id_;
        std::int32_t date_id = message->date_;
        search_results.emplace_back( SearchResult{ chat_name, "", "", text_sent, message_id,
                                                   chat_id, date_id } );
      } else {
        continue;
      }
    } else {
      qDebug() << message_type_id;
    }
    if( ++messages_extracted >= messages_ptr->total_count_ ){
      emit search_done( SearchResultType::Successful, index );
      return;
    }
  }

  if( ( messages_ptr->total_count_ > messages_extracted ) && !search_results.empty() ){
    auto const & last_request{ search_results.back() };
    telegram_accounts_[index]->SendRequest( td::make_tl_object<td_api::searchMessages>(
                                              text, last_request.date_id,
                                              last_request.chat_id, last_request.message_id,
                                              MaxResultAllowed ), [=]( ObjectPtr r )
    {
      OnSearchResultObtained( index, text, std::make_shared<ObjectPtr>( std::move( r ) ) );
    });
  }
}

void MainDialog::StartExport()
{
  QString const directory{ QFileDialog::getExistingDirectory( this, tr("Directory")) };
  for( int i = 0; i != logins_.size(); ++i )
  {
    auto account_widget = qobject_cast<AccountWidget*>( ui->flayout->itemAt( i )->widget() );
    if( !telegram_accounts_[i] || !logins_[i]->is_logged_in_ ||
        !account_widget->IsSelected() )
    {
      continue;
    }
    if( background_search_scheduled_ ){
      telegram_accounts_[i]->StopBackgroundSearch();
    }
    if( directory.isEmpty() || directory.isNull() ) {
      continue;
    }
    auto& search_result{ telegram_accounts_[i]->GetSearchResult() };
    if( !search_result.empty() ) {
      ExportSearchResult( directory, i, search_result );
    }
  }
  if( !directory.isEmpty() ){
    QMessageBox::information( this, tr("Status"), tr("File(s) saved successfully"));
  }
}

void MainDialog::OnStopScheduledSearchClicked()
{
  ui->schedule_stop_button->setEnabled( false );
  bg_search_elapsed_timer_->stop();
  bg_search_elapsed_timer_.reset();
  background_search_scheduled_ = false;
  for( int i = 0; i != logins_.size(); ++i ){
    auto account_widget = qobject_cast<AccountWidget*>( ui->flayout->itemAt( i )->widget() );
    if( logins_[i]->is_logged_in_ && account_widget->IsSelected() ){
      telegram_accounts_[i]->StopBackgroundSearch();
    }
  }
  ui->textEdit->append( tr("Background search stopped"));
  ui->schedule_start_button->setDisabled( false );
  StartExport();
}

void MainDialog::ExportSearchResult( QString const &dir_name, int const index,
                                     SearchResultList const &search_result )
{
  QString const filename{ dir_name + "/" + logins_[index]->phone_number_ + ".xlsx" };
  if( QFileInfo::exists( filename ) ){
    QDir{}.remove( filename );
  }
  QXlsx::Document doc( filename );
  if( !doc.addSheet( "Sheet1" ) ){
    QMetaObject::invokeMethod( this, "DisplayError", Qt::QueuedConnection,
                               Q_ARG( QString, tr( "Unable to write to `result.xlsx`" ) ) );
    return;
  }
  doc.selectSheet( "Sheet1" );
  auto sheet = doc.currentWorksheet();

  sheet->write( "A1", tr( "S/N" ) );
  sheet->write( "C1", tr( "Group/Chat name" ) );
  sheet->write( "E1", tr( "Sender name" ) );
  sheet->write( "G1", tr("@username(sender)" ) );
  sheet->write( "I1", tr( "Text" ) );
  sheet->write( "K1", tr( "Phone number" ) );
  sheet->write( "M1", logins_[index]->phone_number_ );
  sheet->write( "O1", tr("Keyword"));
  sheet->write( "Q1", QString::fromStdString( search_text_ ) );

  std::size_t counter{ 3 };
  QSet<QString> unique_elements{};
  auto const widget = qobject_cast<AccountWidget*>( ui->flayout->itemAt( index )->widget() );
  auto const & selected_groups = widget->SelectedItems();
  bool const using_filter = selected_groups.size() > 0;

  for( auto const & item: search_result ){
    if( using_filter && !selected_groups.contains( item.chat_id ) ){
      continue;
    }
    auto const unique_text = ( item.sender_username.toLower() + ": " + item.text.toLower() )
        .trimmed();
    if( unique_elements.contains( unique_text ) ){
      continue;
    }
    unique_elements.insert( unique_text );
    sheet->write( QString( "A%1" ).arg( counter ), counter - 2 );
    sheet->write( QString( "C%1" ).arg( counter ), item.chat_name );
    sheet->write( QString( "E%1" ).arg( counter ), item.sender_firstname );
    sheet->write( QString( "G%1" ).arg( counter ), item.sender_username );
    sheet->write( QString( "I%1" ).arg( counter ), item.text );
    ++counter;
  }
  doc.save();
}

void MainDialog::OnRemoveAccountButtonClicked()
{
  if( ui->flayout->count() == 0 ||
      QMessageBox::question( this, tr( "Confirmation" ),
                             tr( "Are you sure you want to delete these accounts?" )) ==
      QMessageBox::No )
  {
    return;
  }

  QVector<int> elements_marked_for_deletion{};
  QVector<QLayoutItem*> marked_widgets{};

  for( int i = 0; i != ui->flayout->count(); ++i ){
    auto item = ui->flayout->itemAt( i );
    auto account_widget = qobject_cast<AccountWidget*>( item->widget() );
    if( !account_widget->IsSelected() || logins_[i]->is_logged_in_ ) {
      continue;
    }
    RemoveDir( account_widget->PhoneNumber() );
    elements_marked_for_deletion.push_back( i );
    marked_widgets.push_back( item );
  }

  if( elements_marked_for_deletion.isEmpty() ){
    return;
  }

  for( auto const & row: elements_marked_for_deletion ){
    telegram_accounts_[row].reset();
    telegram_accounts_.removeAt( row );
    logins_.removeAt( row );
  }

  for( auto& marked_item: marked_widgets ){
    ui->flayout->removeItem( marked_item );
    auto account_widget = qobject_cast<AccountWidget*>( marked_item->widget() );
    delete account_widget;
  }

  SaveLoginInfoToDisk();
}

void MainDialog::SaveLoginInfoToDisk()
{
  if( logins_.isEmpty() ) return;
  QFile out_file{ startup_filename };
  if(!out_file.open( QIODevice::WriteOnly ) ){
    QMessageBox::information( this, tr("Error"), tr("Unable to save login information"));
    return;
  }
  QTextStream text_stream{ &out_file };
  for( auto const & info: logins_ )
  {
    text_stream << info->phone_number_ << ":" << info->encryption_key_ << "\n";
  }
  out_file.close();
}

void MainDialog::OnCreateUserButtonClicked()
{
  {
    RegistrationDialog registration_dialog{ this };
    if( registration_dialog.exec() != QDialog::Accepted ){
      return;
    }
    auto const phone_number{ registration_dialog.PhoneNumber() };
    auto const encryption_key{ registration_dialog.EncryptionKey() };
    auto login_info = LoginInformation{ false, phone_number, encryption_key };
    auto login_info_ptr = std::make_shared<LoginInformation>( std::move( login_info ) );
    logins_.push_back( login_info_ptr );
    auto new_account = std::make_shared<Account>( telegram_accounts_.size(), login_info_ptr, this );
    telegram_accounts_.push_back( new_account );
    AddLoginInformation( logins_.back()->phone_number_ );
  }
  SaveLoginInfoToDisk();
  if( ui->select_all_checkbox->isChecked() ){
    ui->select_all_checkbox->setChecked( false );
  } else {
    for( int i = 0; i < logins_.size() - 1; ++i ){
      auto widget = qobject_cast<AccountWidget*>( ui->flayout->itemAt( i )->widget() );
      if( widget->IsSelected() && !logins_[i]->is_logged_in_ ){
        widget->SetChecked( false );
      }
    }
  }

  auto widget = qobject_cast<AccountWidget*>( ui->flayout->itemAt( logins_.size() - 1 )->widget() );
  widget->SetChecked( true );
  LogUserIn();
}

void MainDialog::AuthorizationCodeNeeded( int const index )
{
  auto const code = QInputDialog::getText( this, tr("Request"),
                                           tr( "Enter authorization code for %1" )
                                           .arg( logins_[index]->phone_number_ ),
                                           QLineEdit::Password );
  telegram_accounts_[index]->SendAuthorizationRequest( td_api::make_object<td_api::
                                                       checkAuthenticationCode>(code.toStdString()));
}

void MainDialog::AuthorizationPasswordNeeded( int const index )
{
  auto const password{
    QInputDialog::getText( this, tr("Password"),
                           tr( "Enter authorization password for %1" )
                           .arg( logins_[index]->phone_number_ ), QLineEdit::Password )
  };
  telegram_accounts_[index]->SendAuthorizationRequest( td_api::make_object<td_api::
                                                       checkAuthenticationPassword>
                                                       ( password.toStdString() ) );
}

void MainDialog::NewChannelObtained( int const index, channel_pair_t const & channel_info )
{
  auto item = ui->flayout->itemAt( index );
  auto account_widget = qobject_cast<AccountWidget*>( item->widget() );
  account_widget->AddGroupName( channel_info );
}

void MainDialog::HandshakeCompleted( int const index )
{
  if( logins_[index]->is_logged_in_ ){
    auto item = ui->flayout->itemAt( index );
    auto account_widget = qobject_cast<AccountWidget*>( item->widget() );
    account_widget->SetStatus( user_status_e::online );
  } else {
    QMessageBox::information( this, tr("Login"), tr("Please manually login account"));
  }
  CheckIfLoginCompleted();
}

void MainDialog::CheckIfLoginCompleted()
{
  ++requests_responded_to_;
  if( requests_responded_to_ == proposed_requests_ ){
    ui->search_button->setEnabled( true );
    if( !background_search_scheduled_ ){
      ui->schedule_start_button->setEnabled( true );
    }
    QMessageBox::information( this, tr("Done"), tr("All accounts with green button are logged in"));
  }
}

void MainDialog::ShowError( int const index, QString const & message )
{
  QMessageBox::critical( this, tr("Error"), message );
  auto item = ui->flayout->itemAt( index );
  auto account_widget = qobject_cast<AccountWidget*>( item->widget() );
  account_widget->SetChecked( false );
  telegram_accounts_[index].reset( new Account( index, logins_[index], this ) );
  CheckIfLoginCompleted();
}

void MainDialog::OnLogoutButtonClicked()
{
  for( int i = 0; i != logins_.size(); ++i )
  {
    auto& user_account{ logins_[i] };
    auto account_widget = qobject_cast<AccountWidget*>( ui->flayout->itemAt( i )->widget() );
    if( !account_widget->IsSelected() ){
      continue;
    }
    user_account->is_logged_in_ = false;
    telegram_accounts_[i].reset( new Account( i, logins_[i], this ) );
    account_widget->SetStatus( user_status_e::offline );
    account_widget->HideGroup();
  }
}

void MainDialog::LogUserIn()
{
  proposed_requests_ = 0;
  requests_responded_to_ = 0;

  for( int i = 0; i != logins_.size(); ++i ){
    auto& user_acct{ logins_[i] };
    auto account_widget = qobject_cast<AccountWidget*>( ui->flayout->itemAt( i )->widget() );
    if( user_acct->is_logged_in_ || !account_widget->IsSelected() ){
      continue;
    }
    auto account_ptr = telegram_accounts_[i].get();
    QObject::connect(account_ptr, &Account::requested_authorization_code, [=]( int const index )
    {
      QMetaObject::invokeMethod( this, "AuthorizationCodeNeeded", Qt::BlockingQueuedConnection,
                                 Q_ARG( int, index ) );
    });
    QObject::connect( account_ptr, &Account::new_channel_obtained,
                      [this, i]( channel_pair_t const & channel_info )
    {
      QMetaObject::invokeMethod( this, "NewChannelObtained", Qt::QueuedConnection,
                                 Q_ARG( int const, i ),
                                 Q_ARG( channel_pair_t const&, channel_info ) );
    });
    QObject::connect( account_ptr, &Account::requested_authorization_password,
                      [=]( int const index )
    {
      QMetaObject::invokeMethod( this, "AuthorizationPasswordNeeded", Qt::BlockingQueuedConnection,
                                 Q_ARG( int const, index ) );
    });
    QObject::connect( account_ptr, &Account::handshake_completed, [=]( int row )
    {
      QMetaObject::invokeMethod( this, "HandshakeCompleted", Qt::BlockingQueuedConnection,
                                 Q_ARG( int const, row ) );
    });
    ++proposed_requests_;
    telegram_accounts_[i]->InitiateLoginSequence();
  }
}

void MainDialog::AddLoginInformation( QString const & info )
{
  AccountWidget* new_account_widget = new AccountWidget( info, this );
  QObject::connect( new_account_widget, &AccountWidget::is_selected,
                    [this]( bool const selected )
  {
    if( selected ){
      ++selected_accounts_;
    } else {
      --selected_accounts_;
    }
    if( selected_accounts_ > 0 && ui->flayout->count() > 0 ){
      ui->login_user_button->setVisible( true );
      ui->logout_button->setVisible( true );
    }
  });

  ui->flayout->insertRow( ui->flayout->count(), new_account_widget );
  if( !ui->login_user_button->isEnabled() ){
    ui->login_user_button->setEnabled( true );
  }
  if( !ui->remove_user_button->isEnabled() ){
    ui->remove_user_button->setEnabled( true );
  }
}
