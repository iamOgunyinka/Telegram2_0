#ifndef ACCOUNT_HPP
#define ACCOUNT_HPP

#include <QThread>
#include <QString>
#include <QObject>
#include <QTimer>
#include <td/telegram/Client.h>
#include <memory>
#include <functional>
#include <map>
#include "backgroundworker.hpp"

struct LoginInformation
{
    bool is_logged_in_;
    QString phone_number_;
    QString encryption_key_;
};


struct SearchResult
{
    QString chat_name;
    QString sender;
    QString text;
    std::int64_t message_id;
    std::int64_t chat_id;
    std::int32_t date_id;
};

enum class Length
{
    MaxLength = 0x40
};


using SearchResultList = std::vector<SearchResult>;

class Account: public QObject
{
    Q_OBJECT
public:
    decltype (BackgroundWorker::users_)& GetUsers();
    SearchResultList& GetSearchResult();
    std::map<std::int64_t, std::string>& ChatTitles();
    unsigned long long bg_messages_extracted_ {};
    Account( int index, LoginInformation &, QObject* parent );
    ~Account() override;
    void InitiateLoginSequence();
    void SendAuthorizationRequest( FunctionPtr request );
    void SendRequest( FunctionPtr request, CustomRequestHandler handler );
    void StartBackgroundSearch( std::string const & text, int timeout, CustomRequestHandler&& );
    void StopBackgroundSearch();
    void LogOut();
signals:
    void requested_authorization_code( int );
    void requested_authorization_password( int );
    void handshake_completed( int );
private:
    void RequestForChats();
    void Cleanup();
    void CheckAuthenticationError( ObjectPtr object );
    unsigned long long NextID();
    CustomRequestHandler CreateAuthenticationHandler();
private:
    std::shared_ptr<td::Client> client_{ nullptr };
    LoginInformation& login_info_;
    std::unique_ptr<QTimer> bg_search_timer_ptr_{ nullptr };

    unsigned long long current_request_id {};
    unsigned long long authentication_query_id_{};
    int const index_;

    bool thread_is_running{ true };
    std::map<unsigned long long, CustomRequestHandler> request_handlers_{};
    BackgroundWorker* background_worker_{ nullptr };
    std::shared_ptr<QThread> background_thread_{ nullptr };
    SearchResultList bg_search_results_; // search results done in the background
};

#endif // ACCOUNT_HPP
