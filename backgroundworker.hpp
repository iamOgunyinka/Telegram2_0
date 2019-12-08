#ifndef BACKGROUNDWORKER_HPP
#define BACKGROUNDWORKER_HPP

#include <QObject>
#include <functional>
#include <td/telegram/Client.h>

namespace td_api = td::td_api;
using ObjectPtr = td_api::object_ptr<td_api::Object>;
using FunctionPtr = td_api::object_ptr<td_api::Function>;
using ResponsePtr = std::shared_ptr<td::Client::Response>;
using CustomRequestHandler = std::function<void(ObjectPtr)>;
using CustomRequestHandlerMap = std::map<unsigned long long, CustomRequestHandler>;

enum Constants
{
    MaxTimeout = 10,
    Mins = 60,
    MaxResultAllowed = 100,
    ThousandMilliseconds = 1000,
    AppID= 1'127'150
};

namespace detail {
    template <class... Fs>
    struct overload;

    template <class F>
    struct overload<F> : public F {
        explicit overload(F f) : F(f) {
        }
    };
    template <class F, class... Fs>
    struct overload<F, Fs...>
            : public overload<F>
            , overload<Fs...> {
        overload(F f, Fs... fs) : overload<F>(f), overload<Fs...>(fs...) {
        }
        using overload<F>::operator();
        using overload<Fs...>::operator();
    };
}  // namespace detail

template <class... F>
auto overloaded(F... f) {
    return detail::overload<F...>(f...);
}

class BackgroundWorker : public QObject
{
    Q_OBJECT
public:
    explicit BackgroundWorker( std::shared_ptr<td::Client>& client, CustomRequestHandlerMap &,
                               bool& is_running, QObject *parent_window,
                               unsigned long long& auth_id,
                               QObject* parent = nullptr );
    ~BackgroundWorker() override;
    void InitiateLoginSequence();
    void SendRequest( unsigned long long query_id, FunctionPtr request,
                      CustomRequestHandler handler );
    void StartPolling();
    std::map<std::int32_t, td_api::object_ptr<td_api::user>> users_;
    std::map<std::int64_t, std::string>& ChatTitles();
    void SetHasError();
signals:
    void handshake_completed( bool has_error );
    void requested_phone_number();
    void requested_authorization_code();
    void requested_authorization_password();
    void requested_app_paramters();
    void requested_encryption_code();
private:
    void Restart();
    void ProcessResponse( ResponsePtr );
    void ProcessUpdate( ObjectPtr );
    void OnAuthorizationStateUpdate();
private:
    std::shared_ptr<td::Client>& client_;
    CustomRequestHandlerMap& request_handlers_;
    bool authorization_granted_{ false };
    bool error_set_{ false };
    bool need_restart_{ false };
    bool request_sent_{ false };
    bool& running_;
    unsigned long long& authentication_query_id_;
    td_api::object_ptr<td_api::AuthorizationState> authorization_state_ptr_;
    std::map<std::int64_t, std::string> chat_title_;

    QObject *parent_window_{ nullptr };
};

#endif // BACKGROUNDWORKER_HPP
