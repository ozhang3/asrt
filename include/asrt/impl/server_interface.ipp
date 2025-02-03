#include "asrt/client_server/server_interface.hpp"

namespace ClientServer{

template<typename Executor, typename Protocol, typename Message>
inline ServerInterface<Executor, Protocol, Message>::
ServerInterface(const Endpoint& endpoint, ProcessingMode mode) noexcept
    : executor_{}, acceptor_{executor_, endpoint} 
{
    if(mode == ProcessingMode::kPolling)
        this->inbox_.emplace();
}

template<typename Executor, typename Protocol, typename Message>
inline void ServerInterface<Executor, Protocol, Message>::
Run() noexcept
{
    spdlog::info("Server started listening on {}", 
        this->acceptor_.GetLocalEndpoint());
    this->WaitForClientConnections();
    this->executor_.Run();
}

template<typename Executor, typename Protocol, typename Message>
inline void ServerInterface<Executor, Protocol, Message>::
Stop() noexcept
{
    this->executor_.Stop();
}

template<typename Executor, typename Protocol, typename Message>
inline void ServerInterface<Executor, Protocol, Message>::
DoMessageClient(Client& client, ConstMessageView message) noexcept
{
    if(client && client->IsConnected()){
        client->SendSync(message); //todo sendsync or plain send ?
    }else{
        ASRT_LOG_TRACE("Server interface: detected disconnection when messaging client {}",
            client->GetId());
        /* notify server of disconnect event */
        this->OnClientDisconnect(client);
        //this->RemoveConnection(client->GetId()); //todo mark this conenction for later removal
        //todo attempting to remove it here leads to deadlock as mutex is possibly acquired earlier
    }
}

template<typename Executor, typename Protocol, typename Message>
inline void ServerInterface<Executor, Protocol, Message>::
MessageClientById(ClientId client_id, ConstMessageView message) noexcept
{
    std::scoped_lock const lock{this->connection_storage_mutex_};
    const auto client_it{this->GetClientPositionInQueue(client_id)};
    if(client_it != this->connections_.end()){
        this->DoMessageClient(*client_it, message);
    }else [[unlikely]] {
        //todo maybe return this error to application
        spdlog::error("Unable to find client to send message to. Client id {}",
            client_id);
    }
}

template<typename Executor, typename Protocol, typename Message>
inline void ServerInterface<Executor, Protocol, Message>::
MessageClient(Client& client, ConstMessageView message) noexcept
{
    this->DoMessageClient(client, message);
}

template<typename Executor, typename Protocol, typename Message>
inline void ServerInterface<Executor, Protocol, Message>::
MessageAllClients(ConstMessageView message) noexcept
{
    std::scoped_lock const lock{this->connection_storage_mutex_};
    std::ranges::for_each(this->connections_,
        [this, message](auto& conn){
            this->DoMessageClient(conn, message);
        });
}

template<typename Executor, typename Protocol, typename Message>
inline void ServerInterface<Executor, Protocol, Message>::
Process(std::size_t max_messages) noexcept
{
    assert(this->inbox_.has_value()); //polling mode
    std::size_t msg_processed{};
    while(!this->inbox_.is_empty() && msg_processed < max_messages){
        auto msg{this->inbox_.Pop()};
        this->OnMessage(msg.source_, std::move(msg.msg_));
    }
}

template<typename Executor, typename Protocol, typename Message>
inline auto ServerInterface<Executor, Protocol, Message>::
RetrieveInbox() noexcept -> Inbox*
{
    //assert(this->inbox_.has_value()); //polling mode
    return this->inbox_.has_value() ?
        &(this->inbox_.value()) : nullptr;
}

template<typename Executor, typename Protocol, typename Message>
inline void ServerInterface<Executor, Protocol, Message>::
WaitForClientConnections() noexcept
{
    std::shared_ptr<ConnectionToClient> new_connection{
        ConnectionToClient::Create(this->executor_, *this, next_client_id_++)};

    this->acceptor_.AcceptAsync(new_connection->GetSocket(),
        [this, new_connection](auto accept_result) mutable {
            if(accept_result.has_value()) [[likely]] {

                if constexpr (ProtocolTraits::is_internet_domain<Protocol>::value){
                    ASRT_LOG_INFO("Client connect request from {}",    
                        new_connection->GetSocket().GetRemoteEndpoint());
                }else{
                    ASRT_LOG_INFO("Client connect request from {}",    
                        new_connection->GetSocket().GetPeerCredentials());                    
                }

                if(OnClientConnect(new_connection)){
                    ASRT_LOG_INFO("Accepted new client, assined id {}", new_connection->GetId());
                    new_connection->InitiateHandshake();
                    {
                        std::scoped_lock const lock{this->connection_storage_mutex_};
                        this->connections_.emplace_back(std::move(new_connection));
                    }
                }else{
                    ASRT_LOG_WARN("Denied client connection.");
                }
            }else [[unlikely]] {
                ASRT_LOG_ERROR("Failed to accept client, {}",
                    accept_result.error());
            }
            this->WaitForClientConnections();
        });
}

template<typename Executor, typename Protocol, typename Message>
inline void ServerInterface<Executor, Protocol, Message>::
OnConnectionError(Client client, ErrorCode_Ns::ErrorCode ec) noexcept
{
    //todo is there gurantee that the client connection stays alive
    // todo for the duration of this function?
    //todo for now this callback is only executed in connection handler context
    //todo which means connection is guranteed to be alive

    using enum ErrorCode_Ns::ErrorCode;

    ClientId const client_id{client->GetId()};

    if(ErrorCode_Ns::IsConnectionDown(ec)){
        //const auto peer{client->GetSocket().GetRemoteEndpoint()};
        ASRT_LOG_INFO("Client {} disconnected", client_id);
        this->OnClientDisconnect(client);
    }

    else if (ec == connection_authentication_failed) {
        ASRT_LOG_INFO("Client failed authentication, dropping connection.");
    }

    this->RemoveConnection(client_id);
}

template<typename Executor, typename Protocol, typename Message>
inline void ServerInterface<Executor, Protocol, Message>::
RemoveConnection(ConnectionId conn_id) noexcept
{
    std::scoped_lock const lock{this->connection_storage_mutex_};
    ASRT_LOG_TRACE("Connection size {} before removal", 
        connections_.size());

    Util::QuickRemoveOneIf(this->connections_, 
        [conn_id](const auto& conn){
            return conn->GetId() == conn_id;
        });

    ASRT_LOG_DEBUG("Removed connection {}", conn_id);
    ASRT_LOG_TRACE("Connection size {} after removal", 
        connections_.size());
}

//! not thread safe. protect with mutex before calling
template<typename Executor, typename Protocol, typename Message>
inline auto ServerInterface<Executor, Protocol, Message>::
GetClientPositionInQueue(ClientId client_id) noexcept
{
    return std::ranges::find_if(this->connections_,
        [client_id](const auto& conn){
            return conn->GetId() == client_id;
        });
}
}//end ns