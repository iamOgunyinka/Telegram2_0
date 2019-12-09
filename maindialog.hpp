#ifndef MAINDIALOG_HPP
#define MAINDIALOG_HPP

#include <QDialog>
#include <QList>
#include <QMetaType>
#include <QStandardItemModel>
#include "account.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class MainDialog; }
QT_END_NAMESPACE


enum class SearchResultType
{
    Successful = 0xA,
    NoResult,
    ServerError
};

Q_DECLARE_METATYPE( std::shared_ptr<ObjectPtr> );
Q_DECLARE_METATYPE( SearchResultType );

class MainDialog : public QDialog
{
    Q_OBJECT
    void LoadStartupFile();
    bool RemoveDir( QString const & );
    void FlushLoginInfo();
    void DisableAllButtons();
    void OnCreateUserButtonClicked();
    void OnLoginButtonClicked();
    void OnLogoutButtonClicked();
    void OnRemoveAccountButtonClicked();
    void CheckIfLoginCompleted();
    void OnSearchButtonClicked();
    void OnStartScheduledSearchClicked();
    void OnStopScheduledSearchClicked();
    void OnUserItemSelected(QStandardItem*);
    void OnCustomMenuRequested( QPoint const & );
    void AddLoginInformation( QString const &info );
    void StartExport();
    void ExportSearchResult( QString const & dir_name, int index, SearchResultList const & );
    static QString const startup_filename;
public:
    MainDialog(QWidget *parent = nullptr);
    ~MainDialog() override;
signals:
    void search_done( SearchResultType, int );
public slots:
    void AuthorizationCodeNeeded( int );
    void AuthorizationPasswordNeeded( int );
    void HandshakeCompleted( int );
    void ShowError( int index, QString const & message );
    void OnAccountSearchDone( SearchResultType, int );
    void OnSearchResultObtained( int, std::string const &, std::shared_ptr<ObjectPtr> );
private:
    QString const select_all{ "Select all" };
    bool background_search_scheduled_{false};
    Ui::MainDialog *ui;
    std::string search_text_{};
    int proposed_requests_{};
    int requests_responded_to_{};
    std::unique_ptr<QTimer> bg_search_elapsed_timer_{ nullptr };
    unsigned long long bg_elapsed_timer_{};

    QStandardItemModel accounts_model_{};
    QList<LoginInformation> logins_{};
    QList<std::shared_ptr<Account>> telegram_accounts_;
};
#endif // MAINDIALOG_HPP
