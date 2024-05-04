#ifndef ASRT_BASIC_STREAM_SOCKET_
#define ASRT_BASIC_STREAM_SOCKET_

#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <cstdint>
#include <unistd.h>
#include <stdlib.h>
#include <memory>
#include <mutex>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <iostream>

#include "asrt/socket/basic_socket.hpp"
#include "asrt/error_code.hpp"
#include "asrt/sys/syscall.hpp"
#include "asrt/netbuffer.hpp"
#include "asrt/socket/types.hpp"
#include "asrt/util.hpp"
#include "asrt/socket/async_operation.hpp"
#include "asrt/common_types.hpp"
#include "asrt/type_traits.hpp"
#include "asrt/socket/socket_option.hpp"

namespace Socket{

using namespace Util::Expected_NS;
using namespace Util::Optional_NS;

using SendResult = Result<std::size_t>;
using ReceiveResult = Result<std::size_t>;
using ConnectOperationResult = Result<void>;
using SendCompletionHandler = std::function<void(SendResult&& send_result)>;
using ReceiveCompletionHandler = std::function<void(ReceiveResult&& receive_result)>;
using ConnectCompletionHandler = std::function<void(Result<void>)>;
using ReceiveBuffer = Buffer::MutableBufferView;
using SendBuffer = Buffer::ConstBufferView;
using SockAddressView = Buffer::ConstBufferView;

struct PeerCredentials 
    : public SockOption::SocketOption<PeerCredentials, SOL_SOCKET, SO_PEERCRED>
{
    constexpr PeerCredentials() noexcept = default;
    constexpr explicit PeerCredentials(::ucred data) noexcept : data_{data} {}
    constexpr explicit PeerCredentials(::pid_t pid, ::uid_t uid, ::gid_t gid) noexcept : data_{pid, uid, gid} {}
    constexpr PeerCredentials(PeerCredentials const&) noexcept = default;
    constexpr PeerCredentials& operator=(PeerCredentials const&) noexcept = default;
    constexpr ::ucred Value() const noexcept {return data_;}
    constexpr auto GetPid() const noexcept {return data_.pid;}
    constexpr auto GetUid() const noexcept {return data_.uid;}
    constexpr auto GetGid() const noexcept {return data_.gid;}
    constexpr std::size_t Length() const noexcept {return sizeof(data_);}
    ::ucred* data() noexcept {return &data_;}
    const ::ucred* data() const noexcept {return &data_;}
    constexpr bool operator==(PeerCredentials const&) const noexcept = default;
private:
    ::ucred data_;
};

namespace details{

    enum class BasicStreamSocketState : std::uint8_t
    {
        kDisconnected = 0u,
        kConnecting,
        kConnected,
        kDormant, //todo meaning?
        kConnectError
        //kConnected = 4u,
        //kUndefined = 0xFFu
    };

    inline std::string ToString(const BasicStreamSocketState state) noexcept
    {
        std::string printable;
        switch (state)
        {
        case BasicStreamSocketState::kDisconnected:
            printable = "Disconnected";
            break;
        case BasicStreamSocketState::kConnecting:
            printable = "Connecting";
            break;
        case BasicStreamSocketState::kConnected:
            printable = "Connected";
            break;
        case BasicStreamSocketState::kDormant:
            printable = "Dormant";
            break;
        case BasicStreamSocketState::kConnectError:
            printable = "Connect error";
            break;
        [[unlikely]] default:
            printable = "Invalid";
            break;
        }
        return printable;
    }

