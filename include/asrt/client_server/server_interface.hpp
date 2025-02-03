#ifndef DAFF1A70_A3D3_47D1_8C77_5FE546E42051
#define DAFF1A70_A3D3_47D1_8C77_5FE546E42051

#include <memory>
#include <vector>
#include <algorithm>
#include <thread>

#include "asrt/util.hpp"
#include "asrt/error_code.hpp"
#include "asrt/client_server/common_types.hpp"
#include "asrt/client_server/message_queue.hpp"
#include "asrt/client_server/connection.hpp"

namespace ClientServer{

template<typename Executor,
          typename Protocol,
          typename Message>
class ServerInterface
{
public:

    using Endpoint              = typename Protocol::Endpoint;
    using Acceptor              = typename Protocol::template AcceptorType<Executor>;
    using ConnectionToClient    = Connection<Executor, Protocol, Message, ServerInterface>;
    using Client                = std::shared_ptr<ConnectionToClient>;
    using Inbox                 = typename ConnectionToClient::Inbox;
    using ClientId              = typename ConnectionToClient::ConnectionIdType;
    using ConstMessageView      = typename ConnectionToClient::ConstMessageView;

    explicit ServerInterface(const Endpoint& endpoint, 
                             ProcessingMode mode = ProcessingMode::kEvent) noexcept;

    ServerInterface(ServerInterface const&) = delete;
    ServerInterface(ServerInterface&&) = delete;
    ServerInterface &operator=(ServerInterface const &other) = delete;
    ServerInterface &operator=(ServerInterface &&other) = delete;
    virtual ~ServerInterface() noexcept = default;

    static constexpr auto Identity() {return Identity::kServer;}

    /**
     * @brief Starts the server and executes the event loop
     * 
     * @details Blocks current thread until stopped by calling Stop()
     */
    void Run() noexcept;
    
    /**
     * @brief Stops all processing of events. Causes Run() to exit
     * 
     */
    void Stop() noexcept;

    void MessageClientById(ClientId client_id, ConstMessageView message) noexcept; //todo needs to return result

    void MessageClient(Client& client, ConstMessageView message) noexcept; //todo send sync so no need to increment ref count

    void MessageAllClients(ConstMessageView message) noexcept;

    void Process(std::size_t max_messages) noexcept;

    auto RetrieveInbox() noexcept -> Inbox*;

    /**
     * @brief Callback called when connection encounters a i/o error
     * 
     * @param client the connection object by reference (since the callback resets the connections)
     * @param ec the errorcode associated with the i/o error
     */
    void OnConnectionError(Client client, ErrorCode_Ns::ErrorCode ec) noexcept;


    /**
     * @brief Callback called when connection encounters a i/o error
     * 
     * @param client the connection object by reference (since the callback resets the connections)
     * @param ec the errorcode associated with the i/o error
     */
    void OnValidationError(Client& client, ErrorCode_Ns::ErrorCode ec);

    virtual bool OnClientConnect(Client& client) noexcept {return true;}

    virtual void OnClientValidated(Client& client) noexcept {};

    /**
     * @brief Customization point for server implementation to handle client disconnect
     * 
     * @param client shared pointer to this connection
     */
    virtual void OnClientDisconnect(Client& client) noexcept {}

    virtual void OnMessage(Client client, ConstMessageView message) noexcept {}

protected:

    auto& GetExecutor() noexcept{return this->executor_;}

private:
    using ConnectionId = typename ConnectionToClient::ConnectionIdType;

    auto GetClientPositionInQueue(ClientId client_id) noexcept;

    void DoMessageClient(Client& client, ConstMessageView message) noexcept;

    void WaitForClientConnections() noexcept;

    void RemoveConnection(ConnectionId conn_id) noexcept;

    using ActiveConnections = std::vector<Client>;
    template <typename T> using Optional = Util::Optional_NS::Optional<T>;

    using MutexType = typename Executor::MutexType;

    Executor executor_;
    Acceptor acceptor_;
    Optional<Inbox> inbox_{};
    MutexType connection_storage_mutex_;
    ActiveConnections connections_;
    std::size_t next_client_id_{0};
};

}

#if defined(ASRT_HEADER_ONLY)
# include "asrt/impl/server_interface.ipp"
#endif // defined(ASRT_HEADER_ONLY)

#endif /* DAFF1A70_A3D3_47D1_8C77_5FE546E42051 */
