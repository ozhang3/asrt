#ifndef ASRT_BASIC_DATAGRAM_SOCKET_HPP_
#define ASRT_BASIC_DATAGRAM_SOCKET_HPP_

#include <cstdint>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <memory>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <iostream>
#include <span>
//#include <vector>
//#include <array>

#include "asrt/error_code.hpp"
#include "asrt/socket/protocol.hpp"
#include "asrt/socket/basic_endpoint.hpp"
#include "asrt/sys/syscall.hpp"
#include "asrt/netbuffer.hpp"
#include "asrt/reactor/epoll_reactor.hpp"
#include "asrt/socket/types.hpp"
#include "asrt/util.hpp"
#include "asrt/common_types.hpp"
#include "asrt/socket/basic_socket.hpp"


namespace Socket{

using namespace Util::Expected_NS;
using Util::Optional_NS::Optional;


/**
 * @brief datagram socket implementation
 * 
 * @tparam Protocol UDP / UnixDgram
 * @tparam Executor IO_Executor
 */
template <typename Protocol, class Executor>
class BasicDgramSocket final : 
    public BasicSocket<
        Protocol, 
        BasicDgramSocket<Protocol, Executor>,
        Executor>
{

public:
    using SockErrorCode = ErrorCode_Ns::ErrorCode;
    using EndPointType = typename Protocol::Endpoint;
    using Events = ReactorNS::Events;
    using SendCompletionHandler = std::function<void(Expected<std::size_t, SockErrorCode>&& send_result)>;
    using ReceiveCompletionHandlerType = std::function<void(Expected<std::size_t, SockErrorCode>&& receive_result)>;
    using Base = BasicSocket<Protocol, BasicDgramSocket, Executor>;
    using AddressType = typename Base::AddressType;
    using typename Base::Reactor;
    using typename Base::EventType;
    using typename Base::MutexType;

    BasicDgramSocket() noexcept = default;
    explicit BasicDgramSocket(Executor& executor) noexcept : Base{executor} {};
    BasicDgramSocket(BasicDgramSocket const&) = delete;
    BasicDgramSocket(BasicDgramSocket&& other) noexcept;
    BasicDgramSocket &operator=(BasicDgramSocket const &other) = delete;
    BasicDgramSocket &operator=(BasicDgramSocket &&other) = delete;
    ~BasicDgramSocket() noexcept
    {
        Base::Destroy();
    }

    auto BindToEndpointImpl(const EndPointType& ep) -> Expected<void, SockErrorCode>;

    auto BindToAddrImpl(const AddressType& ep) -> Expected<void, SockErrorCode>;

    auto SetDefaultPeer(const EndPointType& remote_ep) -> Expected<void, SockErrorCode>;

    auto SetDefaultPeer(const AddressType& addr) -> Expected<void, SockErrorCode>;

    auto RemoveDefaultPeer() -> Expected<void, SockErrorCode>;

    constexpr auto HasDefaultPeer() -> bool {return this->has_default_peer_;}

    auto OnReactorEventImpl(Events ev, std::unique_lock<MutexType>& lock) -> void;

    auto OnCloseEvent() -> void;

    auto SendSome(Buffer::ConstBufferView buffer_view) -> Expected<size_t, SockErrorCode>;
    
    // Datagram sockets in various domains (e.g., the UNIX and Internet domains) permit zero-length datagrams. 
    // When such a datagram is received, the return value is 0.
    auto ReceiveSome(Buffer::MutableBufferView buffer_view) -> Expected<size_t, SockErrorCode>;

    /// @brief 
    /// @tparam ReceiveCompletionHandler Type of handler: must conform to signature void F(Expected<std::size_t, SockErrorCode>)
    /// @param buffer_view view of buffer into which datagram will be received
    /// @param handler the callback that gets called when reception is complete; 
    ///         this funciton takes ownership of the handler 
    ///          hence the state of the handler is no longer valid after being passed to this function 
    /// @return the result of the async operation
    template<typename ReceiveCompletionHandler>
    auto ReceiveAsync(Buffer::MutableBufferView buffer_view, ReceiveCompletionHandler&& handler) -> Expected<void, SockErrorCode>;

    //auto RecvFromSync(const Buffer::NetBufferType& buffer_view) -> bool; //todo

    friend std::ostream& operator<<(std::ostream& os, const BasicDgramSocket& socket)
    {
        os << "[socket protocol: " << Protocol{}
            << ", socket fd: " << socket.GetNativeHandle()
            << ", state: " << socket.GetBasicSocketState()
            << ", default peer exists: " << std::boolalpha << socket.has_default_peer_
            << ", blocking: " << std::boolalpha << !socket.IsNonBlocking() << "]";
        return os;
    }

private:

    void HandleSend();
    void HandleReceive();

    auto CheckSendPossible() const -> Expected<void, SockErrorCode>;

    auto CheckRecvPossible() const -> Expected<void, SockErrorCode>;

    constexpr bool IsAsyncInProgress() {return (send_ongoing_ || recv_ongoing_);}

    constexpr auto CheckDefaultPeerExists() -> Expected<void, SockErrorCode>
    {
        return
            this->has_default_peer_ ?
                Expected<void, SockErrorCode>{} :
                MakeUnexpected(SockErrorCode::no_default_peer);
    }

    bool recv_ongoing_{false};
    bool send_ongoing_{false};

    bool has_default_peer_{false};

    Buffer::ConstBufferView send_buffview_{nullptr, 0};
    Buffer::MutableBufferView recv_buffview_{nullptr, 0};

    SendCompletionHandler send_completion_handler_{};
    ReceiveCompletionHandlerType recv_completion_handler_{};
};


template <typename Protocol, class Executor>
BasicDgramSocket<Protocol, Executor>::
BasicDgramSocket(BasicDgramSocket&& other) noexcept
{
    if(this->IsAsyncInProgress() || other.IsAsyncInProgress()){
        LogFatalAndAbort("Trying to move socket when asynchronous operations are in progress");
    }

    Base::MoveSocketFrom(std::move(other)); /* close socket and transfer reactor here */

    this->has_default_peer_ = other.has_default_peer_;
    this->send_ongoing_ = false;
    this->recv_ongoing_ = false;

    /* No need to move additional members. 
    They are only valid during an ongoing asynchronous operation. */ 
}

template <typename Protocol, class Executor>
inline void BasicDgramSocket<Protocol, Executor>::
OnCloseEvent()
{
    std::cout << "DgramSocket: received close event\n";
    this->recv_ongoing_ = false;
    this->send_ongoing_ = false;
    this->has_default_peer_ = false;
}

template <typename Protocol, class Executor>
inline auto BasicDgramSocket<Protocol, Executor>::
BindToEndpointImpl(const EndPointType& ep) -> Expected<void, SockErrorCode>
{
    return OsAbstraction::Bind(Base::GetNativeHandle(), ep.DataView());
}

template <typename Protocol, class Executor>
inline auto BasicDgramSocket<Protocol, Executor>::
SetDefaultPeer(const EndPointType& remote_ep) -> Expected<void, SockErrorCode>
{
    std::scoped_lock const lock{Base::GetMutex()}; 
    return Base::TryOpenSocket()
        .and_then([this, &remote_ep]{
            return Base::CheckProtocolMatch(remote_ep);
        })
        .and_then([this, &remote_ep]() -> Expected<void, SockErrorCode> {
            return OsAbstraction::Connect(Base::GetNativeHandle(), remote_ep.DataView());
        })
        .map([this](){
            this->has_default_peer_ = true;
        });
}

template <typename Protocol, class Executor>
inline auto BasicDgramSocket<Protocol, Executor>::
RemoveDefaultPeer() -> Expected<void, SockErrorCode>
{
    return CheckDefaultPeerExists()
        .and_then([this](){ /* if default peer exists */
            /* dissolve association by connecting to AF_UNSPEC address */
            return OsAbstraction::Connect(
                Base::GetNativeHandle(), 
                std::span<std::uint8_t const>{
                   (std::uint8_t const*)(&Types::kUnspecUnixSockAddress),
                    sizeof(Types::kUnspecUnixSockAddress)});
        })
        .map([this](){
            this->has_default_peer_ = false;
        });
}

template <typename Protocol, class Executor>
inline void BasicDgramSocket<Protocol, Executor>::
OnReactorEventImpl(Events ev, std::unique_lock<MutexType>& lock)
{
    assert(lock.owns_lock()); //asert lock held by reactor thread

    ASRT_LOG_TRACE("[DatagramSocket]: OnReactorEvent()");

    if(ev.HasReadEvent())
        this->HandleReceive();

    if(ev.HasWriteEvent())
        this->HandleSend();
}

template <typename Protocol, class Executor>
inline void BasicDgramSocket<Protocol, Executor>::
HandleSend()
{
    std::cout << "Detected write event\n";
    /* call send directly with no fear of blocking */
    auto send_result{OsAbstraction::Send(Base::GetNativeHandle(), this->send_buffview_, 0)}; //todo: check if flag param need to be filled out here

    /* call completion callbck */
    this->send_completion_handler_(std::move(send_result));

    if(!recv_ongoing_)
        Base::ChangeReactorObservation(EventType::kWrite, false);
}

template <typename Protocol, class Executor>
inline void BasicDgramSocket<Protocol, Executor>::
HandleReceive()
{
    std::cout << "Detected read event\n";
    /* call recv directly with no fear of blocking */
    auto recv_result{OsAbstraction::Receive(Base::GetNativeHandle(), this->recv_buffview_, 0)}; //todo: check if flag param need to be filled out here

    /* call completion callbck */
    this->recv_completion_handler_(std::move(recv_result));

    if(!recv_ongoing_)
        Base::ChangeReactorObservation(EventType::kRead, false);

}

template <typename Protocol, class Executor>
inline auto BasicDgramSocket<Protocol, Executor>::
CheckRecvPossible() const -> Expected<void, SockErrorCode> 
{
    return
        this->CheckSocketOpen()
        .and_then([this](){
            return this->recv_ongoing_ ?
                MakeUnexpected(SockErrorCode::receive_operation_ongoing) :
                Expected<void, SockErrorCode>{};
        });
}

template <typename Protocol, class Executor>
inline auto BasicDgramSocket<Protocol, Executor>::
CheckSendPossible() const -> Expected<void, SockErrorCode> 
{
    return
        this->CheckSocketOpen()
        .and_then([this](){
            return this->send_ongoing_ ?
                MakeUnexpected(SockErrorCode::receive_operation_ongoing) :
                Expected<void, SockErrorCode>{};
        });
}

template <typename Protocol, class Executor>
inline auto BasicDgramSocket<Protocol, Executor>::
SendSome(Buffer::ConstBufferView buffer_view) -> Expected<size_t, SockErrorCode>
{
    auto pre_send_check_result{
        this->CheckDefaultPeerExists()
        .and_then([this]() -> Expected<void, SockErrorCode>{
            return this->CheckSendPossible();
        })};
    
    if(pre_send_check_result.has_value())
    {
        return 
            OsAbstraction::Send(Base::GetNativeHandle(), buffer_view, 0)
            .or_else([this](SockErrorCode ec) -> Expected<size_t, SockErrorCode> {
                if(Base::IsNonBlocking() ||
                    (ec != SockErrorCode::try_again && ec != SockErrorCode::would_block))
                    return 0;
                else
                /* received wouldblock on a blocking socket (how is this possible??)*/
                    return MakeUnexpected(SockErrorCode::default_error);
                    //return OsAbstraction::PollWrite(Base::GetNativeHandle(), -1);
        });
    }
    else
    {
        return MakeUnexpected(pre_send_check_result.error());
    }

}

template <typename Protocol, class Executor>
inline auto BasicDgramSocket<Protocol, Executor>::
ReceiveSome(Buffer::MutableBufferView buffer_view) -> Expected<size_t, SockErrorCode>
{
    auto pre_recv_check_result{this->CheckRecvPossible()};
    
    if(pre_recv_check_result.has_value())
    {
        return 
            OsAbstraction::Receive(Base::GetNativeHandle(), buffer_view, 0)
            .or_else([this](SockErrorCode ec) -> Expected<size_t, SockErrorCode> {
                if(Base::IsNonBlocking() ||
                    (ec != SockErrorCode::try_again && ec != SockErrorCode::would_block))
                    return 0;
                else
                /* received wouldblock on a blocking socket (how is this possible??)*/
                    return MakeUnexpected(SockErrorCode::default_error);
                    //return OsAbstraction::PollWrite(Base::GetNativeHandle(), -1);
        });
    }
    else
    {
        return MakeUnexpected(pre_recv_check_result.error());
    }
}

template <typename Protocol, class Executor>
template <typename ReceiveCompletionHandler>
inline auto BasicDgramSocket<Protocol, Executor>::
ReceiveAsync(Buffer::MutableBufferView buffer_view, ReceiveCompletionHandler&& handler) -> Expected<void, SockErrorCode>
{
    return
        this->CheckReactorAvailable()
        .and_then([this]() -> Expected<void, SockErrorCode>{
            if(!this->IsNonBlocking())
                return MakeUnexpected(SockErrorCode::socket_in_blocking_mode); //todo assert in stead of check
            else
                return {};
        })
        .and_then([this](){
            return this->CheckRecvPossible();
        })
        .map([this, buffer_view, &handler](){
            this->recv_ongoing_ = true;
            this->recv_buffview_ = buffer_view;
            this->recv_completion_handler_ = std::move(handler);
            this->recv_ongoing_ = false;
            Base::ChangeReactorObservation(EventType::kRead, true);
        });
}

}
#endif /* ASRT_BASIC_DATAGRAM_SOCKET_HPP_ */