    inline std::ostream& operator<<(std::ostream& os, const BasicStreamSocketState socket_state)
    {
        os << ToString(socket_state);
        return os;
    } 
}

/*!
 * \brief Implements stream-oriented socket functionalities.
 *
 * \details
 * All basic socket operations like open/close and the reactor handling is implemented in the base class. This class
 * extends this by providing connect service as well as stream oriented communication methods.
 */
template <
    typename Protocol,
    class Executor>
class BasicStreamSocket final : 
    public BasicSocket<
        Protocol, 
        BasicStreamSocket<Protocol, Executor>,
        Executor>
{
    static_assert(ProtocolTraits::is_stream_based<Protocol>::value, "Invalid protocol used. Stream based protocol only!");

public:
    using Base = BasicSocket<Protocol, BasicStreamSocket, Executor>;
    using typename Base::BasicSocketState;
    using typename Base::Reactor;
    using typename Base::NativeHandleType;
    using typename Base::SockErrorCode;
    using typename Base::MutexType;
    using typename Base::EventType;
    template <typename T> using Result = typename Base::template Result<T>;

    using EndPointType = typename Protocol::Endpoint;
    using AddressType = typename Protocol::AddressType;    

    using ReactorEvents = ReactorNS::Events;
    using ConnectCompletionHandler = std::function<void(Result<void>&& connect_result)>;
    using BasicStreamSocketState = Socket::details::BasicStreamSocketState;

    friend Base;

    BasicStreamSocket() noexcept = default;
    explicit BasicStreamSocket(Executor& executor) noexcept : Base{executor} {};
    //explicit BasicStreamSocket(Reactor& reactor) : Base{reactor} {}
    BasicStreamSocket(BasicStreamSocket const&) = delete;
    BasicStreamSocket(BasicStreamSocket&& other) noexcept;
    BasicStreamSocket &operator=(BasicStreamSocket const &other) = delete;
    BasicStreamSocket &operator=(BasicStreamSocket &&other) = delete;
    ~BasicStreamSocket() noexcept
    {
        ASRT_LOG_TRACE("StreamSocket deconstructor, socket fd {}",
            Base::GetNativeHandle());
        Base::Destroy();
    }

public:
   /*!
   * \brief connects to specified endpoint synchronously.
   * \details
   * \param[in] path  Filepath to connect to. For use by unix domain sockets only.
   * \return whether the connection was successful
   */
    auto Connect(const EndPointType& remote_ep) noexcept -> Result<void>;

    /*!
   * \brief Tries to connect to specified endpoint synchronously. If a callback is specified, the connect operation
            will continue asynchronously, and the supplied callback will be informed about the result of the operation.
   * \param[in] path  Filepath to connect to. For use by unix domain sockets only.
   * \param[in] callback An optional function object to be notified about the completion of the connect operation.
   * \return Information on whether connection is complete or further execution is needed.
   */
    template<typename ConnectCompletionCallback>
    void ConnectAsync(const EndPointType& remote_ep, ConnectCompletionCallback&& callback) noexcept;

    /**
     * @brief 
     * 
     * @param remote_ep 
     * @return Result<void> 
     */
    auto ConnectSync(const EndPointType& remote_ep) noexcept -> Result<void>;

    /**
     * @brief Receive blocking until the full requested data has been received
     * 
     * @param recv_view The buffer into which the data will be received into. Must be mutable.
     * @return Expected<size_t, SockErrorCode> 
     */
    auto ReceiveSync(ReceiveBuffer recv_view) noexcept -> ReceiveResult;

    /** @brief Receive blocking until some (any) data has been received on the socket
     * 
     * @param recv_view The buffer into which the data will be received into. Must be mutable.
     * @return Expected<size_t, SockErrorCode> 
     */
    auto ReceiveSome(ReceiveBuffer recv_view) noexcept -> ReceiveResult;

    /**
     * @brief Tries to send the data contained in the buffer, and schedules asynchronous send if not all data could be sent. 
     * 
     * @tparam SendBufferView 
     * @tparam SendCompletionHandler 
     * @param send_view 
     * @param callback 
     */
    template <typename SendBufferView, typename SendCompletionHandler>
    void SendAsync(SendBufferView send_view, SendCompletionHandler&& callback) noexcept;


    /**
     * @brief Tries to send as much data contained in the buffer as possible but may send only partial data.
     * 
     * @tparam SendBufferView 
     * @param send_view 
     * @return Result<size_t> 
     */
    template <typename SendBufferView>
    auto SendSome(SendBufferView send_view) noexcept -> Result<size_t>;

    /**
     * @brief Send all data in buffer synchronously
     * 
     * @tparam SendBufferView 
     * @param send_view 
     * @return Result<void> 
     */
    template <typename SendBufferView>
    auto SendSync(SendBufferView send_view) noexcept -> Result<void>;

    /**
     * @brief Attempts to send the entire message synchronously, and if unable to do so, sends it asynchronously.
     * @details The operation may be performed asynchronously. That means:
     * - In case the operation is performed asynchronously: The completion callback informs about operation completion                                              
     * - In case the operation can be completed immediately: The completion callback is not called
     * The return value indicates whether the operation is performed asynchronously or not. 
     * @return Result<SendResult> 
     */
    template <typename SendBufferView, typename SendCompletionHandler>
    auto TrySend(SendBufferView send_view, SendCompletionHandler&& callback) noexcept -> Result<SendResult>;

    /**
     * @brief Receive from peer into recv_view. Handler will be invoked when operation is complete.
     * 
     * @tparam ReceiveCompletionCallback 
     * @param recv_view view of buffer into which message will be received
     * @param handler the callback that gets called when reception is complete
     */
    template<typename ReceiveBufferView, typename ReceiveCompletionCallback>
    void ReceiveAsync(ReceiveBufferView recv_view, ReceiveCompletionCallback&& handler) noexcept;

    /**
     * @brief Attempts to receive right away, and if unable to do so, receives asynchronously.
     * 
     * @tparam ReceiveCompletionCallback
     * @param recv_view 
     * @param handler 
     */
    template<typename ReceiveCompletionCallback>
    void TryReceiveAsync(ReceiveBuffer recv_view, ReceiveCompletionCallback&& handler) noexcept;
    
    /**
     * @brief 
     * 
     * @tparam ReceiveCompletionCallback 
     * @param recv_view 
     * @param handler 
     */
    template<typename ReceiveCompletionCallback>
    void ReceiveSomeAsync(ReceiveBuffer recv_view, ReceiveCompletionCallback&& handler) noexcept;

    /**
     * @brief 
     * 
     * @param protocol 
     * @param handle 
     * @return Result<void> 
     */
    Result<void> AssignAcceptedHandle(const Protocol& protocol, NativeHandleType handle) noexcept;

    BasicStreamSocketState GetStreamSocketState() const noexcept {return this->stream_sock_state_;}

    template <typename P = Protocol>
    PeerCredentials GetPeerCredentials() const noexcept
    {
        static_assert(ProtocolTraits::is_unix_domain<P>::value, "API only availaible for unix sockets");

        PeerCredentials sock_option{};
        (void)OsAbstraction::GetSocketOptions(Base::GetNativeHandle(), sock_option);
        return sock_option;
    }

    friend std::ostream& operator<<(std::ostream& os, const BasicStreamSocket& stream_socket)
    {
        os << "[Socket type: " << "Data, "  
            << "protocol: " << Protocol{}
            << ", socket fd: " << stream_socket.GetNativeHandle()
            << ", socket state: " << stream_socket.GetStreamSocketState()
            << ", blocking: " << std::boolalpha << !stream_socket.IsNonBlocking() << "]";
        return os;
    }

private:

    bool IsConnected() const noexcept {return stream_sock_state_ == BasicStreamSocketState::kConnected;}

   /*!
   * \brief Implements base class Bind() functionality
   */
    auto BindToEndpointImpl(const EndPointType& ep) noexcept -> Result<void>;

    void OnReactorEventImpl(ReactorEvents ev, std::unique_lock<MutexType>& lock) noexcept;

    void OnCloseEvent() noexcept;
    
    Result<void> CheckIsConnected() const noexcept;

    Result<void> CheckNotConnected() const noexcept;
    
    using OperationType = Socket::Types::OperationType;
    using OperationContext = Socket::Types::OperationContext;

    using SendOperation = 
        Socket::AsyncOperation< 
            OperationType::kSend, 
            SendBuffer, 
            SendResult, 
            SendCompletionHandler>;

    using ReceiveOperation = 
        Socket::AsyncOperation< 
            OperationType::kReceive, 
            ReceiveBuffer, 
            ReceiveResult, 
            ReceiveCompletionHandler>;

    using ConnectOperation = 
        Socket::AsyncOperation< 
            OperationType::kConnect, 
            SockAddressView, 
            ConnectOperationResult, 
            ConnectCompletionHandler>;

    friend SendOperation;
    friend ReceiveOperation;

    template <typename ReceiveBufferView, typename ReceiveCompletionCallback>
    void DoReceiveAsync(ReceiveBufferView recv_view, ReceiveCompletionCallback&& callback, int op_mode = 0) noexcept;

    template <typename SendCompletionCallback>
    void DoSendAsync(SendBuffer send_view, SendCompletionCallback&& callback, int op_mode = 0) noexcept;

    void NotifySendResult(std::unique_lock<MutexType>& lock, Result<void>&& result) noexcept;
    void NotifyReceiveResult(std::unique_lock<MutexType>& lock, ReceiveResult&& result) noexcept;

    template<typename ConnectCompletionCallback>
    void DoConnectAsync(SockAddressView remote_address, ConnectCompletionCallback&& callback) noexcept;

    Result<void> DoConnect(const EndPointType& ep) noexcept;
    Result<void> DoConnect(const AddressType& addr) noexcept;

    void HandleConnectionEstablishment(std::unique_lock<MutexType>& lock) noexcept;

    /**
     * @warning This function doesn't take the lock parameter; instead it directly interacts with the underlying mutex
    */
    void HandleDataTranster(ReactorEvents ev, std::unique_lock<MutexType>& lock) noexcept;

    void SetStreamSocketState(BasicStreamSocketState state) noexcept { this->stream_sock_state_ = state; }


    void HandleSend(std::unique_lock<MutexType>& lock) noexcept;
    void HandleReceive(std::unique_lock<MutexType>& lock) noexcept;

    Result<void> CheckSendPossible() const noexcept;

    Result<void> CheckRecvPossible() const noexcept;

    bool IsAsyncInProgress() const noexcept {return (this->send_operation_.IsOngoing() || this->recv_operation_.IsOngoing());}

    ReceiveResult DoReceiveSync(ReceiveBuffer recv_view, int flags = 0) noexcept;

    SendOperation send_operation_{};

    ReceiveOperation recv_operation_{};

    ConnectOperation connect_operation_{};

    BasicStreamSocketState stream_sock_state_{BasicStreamSocketState::kDisconnected};

}; //end class BasicStreamSocket

template <typename Protocol, class Executor>
BasicStreamSocket<Protocol, Executor>::
BasicStreamSocket(BasicStreamSocket&& other) noexcept
{
    if(this->IsAsyncInProgress() || other.IsAsyncInProgress()){
        LogFatalAndAbort("Trying to move socket when asynchronous operations are in progress");
    }

    Base::MoveSocketFrom(std::move(other)); /* close socket and transfer reactor here */

    this->stream_sock_state_ = other.stream_sock_state_;

    //todo transfer send/recv/connect operations
    /* No need to move additional members. They are only valid during an ongoing asynchronous operation. */ 
}

template <typename Protocol, class Executor>
template <typename ConnectCompletionCallback>
inline void BasicStreamSocket<Protocol, Executor>::
ConnectAsync(const EndPointType& remote_ep, ConnectCompletionCallback&& callback) noexcept
{
    using namespace Socket::Types;
    assert(Base::IsAsyncPreconditionsMet());
    std::scoped_lock const lock{Base::GetMutexUnsafe()};

    ASRT_LOG_TRACE("ConnectAsync entry");
    this->DoConnectAsync(remote_ep.DataView(), std::move(callback));
}

template <typename Protocol, class Executor>
template<typename ConnectCompletionCallback>
inline void BasicStreamSocket<Protocol, Executor>::
DoConnectAsync(SockAddressView remote_address, ConnectCompletionCallback&& callback) noexcept
{
    using enum Socket::Types::OperationMode;
    auto immediate_completion{
        [this](ConnectCompletionCallback&& cb, Result<void>&& res){
            Base::PostImmediateExecutorJob(
                [this, callback = std::move(cb), result = std::move(res)](){
                    if(result.has_value()){
                        ASRT_LOG_TRACE("ConnectAsync immediate completion success");
                        this->SetStreamSocketState(BasicStreamSocketState::kConnected);
                    }else{
                        ASRT_LOG_TRACE("ConnectAsync immediate completion error: {}", result.error());
                        this->SetStreamSocketState(BasicStreamSocketState::kConnectError);
                    }
                    callback(std::move(result));
                });
        }};
    
    // if(this->GetStreamSocketState() == BasicStreamSocketState::kConnected) [[unlikely]] {
    //     ASRT_LOG_TRACE("Notifying socket connect completion with error: socket_already_connected");
    //     immediate_completion(
    //         std::move(callback), MakeUnexpected(SockErrorCode::socket_already_connected));
    // }
    
    Socket::Types::OperationStatus const op_status{
        this->connect_operation_.Perform(
            Base::GetNativeHandle(),
            kSpeculative, /* always perform speculative connect */
            remote_address,
            std::move(callback),
            std::move(immediate_completion))};
    
    if(op_status != OperationStatus::kComplete) {
        this->SetStreamSocketState(BasicStreamSocketState::kConnecting);
        /* man connect(2): 
            It is possible to select(2) or poll(2) for completion 
            by selecting the socket for writing. */
        Base::AsyncWriteOperationStarted();
        ASRT_LOG_TRACE("Starting async connect operation");
    }
}

template <typename Protocol, class Executor>
template <typename SendBufferView, typename SendCompletionCallback>
inline void BasicStreamSocket<Protocol, Executor>::
SendAsync(SendBufferView send_view, SendCompletionCallback&& callback) noexcept
{
    using namespace Socket::Types;
    ASRT_LOG_TRACE("Socket fd {} Start async send", this->GetNativeHandle());
    assert(Base::IsAsyncPreconditionsMet());
    std::scoped_lock const lock{Base::GetMutexUnsafe()};

    this->DoSendAsync(send_view, std::move(callback), kSpeculative | kExhaustive);
}

template <typename Protocol, class Executor>
template <typename SendBufferView>
inline auto BasicStreamSocket<Protocol, Executor>::
SendSome(SendBufferView send_view) noexcept -> Result<size_t>
{
    std::scoped_lock const lock{Base::GetMutex()};
    return this->CheckSendPossible()
        .and_then([this, send_view](){
            return OsAbstraction::Send(this->GetNativeHandle(), send_view);
        })
        .map_error([this](SockErrorCode error){
            if(((this->stream_sock_state_ == BasicStreamSocketState::kConnected) || (this->stream_sock_state_ == BasicStreamSocketState::kDormant)) && 
                (error == SockErrorCode::not_connected)){
                ASRT_LOG_TRACE("Setting socket state dormant");
                this->SetStreamSocketState(BasicStreamSocketState::kDormant);
            }
            return error;
        });
}

template <typename Protocol, class Executor>
template <typename SendBufferView>
inline auto BasicStreamSocket<Protocol, Executor>::
SendSync(SendBufferView send_view) noexcept -> Result<void>
{
    using enum BasicStreamSocketState;
    std::scoped_lock const lock{Base::GetMutex()};
    return this->CheckSendPossible()
        .and_then([this, send_view](){
            return OsAbstraction::SendAll(this->GetNativeHandle(), send_view);
        })
        .map_error([this](SockErrorCode error){
            if(((this->stream_sock_state_ == kConnected) || (this->stream_sock_state_ == kDormant)) && //todo
                (error == SockErrorCode::not_connected)){
                this->SetStreamSocketState(kDormant);
            }
            return error;
        });
}

template <typename Protocol, class Executor>
template <typename SendBufferView, typename SendCompletionCallback>
[[deprecated("not correctly implemented")]] inline auto BasicStreamSocket<Protocol, Executor>::
TrySend(SendBufferView send_view, SendCompletionCallback&& callback) noexcept -> Result<SendResult>
{
    std::scoped_lock const lock{Base::GetMutex()};
    return this->CheckSendPossible()
        .and_then([this, send_view](){
            return OsAbstraction::NonBlockingSend(this->GetNativeHandle(), send_view);
        })
        .map_error([this](SockErrorCode error){
            if(((this->stream_sock_state_ == BasicStreamSocketState::kConnected) || (this->stream_sock_state_ == BasicStreamSocketState::kDormant)) && 
                (error == SockErrorCode::not_connected)){
                this->SetStreamSocketState(BasicStreamSocketState::kDormant);
            }
            return error;
        });
}
template <typename Protocol, class Executor>
template <typename SendCompletionCallback>
inline void BasicStreamSocket<Protocol, Executor>::
DoSendAsync(SendBuffer send_view, SendCompletionCallback&& callback, int op_mode) noexcept
{
    using namespace Socket::Types;
    auto immediate_completion{
        [this](SendCompletionCallback&& cb, SendResult&& res){
            Base::PostImmediateExecutorJob(
                [callback = std::move(cb), result = std::move(res)](){
                    callback(std::move(result));
                });
        }};

    Socket::Types::OperationStatus const op_status{
        this->send_operation_.Perform(
            Base::GetNativeHandle(),
            op_mode, 
            send_view,
            std::move(callback),
            std::move(immediate_completion))};

    if(op_status != OperationStatus::kComplete) [[unlikely]] {
        Base::AsyncWriteOperationStarted();
    }
}

template <typename Protocol, class Executor>
template <typename ReceiveBufferView, typename ReceiveCompletionCallback>
inline void BasicStreamSocket<Protocol, Executor>::
DoReceiveAsync(ReceiveBufferView recv_view, ReceiveCompletionCallback&& callback, int op_mode) noexcept
{
    using enum Socket::Types::OperationMode;
    auto immediate_completion{
        [this](ReceiveCompletionCallback&& cb, ReceiveResult&& res){
            Base::PostImmediateExecutorJob(
                [callback = std::move(cb), result = std::move(res)]() mutable {
                    callback(std::move(result));
                });
        }};
    
    Socket::Types::OperationStatus const op_status{
        this->recv_operation_.Perform(
            Base::GetNativeHandle(),
            op_mode, 
            recv_view,
            std::move(callback),
            std::move(immediate_completion))};
    
    if(op_status != OperationStatus::kComplete) {
        Base::AsyncReadOperationStarted();
        if(op_mode & kSpeculative) {
            ASRT_LOG_TRACE("Consumed read event");
            /* update socket read readiness status */
            Base::ReadEventConsumed();
        }
    }
}

template <typename Protocol, class Executor>
template <typename ReceiveBufferView, typename ReceiveCompletionCallback>
inline void BasicStreamSocket<Protocol, Executor>::
ReceiveAsync(ReceiveBufferView recv_view, ReceiveCompletionCallback&& callback) noexcept
{
    using namespace Socket::Types;
    assert(Base::IsAsyncPreconditionsMet());
    std::scoped_lock const lock{Base::GetMutexUnsafe()};

    const auto op_mode{
        Base::IsSocketReadableUnsafe() ? 
        (kSpeculative | kExhaustive) : kExhaustive};

    ASRT_LOG_TRACE("ReceiveAsync entry, {}", op_mode);
    this->DoReceiveAsync(recv_view, std::move(callback), (int)op_mode);
}

template <typename Protocol, class Executor>
template <typename ReceiveCompletionCallback>
inline void BasicStreamSocket<Protocol, Executor>::
TryReceiveAsync(ReceiveBuffer recv_view, ReceiveCompletionCallback&& callback) noexcept
{
    using namespace Socket::Types;
    assert(Base::IsAsyncPreconditionsMet());
    std::scoped_lock const lock{Base::GetMutexUnsafe()};

    ASRT_LOG_TRACE("TryReceiveAsync entry");
    this->DoReceiveAsync(recv_view, std::move(callback), kSpeculative | kExhaustive);
}

template <typename Protocol, class Executor>
template <typename ReceiveCompletionCallback>
inline void BasicStreamSocket<Protocol, Executor>::
ReceiveSomeAsync(ReceiveBuffer recv_view, ReceiveCompletionCallback&& callback) noexcept
{
    assert(Base::IsAsyncPreconditionsMet());
    std::scoped_lock const lock{Base::GetMutexUnsafe()};

    Socket::Types::OperationMode const op_mode{
        Base::IsSocketReadableUnsafe() ? kSpeculative : 0};

    ASRT_LOG_TRACE("ReceiveSomeAsync entry");
    this->DoReceiveAsync(recv_view, std::move(callback), op_mode);
}

template <typename Protocol, class Executor>
inline void BasicStreamSocket<Protocol, Executor>::
OnCloseEvent() noexcept
{
    ASRT_LOG_TRACE("[StreamSocket]: socket {} received close event",
        Base::GetNativeHandle());

    // this->send_operation_.Reset(); //double free?
    // this->recv_operation_.Reset(); //double free?
    // this->connect_operation_.Reset(); //double free?
    this->SetStreamSocketState(BasicStreamSocketState::kDisconnected);
}

template <typename Protocol, class Executor>
inline void BasicStreamSocket<Protocol, Executor>::
OnReactorEventImpl(ReactorEvents ev, std::unique_lock<MutexType>& lock) noexcept
{
    assert(lock.owns_lock()); //asert lock held by reactor thread

    switch(this->GetStreamSocketState())
    {
        [[unlikely]] case BasicStreamSocketState::kDisconnected:
            /* socket closed before arrival of notification; just drop the message */
            ASRT_LOG_INFO("Socket closed. Dropping event.");
            Base::OnReactorEventIgnored(ev);
            break;
        case BasicStreamSocketState::kConnecting:
            this->HandleConnectionEstablishment(lock);
            break;
        case BasicStreamSocketState::kConnected:
            this->HandleDataTranster(ev, lock); /* unlocks and re-locks mutex inside */
            break;
        [[unlikely]] case BasicStreamSocketState::kDormant:
            /* just drop message on socket error */
            ASRT_LOG_WARN("Socket state: kDormant. Dropping event.");
            Base::OnReactorEventIgnored(ev);
            break;
        [[unlikely]] case BasicStreamSocketState::kConnectError:
            /* just drop message on socket error */
            ASRT_LOG_WARN("Socket state: kConnectError. Dropping event.");
            Base::OnReactorEventIgnored(ev);
            break;
        [[unlikely]] default:
            LogFatalAndAbort("Unexpected stream socket state encountered!");
            break;
    }
}

template <typename Protocol, class Executor>
inline void BasicStreamSocket<Protocol, Executor>::
HandleConnectionEstablishment(std::unique_lock<MutexType>& lock) noexcept
{
    ASRT_LOG_TRACE("[StreamSocket]: Handling sockfd {} connect", Base::GetNativeHandle());
    assert(Base::IsNonBlocking());

    auto on_immediate_completion{
        [this, &lock](auto&& callback, ConnectOperationResult&& res){
            this->SetStreamSocketState(res.has_value() ?
                BasicStreamSocketState::kConnected :
                BasicStreamSocketState::kConnectError);

            ASRT_LOG_TRACE("Notifying connect completion");
            lock.unlock();
            /* this is executor context so we directly invoke the handler */
            callback(std::move(res));
            lock.lock();
            /* since we are relying on edge-triggered notifications 
                we do not remove interest in write events from epoll */
        }
    };

    const auto op_status{
        this->connect_operation_.Perform(
            Base::GetNativeHandle(), std::move(on_immediate_completion))};

    if(op_status != OperationStatus::kComplete){
        Base::AsyncWriteOperationStarted();
        ASRT_LOG_TRACE("[BasicStreamSocket]: Continuing async connect");
    }
}

template <typename Protocol, class Executor>
inline void BasicStreamSocket<Protocol, Executor>::
HandleDataTranster(ReactorEvents ev, std::unique_lock<MutexType>& lock) noexcept
{
    ASRT_LOG_TRACE("[StreamSocket]: Handling sockfd {} data transfer", this->GetNativeHandle());
    /* prioritize socket read events */
    if(ev.HasReadEvent()) [[likely]] {
        /* only perform io if we are in the middle of an asynchronous operation 
            since we may receive events that we never registered for */
        if(this->recv_operation_.IsOngoing()) [[likely]]
            this->HandleReceive(lock);
        else [[unlikely]] {
            ASRT_LOG_TRACE("Got uninteresting read event");
            Base::OnReactorEventIgnored(EventType::kRead);
        }
    }

    /* async sends are assumed to be rare */
    if(this->send_operation_.IsOngoing()) [[unlikely]] 
        if(ev.HasWriteEvent()) {
             /* recheck preconditions since lock has been released during HandleReceive() */    
            if(this->IsConnected()) [[likely]] {  
                if(this->send_operation_.IsOngoing()) [[likely]]
                    this->HandleSend(lock);
                else [[unlikely]] {
                    ASRT_LOG_TRACE("Got uninteresting write event");
                    Base::OnReactorEventIgnored(EventType::kWrite);
                }
            }
        }
}

template <typename Protocol, class Executor>
inline void BasicStreamSocket<Protocol, Executor>::
HandleSend(std::unique_lock<MutexType>& lock) noexcept
{
    ASRT_LOG_TRACE("[StreamSocket]: Handling sockfd {} send", this->GetNativeHandle());
    assert(Base::IsNonBlocking());

    auto on_immediate_completion{
        [this, &lock](auto&& callback, SendResult&& res){
            ASRT_LOG_TRACE("Notifying send completion");
            lock.unlock();
            /* this is executor context so we directly invoke the handler */
            callback(std::move(res));
            lock.lock();
            /* since we are relying on edge-triggered notifications 
                we do not remove interest in write events from epoll */
        }
    };

    const auto op_status{
        this->send_operation_.Perform(
            Base::GetNativeHandle(), std::move(on_immediate_completion))};

    if(op_status != OperationStatus::kComplete){
        Base::AsyncWriteOperationStarted();
    }
}

template <typename Protocol, class Executor>
inline void BasicStreamSocket<Protocol, Executor>::
HandleReceive(std::unique_lock<MutexType>& lock) noexcept
{
    ASRT_LOG_TRACE("[StreamSocket]: Handling sockfd {} receive", this->GetNativeHandle());
    assert(Base::IsNonBlocking());

    auto on_immediate_completion{
        [this, &lock](auto&& callback, ReceiveResult&& res){
            ASRT_LOG_TRACE("Notifying receive completion");
            lock.unlock();
            /* this is executor context so we directly invoke the handler */
            callback(std::move(res));
            lock.lock();
            /* since we are relying on edge-triggered notifications 
                we do not remove interest in read events from epoll */
        }
    };

    const auto op_status{
        this->recv_operation_.Perform(
            Base::GetNativeHandle(), std::move(on_immediate_completion))};

    if(op_status != OperationStatus::kComplete){
        Base::AsyncReadOperationStarted();
    }
}

}//end namespace Socket

template<>
struct fmt::formatter<Socket::PeerCredentials> : fmt::formatter<int>
{
    auto format(Socket::PeerCredentials data, format_context &ctx) const -> decltype(ctx.out())
    {
        return format_to(ctx.out(), "[{}:{}:{}]", data.GetPid(), data.GetUid(), data.GetGid());
    }
};

#if defined(ASRT_HEADER_ONLY)
# include "asrt/socket/impl/stream_socket.ipp"
#endif // defined(ASRT_HEADER_ONLY)


#endif /* ASRT_BASIC_STREAM_SOCKET_ */
