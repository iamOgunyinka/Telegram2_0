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
#include <xlsxdocument.h>
#include "registrationdialog.hpp"
#include "schedulerdialog.hpp"

QString const MainDialog::startup_filename{ "config.ini" };

MainDialog::MainDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::MainDialog)
{
    ui->setupUi( this );
    QObject::connect( ui->new_user_button, &QPushButton::clicked, this,
                      &MainDialog::OnCreateUserButtonClicked );
    QObject::connect( &accounts_model_, &QStandardItemModel::itemChanged, this,
                      &MainDialog::OnUserItemSelected );
    QObject::connect( ui->login_user_button, &QPushButton::clicked, this,
                      &MainDialog::OnLoginButtonClicked );
    QObject::connect( ui->remove_user_button, &QPushButton::clicked, this,
                      &MainDialog::OnRemoveAccountButtonClicked );
    QObject::connect( ui->schedule_start_button, &QPushButton::clicked, this,
                      &MainDialog::OnStartScheduledSearchClicked );
    QObject::connect( ui->search_button, &QPushButton::clicked, this,
                      &MainDialog::OnSearchButtonClicked );
    QObject::connect( this, &MainDialog::search_done, [=]( int const & index ){
        QMetaObject::invokeMethod(this, "OnAccountSearchDone", Qt::QueuedConnection,
                                  Q_ARG( int, index ));
    });
    ui->user_list_view->setContextMenuPolicy( Qt::CustomContextMenu );
    QObject::connect( ui->user_list_view, &QListView::customContextMenuRequested, this,
                      &MainDialog::OnCustomMenuRequested );
    QObject::connect( ui->schedule_stop_button, &QPushButton::clicked, this,
                      &MainDialog::OnStopScheduledSearchClicked );
    accounts_model_.setColumnCount( 1 );
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

void MainDialog::OnCustomMenuRequested( QPoint const &p )
{
    QModelIndex const index{ ui->user_list_view->indexAt( p )};
    if( !index.isValid() ) return;
    QMenu custom_menu( this );
    custom_menu.setWindowTitle( "Menu" );
    if( !index.parent().isValid() ){
        QAction* action_remove{ new QAction( "Remove" ) };
        QAction* action_login{ new QAction( "Login" ) };
        QAction* action_logout{ new QAction( "Logout" ) };

        QObject::connect( action_login, &QAction::triggered, this,
                          &MainDialog::OnLoginButtonClicked );
        //        QObject::connect( action_logout, &QAction::triggered, this,
        //                          &MainDialog::OnLogoutButtonClicked );
        QObject::connect( action_remove, &QAction::triggered, this,
                          &MainDialog::OnRemoveAccountButtonClicked );

        custom_menu.addAction( action_login );
        custom_menu.addAction( action_logout );
        custom_menu.addAction( action_remove );
    }
    custom_menu.exec( ui->user_list_view->mapToGlobal( p ) );
}

void MainDialog::OnAccountSearchDone( int const index )
{
    auto const & result{ telegram_accounts_[index]->GetSearchResult() };
    if( proposed_requests_ > 1 ){
        ui->textEdit->append( tr( "%1 items found for %2" ).arg( result.size() )
                              .arg( logins_[index].phone_number_ ) );
    } else {
        ui->textEdit->clear();
        for( auto const & message: result ){
            ui->textEdit->append( tr( "%1(%2): %3" ).arg( message.sender ).arg( message.chat_name )
                                  .arg( message.text ) );
        }
    }
}

void MainDialog::LoadStartupFile()
{
    if( !QFileInfo::exists( startup_filename ) ) return;
    QFile file{ startup_filename };
    if( !file.open( QIODevice::ReadOnly ) ) return;
    QTextStream text_stream{ &file };
    QString line{};
    telegram_accounts_.clear();
    int index = 0;
    while( text_stream.readLineInto( &line ) ){
        QStringList splits{ line.split( ":" ) };
        if( splits.size() < 2 ) continue;
        logins_.push_back( LoginInformation{ false, splits[0], splits[1] } );
        telegram_accounts_.push_back( std::make_shared<Account>( ++index, logins_.back(), this ) );
    }
    if( !logins_.empty() ){
        telegram_accounts_.push_front( nullptr );
        AddLoginInformation( select_all );
        for( auto const & info: logins_ ) AddLoginInformation( info.phone_number_ );
        logins_.prepend( LoginInformation{ false, "", "" } );
    }
}

bool MainDialog::RemoveDir( QString const & dir_name )
{
    bool result = true;
    QDir dir(dir_name);

    if (dir.exists()) {
        Q_FOREACH(QFileInfo info, dir.entryInfoList( QDir::NoDotAndDotDot |
                                                     QDir::System | QDir::Hidden
                                                     | QDir::AllDirs | QDir::Files,
                                                     QDir::DirsFirst))
        {
            if (info.isDir()) result = RemoveDir(info.absoluteFilePath());
            else result = QFile::remove(info.absoluteFilePath());

            if (!result) return result;
        }
        result = QDir().rmdir(dir_name);
    }
    return result;
}

void MainDialog::OnSearchButtonClicked()
{
    proposed_requests_ = requests_responded_to_ = 0;
    if( background_search_scheduled_ ){
        QMessageBox::critical( this, "Task", "You have a background search ongoing, suspend "
                                                "it first" );
        return;
    }
    fg_search_text_ = ui->query_line->text().trimmed().toStdString();
    for( int i = 1; i != accounts_model_.rowCount(); ++i ){
        QStandardItem* item { accounts_model_.item( i ) };
        if( item->checkState() == Qt::Unchecked || !logins_[i].is_logged_in_ ) continue;
        telegram_accounts_[i]->SendRequest( td_api::make_object<td_api::searchMessages>(
                                                fg_search_text_, 0, 0, 0, MaxResultAllowed ),
                                            [=]( ObjectPtr result )
        {
            OnSearchResultObtained( i, fg_search_text_,
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
    bg_search_text_ = scheduler_dialog.GetText().toStdString();

    bg_elapsed_timer_ = requests_responded_to_ = proposed_requests_ = 0;
    int const timeout{ scheduler_dialog.GetTime() };
    for( int i = 1; i != accounts_model_.rowCount(); ++i ){
        QStandardItem* item { accounts_model_.item( i ) };
        if( item->checkState() == Qt::Unchecked || !logins_[i].is_logged_in_ ) continue;
        telegram_accounts_[i]->StartBackgroundSearch( bg_search_text_, timeout, [=]( ObjectPtr ptr )
        {
            OnSearchResultObtained( i, bg_search_text_,
                                    std::make_shared<ObjectPtr>( std::move( ptr ) ) );
        });
        ++proposed_requests_;
    }
    if( proposed_requests_ == 0 ){
        QMessageBox::information( this, "Search", "Choose an account(online) to conduct a search "
                                                  "on" );
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
    ui->textEdit->clear();
    ui->timer_display_button->setText( tr( "Interval: %1s" ).arg( timeout ) );
    bg_search_elapsed_timer_->start( ThousandMilliseconds );
}

void MainDialog::OnSearchResultObtained( int const index, std::string const & text,
                                         std::shared_ptr<ObjectPtr> ptr )
{
    if( (*ptr)->get_id() == td_api::error::ID ) return;
    auto messages_ptr = td::move_tl_object_as<td_api::messages>( *ptr );
    auto& messages = messages_ptr->messages_;
    auto& messages_extracted{ telegram_accounts_[index]->bg_messages_extracted_ };
    if( ( messages_extracted >= messages_ptr->total_count_ ) || messages.empty() ) return;
    auto& users { telegram_accounts_[index]->GetUsers() };
    auto& search_results{ telegram_accounts_[index]->GetSearchResult() };
    auto& chat_titles{ telegram_accounts_[index]->ChatTitles() };
    for( std::size_t i = 0; i != messages.size(); ++i ){
        auto& message = messages[i];
        if ( message->content_->get_id() == td_api::messageText::ID ) {
            auto content = td::move_tl_object_as<td_api::messageText>(message->content_);
            auto& sender = users[message->sender_user_id_];

            QString sender_first_name = QString::fromStdString( sender->first_name_ );
            QString text_sent = QString::fromStdString( content->text_->text_ );
            QString chat_name = QString::fromStdString( chat_titles[message->chat_id_] );
            std::int64_t chat_id = message->chat_id_;
            std::int64_t message_id = message->id_;
            std::int32_t date_id = message->date_;
            search_results.emplace_back( SearchResult{ chat_name, sender_first_name, text_sent,
                                                       message_id, chat_id, date_id } );
        }
        if( ++messages_extracted >= messages_ptr->total_count_ ){
            emit search_done( index );
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

void MainDialog::OnStopScheduledSearchClicked()
{
    ui->schedule_stop_button->setEnabled( false );
    bg_search_elapsed_timer_->stop();
    bg_search_elapsed_timer_.reset();
    background_search_scheduled_ = false;
    ui->textEdit->append( "Background search stopped" );
    ui->schedule_start_button->setDisabled( false );
    QString const directory{ QFileDialog::getExistingDirectory( this, "Directory" ) };
    for( int i = 1; i != logins_.size(); ++i ){
        if( !telegram_accounts_[i] || !logins_[i].is_logged_in_ ||
            accounts_model_.item( i )->checkState() == Qt::Unchecked ) continue;
        telegram_accounts_[i]->StopBackgroundSearch();
        if( directory.isEmpty() || directory.isNull() ) continue;
        auto& search_result{ telegram_accounts_[i]->GetSearchResult() };
        if( !search_result.empty() ) {
            ExportSearchResult( directory, i, search_result );
        }
    }
    if( !directory.isEmpty() ){
        QMessageBox::information( this, "Status", "File(s) saved successfully" );
    }
}

void MainDialog::ExportSearchResult( QString const &dir_name, int const index,
                                     SearchResultList const &search_result )
{
    QString const filename{ dir_name + "/" + logins_[index].phone_number_ + ".xlsx" };
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
    sheet->write( "E1", tr( "Sender" ) );
    sheet->write( "G1", tr( "Text" ) );
    sheet->write( "I1", tr( "Phone number" ) );
    sheet->write( "J1", logins_[index].phone_number_ );
    sheet->write( "L1", "Keyword" );
    sheet->write( "M1", QString::fromStdString( bg_search_text_ ) );

    std::size_t counter{ 3 };
    QSet<QString> unique_elements{};

    for( auto const & item: search_result ){
        if( unique_elements.contains( item.text ) ) continue;
        unique_elements.insert( item.text );
        sheet->write( tr( "A%1" ).arg( counter ), counter - 2 );
        sheet->write( tr( "C%1" ).arg( counter ), item.chat_name );
        sheet->write( tr( "E%1" ).arg( counter ), item.sender );
        sheet->write( tr( "G%1" ).arg( counter ), item.text );
        ++counter;
    }
    doc.save();
}

void MainDialog::OnRemoveAccountButtonClicked()
{
    QVector<int> elements_marked_for_deletion{};
    for( int i = 1; i != accounts_model_.rowCount(); ++i ){
        auto item = accounts_model_.item(i);
        if( item->checkState() == Qt::Unchecked || logins_[i].is_logged_in_ ) continue;
        RemoveDir( item->text() );
        elements_marked_for_deletion.push_back( i );
    }
    if( !elements_marked_for_deletion.isEmpty() &&
        QMessageBox::question( this, "Confirmation", "Are you sure you want to delete these "
                               "accounts?" ) == QMessageBox::No ) return;

    for( auto const & row: elements_marked_for_deletion ){
        delete accounts_model_.takeItem( row );
        accounts_model_.removeRow( row );
        telegram_accounts_[row].reset();
        telegram_accounts_.removeAt( row );
        logins_.removeAt( row );
    }
    ui->login_user_button->setEnabled( false );
    ui->remove_user_button->setEnabled( false );
    ui->user_list_view->setModel( &accounts_model_ );
    FlushLoginInfo();
}

void MainDialog::FlushLoginInfo()
{
    if( logins_.isEmpty() ) return;
    QFile out_file{ startup_filename };
    if(!out_file.open( QIODevice::WriteOnly ) ){
        QMessageBox::information( this, "Error", "Unable to save login information" );
        return;
    }
    QTextStream text_stream{ &out_file };
    for( int index = 1; index != logins_.size(); ++index )
    {
        auto const &info{ logins_[index] };
        text_stream << info.phone_number_ << ":" << info.encryption_key_ << "\n";
    }
    out_file.close();
}

void MainDialog::OnUserItemSelected( QStandardItem* item )
{
    auto const state = item->checkState();
    if( item->text() == select_all ){
        for( int i = 1; i != accounts_model_.rowCount(); ++i ){
            accounts_model_.item( i )->setCheckState( state );
        }
        return;
    }
    auto logged_out{ !logins_[accounts_model_.indexFromItem( item ).row()].is_logged_in_ };
    if( state == Qt::Checked && logged_out ){
        ui->login_user_button->setEnabled( true );
        ui->remove_user_button->setEnabled( true );
        return;
    }
    for( int row = 1; row != accounts_model_.rowCount(); ++row ){
        auto logged_in{ logins_[row].is_logged_in_ };
        if( accounts_model_.item( row )->checkState() == Qt::Checked && !logged_in ) return;
    }
    ui->remove_user_button->setDisabled( true );
    ui->login_user_button->setDisabled( true );
}

void MainDialog::OnCreateUserButtonClicked()
{
    RegistrationDialog* registration_dialog{ new RegistrationDialog( this ) };
    if( registration_dialog->exec() == QDialog::Accepted ){
        QString phone_number{ registration_dialog->PhoneNumber() };
        QString encryption_key{ registration_dialog->EncryptionKey() };
        logins_.push_back( LoginInformation{ false, phone_number, encryption_key } );
        telegram_accounts_.push_back( std::make_shared<Account>( telegram_accounts_.size(),
                                                                 logins_.back(), this ));
        AddLoginInformation( logins_.back().phone_number_ );
    }
    FlushLoginInfo();
}

void MainDialog::AuthorizationCodeNeeded( int const index )
{
    auto code = QInputDialog::getText( this, "Request", tr( "Enter authorization code for "
                                                            "%1" ).arg( logins_[index]
                                                                        .phone_number_ ),
                                       QLineEdit::Password ).toStdString();
    telegram_accounts_[index]->SendAuthorizationRequest( td_api::make_object<td_api::
                                                         checkAuthenticationCode>(code) );
}

void MainDialog::AuthorizationPasswordNeeded( int const index )
{
    auto password{ QInputDialog::getText( this, "Password", tr( "Enter authorization "
                                                                "password for %1" ).arg(
                                              logins_[index].phone_number_ ), QLineEdit::Password )};
    telegram_accounts_[index]->SendAuthorizationRequest( td_api::make_object<td_api::
                                                         checkAuthenticationPassword>
                                                         ( password.toStdString() ) );
}

void MainDialog::HandshakeCompleted( int const index )
{
    QStandardItem* item { accounts_model_.item( index ) };
    QString const phone_number{ item->text().split( '(' )[0] };
    QStandardItem* new_item{ new QStandardItem( phone_number + "(Online)" ) };
    new_item->setFlags( Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
    new_item->setData( Qt::Checked, Qt::CheckStateRole );
    accounts_model_.setItem( index, new_item );
    CheckIfLoginCompleted();
}

void MainDialog::CheckIfLoginCompleted()
{
    ++requests_responded_to_;
    //ui->user_list_view->setModel( &accounts_model_ );
    if( requests_responded_to_ == proposed_requests_ ){
        ui->search_button->setEnabled( true );
        if( !background_search_scheduled_ ) ui->schedule_start_button->setEnabled( true );
        ui->remove_user_button->setEnabled( false );
        ui->login_user_button->setEnabled( false );
        QMessageBox::information( this, "Done", "All accounts marked `Online` are logged in" );
    }
}

void MainDialog::ShowError( int const index, QString const & message )
{
    QMessageBox::critical( this, "Error", message );
    QStandardItem* item{ accounts_model_.item( index ) };
    telegram_accounts_[index].reset( new Account( index, logins_[index], this ) );
    item->setCheckState( Qt::Unchecked );
    CheckIfLoginCompleted();
}

void MainDialog::OnLogoutButtonClicked()
{
    int logged_out_accounts {};
    for( int i = 1; i != logins_.size(); ++i )
    {
        auto& user_account{ logins_[i] };
        auto item{ accounts_model_.item( i ) };
        if( item->checkState() == Qt::Unchecked ){
            if( !user_account.is_logged_in_ ) ++logged_out_accounts;
            continue;
        }
        user_account.is_logged_in_ = false;
        telegram_accounts_[i]->LogOut();
        item->setText( user_account.phone_number_ + "(Offline)" );
        ++logged_out_accounts;
    }
    if( logged_out_accounts == logins_.size() - 1 ){
        DisableAllButtons();
    }
}

void MainDialog::OnLoginButtonClicked()
{
    proposed_requests_ = 0;
    requests_responded_to_ = 0;
    for( int i = 1; i != logins_.size(); ++i ){
        auto& user_acct{ logins_[i] };
        if( user_acct.is_logged_in_ || accounts_model_.item( i )->checkState() == Qt::Unchecked ){
            continue;
        }
        QObject::connect(telegram_accounts_[i].get(), &Account::requested_authorization_code,
                         [=]( int const index )
        {
            QMetaObject::invokeMethod( this, "AuthorizationCodeNeeded", Qt::QueuedConnection,
                                       Q_ARG( int, index ) );
        });
        QObject::connect( telegram_accounts_[i].get(), &Account::requested_authorization_password,
                          [=]( int const index )
        {
            QMetaObject::invokeMethod( this, "AuthorizationPasswordNeeded", Qt::QueuedConnection,
                                       Q_ARG( int const, index ) );
        });
        QObject::connect( telegram_accounts_[i].get(), &Account::handshake_completed, [=]( int row )
        {
            QMetaObject::invokeMethod( this, "HandshakeCompleted", Qt::QueuedConnection,
                                       Q_ARG( int const, row ) );
        });
        ++proposed_requests_;
        telegram_accounts_[i]->InitiateLoginSequence();
    }
}

void MainDialog::AddLoginInformation(const QString& info )
{
    auto number_item = new QStandardItem( tr( "%1%2").arg( info )
                                          .arg( info == select_all ? "": "(Offline)" ));
    number_item->setFlags( Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
    number_item->setData( Qt::Unchecked, Qt::CheckStateRole );
    accounts_model_.appendRow( number_item );
    ui->user_list_view->setModel( &accounts_model_ );
}
