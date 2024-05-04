#include "asrt/socket/basic_stream_socket.hpp"

namespace Socket{

template <typename Protocol, class Executor>
inline auto BasicStreamSocket<Protocol, Executor>::
BindToEndpointImpl(const EndPointType& ep) noexcept -> Result<void>
{
    return OsAbstraction::Bind(this->GetNativeHandle(), ep.DataView());
}

template <typename Protocol, class Executor>
inline auto BasicStreamSocket<Protocol, Executor>::
CheckIsConnected() const noexcept -> Result<void>
{
    return this->IsConnected() ?
        Result<void>{} :
        MakeUnexpected(SockErrorCode::socket_not_connected);
}

template <typename Protocol, class Executor>
inline auto BasicStreamSocket<Protocol, Executor>::
CheckNotConnected() const noexcept -> Result<void>
{
    return !this->IsConnected() ?
        Result<void>{} :
        MakeUnexpected(SockErrorCode::socket_already_connected);
}

template <typename Protocol, class Executor>
inline auto BasicStreamSocket<Protocol, Executor>::
Connect(const EndPointType& remote_ep) noexcept -> Result<void>
{
    std::scoped_lock const lock{Base::GetMutex()}; 
    return Base::TryOpenSocket()
        .and_then([this](){
            return this->CheckNotConnected();
        })
        .and_then([this, &remote_ep]{
            return Base::CheckProtocolMatch(remote_ep);
        })
        .and_then([this, &remote_ep]() -> Result<void> {
            return OsAbstraction::Connect(this->GetNativeHandle(), remote_ep.DataView());
        })
        .map([this](){
            this->SetStreamSocketState(BasicStreamSocketState::kConnected);
        });
}

template <typename Protocol, class Executor>
inline auto BasicStreamSocket<Protocol, Executor>::
CheckRecvPossible() const noexcept -> Result<void> 
{
    return Base::CheckSocketOpen()
        .and_then([this](){
            return this->recv_operation_.IsOngoing() ?
                MakeUnexpected(SockErrorCode::receive_operation_ongoing) :
                Result<void>{};    
        });
}

template <typename Protocol, class Executor>
inline auto BasicStreamSocket<Protocol, Executor>::
CheckSendPossible() const noexcept -> Result<void> 
{
    return this->CheckIsConnected()
        .and_then([this](){
            return this->send_operation_.IsOngoing() ?
                MakeUnexpected(SockErrorCode::send_operation_ongoing) :
                Result<void>{};
        });
}

template <typename Protocol, class Executor>
inline auto BasicStreamSocket<Protocol, Executor>::
ReceiveSync(ReceiveBuffer recv_view) noexcept -> ReceiveResult
{
    std::scoped_lock const lock{Base::GetMutex()};
    return this->CheckRecvPossible()
        .and_then([this, recv_view]() -> Result<size_t> {
            return this->DoReceiveSync(recv_view, MSG_WAITALL);
        });
}

template <typename Protocol, class Executor>
inline auto BasicStreamSocket<Protocol, Executor>::
ReceiveSome(ReceiveBuffer recv_view) noexcept -> ReceiveResult
{
    std::scoped_lock const lock{Base::GetMutex()};
    return this->CheckRecvPossible()
        .and_then([this, recv_view]() -> Result<size_t> {
            return this->DoReceiveSync(recv_view);
        });
}

template <typename Protocol, class Executor>
inline auto BasicStreamSocket<Protocol, Executor>::
AssignAcceptedHandle(const Protocol& protocol, NativeHandleType handle) noexcept -> Result<void>
{
    /* no mutex protection needed as the only use case for this API is to assign accepted
        socket handle to a new (not-open) socket. All operations to the socket that may alter
        its internal data could only take place after this API returns */
    ASRT_LOG_TRACE("[BasicStreamSocket]: Assigning accepted socket handle {}", handle);
    return Base::AssignNativeHandle(protocol, handle)
        .map([this]() {
            this->SetStreamSocketState(BasicStreamSocketState::kConnected);
        });
}
    
template <typename Protocol, class Executor>
inline auto BasicStreamSocket<Protocol, Executor>::
DoReceiveSync(ReceiveBuffer recv_view, int flags) noexcept -> ReceiveResult
{   
    /* man recv(): "The value 0 may also be returned if the requested number of bytes 
        to receive from a stream socket was 0." Here we don't depend on the implementation
        for this behavior (since the standard doesn't gurantee it). Instead we always
        return zero in userspace when we detect a zero-byte receive. */
    if(recv_view.size() == 0) [[unlikely]] /* reading 0-bytes on a stream socket is a no-op */
        return ReceiveResult{0};

    return OsAbstraction::ReceiveWithFlags(this->GetNativeHandle(), recv_view, flags)
        .or_else([this](SockErrorCode ec) -> ReceiveResult {
            if(ErrorCode_Ns::IsBusy(ec) && Base::IsNonBlocking())
                return ReceiveResult{0};
            return MakeUnexpected(ec);
        });
}

/* explicit instantiations for async send/recv/connect operations */
template class AsyncOperation<
        OperationType::kSend,
        SendBuffer,
        SendResult,
        SendCompletionHandler>;

template class AsyncOperation<
        OperationType::kReceive,
        ReceiveBuffer,
        ReceiveResult,
        ReceiveCompletionHandler>;

template class AsyncOperation<
        OperationType::kConnect,
        SockAddressView,
        ConnectOperationResult,
        ConnectCompletionHandler>;

} //end ns