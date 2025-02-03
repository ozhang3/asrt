#include "asrt/type_traits.hpp"
#include "asrt/sys/syscall.hpp"
#include "asrt/socket/acceptor.hpp"

namespace Socket{

template <typename Protocol, class Executor>
BasicAcceptorSocket<Protocol, Executor>::
BasicAcceptorSocket(Executor& executor, 
    const BoundEndpointType& endpoint, AcceptorOptions options) noexcept 
    : Base{executor}
{
    Base::Open()
    .and_then([this, options](){
        ASRT_LOG_TRACE("Base socket open success");
        if constexpr (ProtocolTraits::is_internet_domain<Protocol>::value) {
            if(options == AcceptorOptions::kReuseAddress) [[likely]] {
                return Base::SetOption(SocketBase::ReuseAddress{true})
                    .map([](){
                        ASRT_LOG_TRACE("Acceptor set reuse address success");
                    });
            }
        }
        return Result<void>{};
    })
    .and_then([this, &endpoint](){
        if constexpr (ProtocolTraits::is_unix_domain<Protocol>::value) {
            (void)OsAbstraction::Unlink(endpoint.Path());
        }   
        return Base::Bind(endpoint);
    })
    .map_error([&endpoint](SockErrorCode ec){
        LogFatalAndAbort("Failed to construct/bind acceptor socket to {}, {}", endpoint, ec);
    });

}

template <typename Protocol, class Executor>
BasicAcceptorSocket<Protocol, Executor>::
BasicAcceptorSocket(BasicAcceptorSocket&& other) noexcept
{
    if(this->acceptor_sockstate_ == AcceptorSocketState::kAccepting || 
        other.acceptor_sockstate_ == AcceptorSocketState::kAccepting) [[unlikely]] {
        LogFatalAndAbort("Trying to move socket when asynchronous operations are in progress");
    }

    Base::MoveSocketFrom(std::move(other)); /* close socket and transfer reactor here */

    this->acceptor_sockstate_ = other.acceptor_sockstate_;

    /* No need to move additional members. 
       They are only valid during an ongoing asynchronous operation. */ 
}

template <typename Protocol, class Executor>
inline void BasicAcceptorSocket<Protocol, Executor>::
OnCloseEvent() noexcept
{
    ASRT_LOG_TRACE("[Acceptor]: received close event");
    this->SetAcceptorSocketState(AcceptorSocketState::kDisconnected);
    //todo release handler memory?
}

template <typename Protocol, class Executor>
inline void BasicAcceptorSocket<Protocol, Executor>::
OnReactorEventImpl(Events events, std::unique_lock<MutexType>& lock) noexcept
{
    assert(lock.owns_lock()); //asert lock held by reactor thread

    ASRT_LOG_TRACE("[Acceptor]: OnReactorEvent()");
    assert(!events.HasWriteEvent()); /* write events are never supposed to be captured by an acceptor socekt */
    if(this->acceptor_sockstate_ == AcceptorSocketState::kAccepting)
        this->HandleAysncAccept(lock);
    else [[unlikely]] {
        ASRT_LOG_INFO("[Acceptor]: Not currently accepting.");
        this->speculative_accept_ = true;
        Base::OnReactorEventIgnored(events);
    }
}

template <typename Protocol, class Executor>
inline void BasicAcceptorSocket<Protocol, Executor>::
HandleAysncAccept(std::unique_lock<MutexType>& lock) noexcept
{
    ASRT_LOG_TRACE("[Acceptor]: Handling async accept");
    lock.unlock(); /* unlock to allow reactor registration for accepted socket to proceed */
    auto accept_result{this->DoAccept(*this->peer_socket_, this->peer_info_)};
    lock.lock();

    /* make sure we recheck acceptor state upon lock reacquisition */
    if(!(this->acceptor_sockstate_ == AcceptorSocketState::kDisconnected)) [[likely]] {
        bool false_notification{
            !accept_result.has_value() && ErrorCode_Ns::IsBusy(accept_result.error())
        };

        if(!false_notification) [[likely]] {
            auto temp_handler{std::move(this->on_accept_complete_)};
            this->SetAcceptorSocketState(AcceptorSocketState::kListening);

            lock.unlock();
            ASRT_LOG_TRACE("[Acceptor]: Calling accept handler...");
            temp_handler(std::move(accept_result));
            lock.lock();
        }else [[unlikely]] {
            ASRT_LOG_INFO("[Acceptor]: False wakeup, re-submitting async accept request"); //spurious wake

            /* the accept event was not actually handled 
                therefore we re-initiate an async operation */
            Base::AsyncReadOperationStarted(); 
        }
    }
}

template <typename Protocol, class Executor>
inline auto BasicAcceptorSocket<Protocol, Executor>::
BindToEndpointImpl(const BoundEndpointType& ep) noexcept -> Result<void>
{
    return OsAbstraction::Bind(Base::GetNativeHandle(), ep.DataView())
        .map([this](){
            this->SetAcceptorSocketState(AcceptorSocketState::kBound);
        });
}

template <typename Protocol, class Executor>
inline auto BasicAcceptorSocket<Protocol, Executor>::
Listen() noexcept -> Result<void>
{    
    switch(this->acceptor_sockstate_)
    {
        case AcceptorSocketState::kBound:
            return this->DoListen();
        [[unlikely]] case AcceptorSocketState::kAccepting:
        /* listen() called whiile accept() is ongoing */
            return MakeUnexpected(SockErrorCode::accept_operation_ongoing);
        [[unlikely]] case AcceptorSocketState::kListening:
        /* already listen()ing */
            return MakeUnexpected(SockErrorCode::listen_operation_ongoing);
        [[unlikely]] case AcceptorSocketState::kDisconnected:
            return MakeUnexpected(SockErrorCode::socket_not_bound);
        [[unlikely]] default:
            return MakeUnexpected(SockErrorCode::socket_state_invalid);
    }
}

template <typename Protocol, class Executor>
inline auto BasicAcceptorSocket<Protocol, Executor>::
Accept(AcceptedSocketType& peer_socket) noexcept -> Result<void>
{    
    std::scoped_lock const lock{Base::GetMutexUnsafe()};
    return this->AcceptSyncInternal(peer_socket, nullptr);
}

template <typename Protocol, class Executor>
inline auto BasicAcceptorSocket<Protocol, Executor>::
Accept(AcceptedSocketType& peer_socket, AcceptedEndpointType& peer_endpoint) noexcept -> Result<void>
{    
    std::scoped_lock const lock{Base::GetMutexUnsafe()};
    return this->AcceptSyncInternal(peer_socket, &peer_endpoint);
}

template <typename Protocol, class Executor>
inline auto BasicAcceptorSocket<Protocol, Executor>::
AcceptSyncInternal(AcceptedSocketType& peer_socket, AcceptedEndpointType* peer_endpoint) noexcept -> Result<void>
{  
    switch(this->acceptor_sockstate_)
    {
        case AcceptorSocketState::kListening:
            return this->DoAccept(peer_socket, peer_endpoint);
        [[likely]]case AcceptorSocketState::kBound:
            return this->DoListen()
                .and_then([this, &peer_socket, peer_endpoint](){
                    return this->DoAccept(peer_socket, peer_endpoint);
                });
        case AcceptorSocketState::kAccepting:
            return MakeUnexpected(SockErrorCode::accept_operation_ongoing);
        case AcceptorSocketState::kDisconnected:
            return MakeUnexpected(SockErrorCode::socket_not_bound);
        [[unlikely]]default:
            return MakeUnexpected(SockErrorCode::api_error);;
    }
}


template <typename Protocol, class Executor>
inline auto BasicAcceptorSocket<Protocol, Executor>::
DoListen() noexcept -> Result<void>
{
    return OsAbstraction::Listen(this->GetNativeHandle(), Socket::details::kDefaultListenConnections)
        .map([this](){
            ASRT_LOG_TRACE("[Acceptor]: Listening...");
            this->SetAcceptorSocketState(AcceptorSocketState::kListening);
        });
}

template <typename Protocol, class Executor>
inline auto BasicAcceptorSocket<Protocol, Executor>::
DoAccept(AcceptedSocketType& peer_socket, AcceptedEndpointType* peer_endpoint) noexcept -> Result<void>
{   
    Result<NativeHandleType> accept_result{
        (peer_endpoint == nullptr) ?
            OsAbstraction::AcceptWithoutPeerInfo(Base::GetNativeHandle(), SOCK_NONBLOCK) :
            OsAbstraction::Accept(Base::GetNativeHandle(), peer_endpoint->DataView(), SOCK_NONBLOCK)
    };

    return accept_result
        .and_then([this, &peer_socket](NativeHandleType accepted_sock_handle){
            peer_socket.AssignAcceptedHandle(Base::GetProtocolUnsafe(), accepted_sock_handle);
            return Result<void>{};
        });
}

} //end ns