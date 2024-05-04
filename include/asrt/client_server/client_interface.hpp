#ifndef D3463D7C_ED08_433F_A558_331B68FAC66B
#define D3463D7C_ED08_433F_A558_331B68FAC66B

#include "asrt/util.hpp"
#include "asrt/error_code.hpp"
#include "asrt/client_server/common_types.hpp"
#include "asrt/client_server/message_queue.hpp"
#include "asrt/client_server/connection.hpp"

namespace ClientServer{

 template<
        typename Executor,
        typename Protocol,
        typename Message>
class ClientInterface
{
public:

    using Endpoint = typename Protocol::Endpoint;
    using ConnectionToServer = Connection<Executor, Protocol, Message, ClientInterface>;
    using ConstMessageView = typename ConnectionToServer::ConstMessageView;
    using Server = std::shared_ptr<ConnectionToServer>;
    using Inbox = typename ConnectionToServer::Inbox;

    static constexpr auto Identity() {return Identity::kClient;}

    explicit ClientInterface(Executor& executor, ProcessingMode mode = ProcessingMode::kEvent) noexcept;
    ClientInterface(ClientInterface const&) = delete;
    ClientInterface(ClientInterface&&) = delete;
    ClientInterface &operator=(ClientInterface const &other) = delete;
    ClientInterface &operator=(ClientInterface &&other) = delete;
    virtual ~ClientInterface() noexcept;

    bool IsConnected() noexcept;

    /**
     * @brief Starts a connection to the remote server.
     * 
     * @param server 
     */
    void Connect(const Endpoint& server) noexcept;

    /**
     * @brief Kills the current connection
     * 
     */
    void Disconnect() noexcept;

    /**
     * @brief Resets current connection and initiates a new connection to remote server.
     * 
     * @param server 
     */
    void Reconnect(const Endpoint& server) noexcept;
    
    void Send(const Message& message) noexcept;

    void SendSync(ConstMessageView message) noexcept;

    auto RetrieveInbox() noexcept -> Inbox*;

    void OnConnectionError(ErrorCode_Ns::ErrorCode ec) noexcept;

    virtual void OnMessage(ConstMessageView message) noexcept = 0;

    virtual void OnServerDisconnect() noexcept {}

private:
    template <typename T> using Optional = Util::Optional_NS::Optional<T>;

    Optional<Inbox> inbox_;
    Server connection_;
};

}

#if defined(ASRT_HEADER_ONLY)
# include "asrt/client_server/impl/client_interface.ipp"
#endif // defined(ASRT_HEADER_ONLY)

#endif /* D3463D7C_ED08_433F_A558_331B68FAC66B */
