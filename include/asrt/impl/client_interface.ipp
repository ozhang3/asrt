#include "asrt/client_server/client_interface.hpp"

namespace ClientServer{

template <typename Executor, typename Protocol, typename Message>
inline ClientInterface<Executor, Protocol, Message>::
ClientInterface(Executor& executor, ProcessingMode mode) noexcept
{
    if(mode == ProcessingMode::kPolling){
        this->inbox_.emplace();
    }

    this->connection_ = ConnectionToServer::Create(executor, *this);
}

template <typename Executor, typename Protocol, typename Message>
inline ClientInterface<Executor, Protocol, Message>::
~ClientInterface() noexcept
{
    this->connection_->Close();
}

template <typename Executor, typename Protocol, typename Message>
inline bool ClientInterface<Executor, Protocol, Message>::
IsConnected() noexcept
{
    return this->connection_->IsConnected();
}

template <typename Executor, typename Protocol, typename Message>
inline void ClientInterface<Executor, Protocol, Message>::
Connect(const Endpoint& server) noexcept
{
    this->connection_->ConnectToServer(server, 5s); //retries connection every 5s
}

template <typename Executor, typename Protocol, typename Message>
inline void ClientInterface<Executor, Protocol, Message>::
Disconnect() noexcept
{
    this->connection_->Close();
}

template <typename Executor, typename Protocol, typename Message>
inline void ClientInterface<Executor, Protocol, Message>::
Reconnect(const Endpoint& server) noexcept
{
    Executor& executor{this->connection_->GetExecutor()};
    this->connection_ = ConnectionToServer::Create(executor, *this);
    this->connection_->ConnectToServer(server, 5s); //retries connection every 5s
}

template <typename Executor, typename Protocol, typename Message>
inline void ClientInterface<Executor, Protocol, Message>::
Send(const Message& message) noexcept
{
    if(this->IsConnected()) [[likely]]
        connection_->Send(message);
    else
        ASRT_LOG_ERROR("Not currently connected to server, send failed");
}

template <typename Executor, typename Protocol, typename Message>
inline void ClientInterface<Executor, Protocol, Message>::
SendSync(ConstMessageView message) noexcept //todo api needs to return error
{
    ASRT_LOG_TRACE("Client: SendSync");
    connection_->SendSync(message);
}

template <typename Executor, typename Protocol, typename Message>
inline auto ClientInterface<Executor, Protocol, Message>::
RetrieveInbox() noexcept -> Inbox*
{
    return this->inbox_.has_value() ?
        &(this->inbox_.value()) : nullptr;
}


template <typename Executor, typename Protocol, typename Message>
inline void ClientInterface<Executor, Protocol, Message>::
OnConnectionError(ErrorCode_Ns::ErrorCode ec) noexcept
{
    using enum ErrorCode_Ns::ErrorCode;

    ASRT_LOG_DEBUG("Client got connection error {}", ec);

    if(ec == end_of_file || ec == connection_reset){
        ASRT_LOG_INFO("Server disconnected");
        this->OnServerDisconnect();
    }
}

} //end ns ClientServer