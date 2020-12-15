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

using channel_pair_t = QPair<std::int64_t const, QString>;

Q_DECLARE_METATYPE( std::shared_ptr<ObjectPtr> );
Q_DECLARE_METATYPE( SearchResultType );
Q_DECLARE_METATYPE( channel_pair_t );

class MainDialog : public QDialog
{
    Q_OBJECT
    void LoadStartupFile();
    bool RemoveDir( QString const & );
    void SaveLoginInfoToDisk();
    void DisableAllButtons();
    void OnCreateUserButtonClicked();
    void LogUserIn();
    void OnLogoutButtonClicked();
    void OnRemoveAccountButtonClicked();
    void CheckIfLoginCompleted();
    void OnSearchButtonClicked();
    void OnStartScheduledSearchClicked();
    void OnStopScheduledSearchClicked();
    void AddLoginInformation( QString const &info );
    void StartExport();
    void ExportSearchResult( QString const & dir_name, int index, SearchResultList const & );
    static char const * const startup_filename;
public:
    MainDialog(QWidget *parent = nullptr);
    ~MainDialog() override;
signals:
    void search_done( SearchResultType, int );
public slots:
    void AuthorizationCodeNeeded( int );
    void AuthorizationPasswordNeeded( int );
    void HandshakeCompleted( int );
    void NewChannelObtained( int, channel_pair_t const & channel_info );
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
    int selected_accounts_{};
    std::unique_ptr<QTimer> bg_search_elapsed_timer_{ nullptr };
    unsigned long long bg_elapsed_timer_{};

    QVector<std::shared_ptr<LoginInformation>> logins_{};
    QVector<std::shared_ptr<Account>> telegram_accounts_{};
};
#endif // MAINDIALOG_HPP
