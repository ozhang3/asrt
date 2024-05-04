#ifndef BDFB44F0_DCC3_4D2A_B6C0_F06452F7508F
#define BDFB44F0_DCC3_4D2A_B6C0_F06452F7508F

#include <cstdint>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <cstring>
#include <memory>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "asrt/socket/socket_base.hpp"
#include "asrt/error_code.hpp"
#include "asrt/sys/syscall.hpp"
#include "asrt/netbuffer.hpp"
#include "asrt/reactor/reactor_interface.hpp"
#include "asrt/socket/types.hpp"
#include "asrt/util.hpp"
#include "expected.hpp"
#include "asrt/common_types.hpp"


namespace Socket{

using namespace Util::Expected_NS;
using namespace Util::Optional_NS;

namespace details{

    enum class BasicSocketState : std::uint8_t
    {
        kClosed = 0u,
        kOpen = 1u,
        kBound = 2u,
        //kConnected = 4u,
        kUndefined = 0xFFu
    };

    inline std::string ToString(const BasicSocketState state) noexcept 
    {
        std::string printable;
        switch (state)
        {
        case BasicSocketState::kClosed:
            printable = "Closed";
            break;
        case BasicSocketState::kOpen:
            printable = "Open";
            break;
        case BasicSocketState::kBound:
            printable = "Bound";
            break;
        [[unlikely]] default:
            printable = "Invalid";
            break;
        }
        return printable;
    }

    inline std::ostream& operator<<(std::ostream& os, const BasicSocketState socket_state)
    {
        os << ToString(socket_state);
        return os;
    } 
}
/**
 * @brief A basic socket opens/closes a socket and accepts an executor for asynchronous operations
 * 
 * @details
 *  Provide the functionality that is common to all socket that exchange data regardless of the used protocol.
 *  In general it provides the functionality for:
 *  - Opening a socket
 *  - Binding a socket to an address
 *  - Changing settings of a socket
 *  - Terminating communication over a socket
 * @warning The socket object is generally NOT thread-safe, in the sense that concurrent calls to socket APIs 
 *  such as Open(), Close(), SendAsync(), RecvAsync() may lead to undefined behavior. Although internal synchronization is guranteed between a socket and its bound executor as they may run on 
 *  different threads, Eexplicit synchronization is needed by applciation code to ensure socket APIs are not called in parallel.
 */
template<
    typename Protocol, 
    class DerivedSocket,
    class Executor>
class BasicSocket : public SocketBase
{
protected:

    /* Class scope using directives */
    using ExecutorType = Executor;
    using Reactor = typename Executor::ReactorType;
    using EndPointType = typename Protocol::Endpoint;
    using AddressType = typename Protocol::AddressType;
    using MutexType = Reactor::MutexType;
    using NativeHandleType = Types::NativeSocketHandleType;
    using Events = ReactorNS::Events;
    using EventType = ReactorNS::EventType;
    using SockErrorCode = ErrorCode_Ns::ErrorCode;
    using BasicSocketState = details::BasicSocketState;
    template <typename T>
    using Result = Expected<T, SockErrorCode>;
    using ReactorHandle = typename Reactor::HandlerTag;

    friend Reactor;

    enum class SocketBlockingMode : std::uint8_t
    {
        kBlocking,
        kNonBlocking,
    };

    /**
     * @brief Default construct a BasicSocket (without associated executor)
     *  
     */
    BasicSocket() noexcept = default;

    /**
     * @brief Construct a BasicSocket from an executor
     * 
     * @param executor 
     */
    explicit BasicSocket(Executor& executor) noexcept : 
        executor_{executor}, reactor_{executor.UseReactorService()}
    {
        ASRT_LOG_TRACE("[BasicSocket]: construction from exeuctor");
    };

    /**
     * @brief Construct a BasicSocket from an optional (nullable) executor
     * 
     * @param executor an optional executor object to pass to the BasicSocket constructor
     */
    explicit BasicSocket(Optional<Executor&> executor) noexcept : 
        executor_{executor}
    {
        if(executor.has_value()){
            ASRT_LOG_TRACE("[BasicSocket]: construction from exeuctor");
            this->reactor_ = executor.value().UseReactorService();
        }else{
            ASRT_LOG_TRACE("[BasicSocket]: construction without exeuctor");
        }
    };

    BasicSocket(BasicSocket const&) = delete;

    /**
     * @warning do not use move ctor. Use MoveSocket() instead 
     */
    BasicSocket(BasicSocket&&) = delete;
    BasicSocket &operator=(BasicSocket const &other) = delete;
    BasicSocket &operator=(BasicSocket &&other) = delete;
    ~BasicSocket() noexcept = default; /* it's derived socket's job to do cleanup (by calling Destroy()) */

    void MoveSocketFrom(BasicSocket&& other) noexcept;


public:

    /**
     * @brief Opens the given socket with provided protocol
     * 
     * @param proto an optional protocol (eg: IPV4, IPV6) the socket should be opened with. 
     *          Defaults to opening with protocol associated with the socket upon creation
     * @note Open()ing an already open socket does not alter its state and will give no error; this is defined behavior
     * @return Result<void> 
     */
    auto Open(const Protocol& proto = Protocol()) noexcept -> Result<void>;
    
    /**
     * @brief Closes the underlying socket and resets any associated socket states
     * 
     * @return Result<void> 
     */
    auto Close() noexcept -> Result<void>;

    /**
     * @brief Retrieve the native socket decriptor associated with this BasicSocket
     * 
     * @return NativeHandleType return -1 for closed sockets
     */
    auto GetNativeHandle() const noexcept -> NativeHandleType {return this->socket_handle_;}

    /**
     * @brief Performs bind() operation on the socket
     * 
     * @param ep The local endpoint to bind to (eg: IP + port number or network interface)
     * @return Result<void> 
     */
    auto Bind(const EndPointType& ep) noexcept -> Result<void>;

    bool HasReactor() const noexcept {return this->reactor_.has_value();}

    auto GetReactor() noexcept -> Optional<Reactor&> & { return this->reactor_; }

    auto GetReactorHandle() const noexcept -> ReactorHandle {return this->reactor_handle_;}

    auto GetReactorUnsafe() noexcept -> Reactor& { return *(this->reactor_); }

    auto GetReactorUnsafe() const noexcept -> const Reactor& { return *(this->reactor_); }

    auto GetLocalEndpoint() const noexcept -> EndPointType;

    auto GetRemoteEndpoint() const noexcept -> EndPointType;

    auto CheckReactorAvailable() const noexcept -> Result<void>;

    auto CheckSocketNonblocking() const noexcept -> Result<void>;

    bool IsInState(BasicSocketState state) const noexcept;

    auto CheckSocketOpen() const noexcept -> Result<void>;

    auto CheckSocketClosed() const noexcept -> Result<void>;

    bool IsNonBlocking() const noexcept {return this->isNonBlocking_;}

    /* no need to check for undefined as setting state to undef will trigger an abort */
    /**
     * @brief Check socket is open (descriptor is valid)
     * 
     * @return true socket descriptor is valid
     * @return false socket descriptor is not valid
     * 
     * @warning NOT thread-safe
     */
    bool IsOpen() const noexcept {return (this->basic_socket_state_ != 0u);}

    bool IsClosed() const noexcept {return this->basic_socket_state_ == 0u;}

    auto SetBlocking() noexcept -> Result<void>;

    auto SetNonBlocking() noexcept -> Result<void>;

    [[deprecated]] /* not implemented */
    bool HasOption() const noexcept {return true;}

    auto GetBasicSocketState(void) const noexcept -> BasicSocketState;

    /**
     * @brief Sets socket option for this socket
     * 
     * @tparam SocketOption A configurable socket option
     * @param option the socket option to set
     * @return Result<void> 
     */
    template <typename SocketOption>
    auto SetOption(SocketOption option) noexcept -> Result<void>;

protected:

    
    auto AssignNativeHandle(const Protocol& protocol, NativeHandleType handle) noexcept -> Result<void>;

    const Protocol& GetProtocolUnsafe() const noexcept
    {
        return this->protocol_.value();
    }

    void Destroy() noexcept;

    Result<void> TryOpenSocket(const Protocol& proto = Protocol{}) noexcept;

    Result<void> CheckAsyncPreconditionsMet() noexcept;

    void AssignNativeHandleInternal(NativeHandleType fd) noexcept {this->socket_handle_ = fd;}

    Result<void> ToggleNonBlockingModeInternal(bool enable) noexcept;

    Result<void> CheckProtocolMatch(const EndPointType& ep) const noexcept  
    {
        return (Protocol{}.Name() == ep.ProtoName()) ?
            Result<void>{} :
            MakeUnexpected(SockErrorCode::protocol_mismatch);
    }

    Result<void> CheckSocketNotAlreadyBound() const noexcept
    {
        return !this->IsInState(BasicSocketState::kBound) ?
            Result<void>{} :
            MakeUnexpected(SockErrorCode::socket_already_bound);
    }

    Result<void> CheckReactorNotExist() const noexcept
    {
        return !this->reactor_.has_value() ?
            Result<void>{} :
            MakeUnexpected(SockErrorCode::socket_already_has_reactor);
    }

    void SetSocketBound() noexcept
    {
        this->SetBasicSockState(BasicSocketState::kBound);
    }

    /**
     * @brief Check that reactor exits and that native socket is in non-blocking mode
     * 
     */
    bool IsAsyncPreconditionsMet() const noexcept
    {
        ASRT_LOG_TRACE("Reactor valid: {}, non-blocking: {}",
            this->reactor_.has_value(), this->IsNonBlocking());
        return this->reactor_.has_value() && this->IsNonBlocking();
    }

    /**
     * @brief Child sockets may post operations for immediate completion to executor through
     *          this function. 
     * @warning Assumes executor is present. Else behavior is undefined.
     * @tparam Operation Must have signature void()
     * @param operation The executable to execute
     */
    template <typename Operation>
    void PostImmediateExecutorJob(Operation&& operation) noexcept
    {
        ASRT_LOG_TRACE("[BasicSocket]: Sockfd {} posting operation for immediate completion",
            this->GetNativeHandle());
        this->executor_.value().EnqueueOnJobArrival(std::move(operation)); //todo consider using a strand?
    }

    /**
     * @brief Toggles reactor monitoring of ready to read (receive) / ready to write (send) events
     * 
     * @param event the type of event observation to modify
     * @param enable true: register for event; false: stop registering for event
     */
    void ChangeReactorObservation(EventType event, bool enable) noexcept //todo
    {
        Result<void> reactor_change_result;
        if(enable){
            reactor_change_result =
                this->GetReactorUnsafe().AddMonitoredEvent(this->reactor_handle_, event);
        }else{
            reactor_change_result =
                this->GetReactorUnsafe().RemoveMonitoredEvent(this->reactor_handle_, event);  
        }

        reactor_change_result.map([enable, event]{
            ASRT_LOG_TRACE("[BasicSocket]: {} reacting to {} event(s)",
                (enable ? "Now" : "Stopped"), Events{event});})
        .map_error([enable, event](SockErrorCode ec){
            ASRT_LOG_ERROR("[BasicSocket]: Failed to {} registration for {} event, {}",
                (enable ? "enable" : "disable"), Events{event}, ec);
        });
    }

    Events GetReactorObservationStatus() const noexcept 
    {
        return this->GetReactorUnsafe()
            .GetObservationStatusUnsafe(this->reactor_handle_);
    }

    bool IsSocketReadableUnsafe() const noexcept 
    {
        return this->GetReactorObservationStatus()
            .HasReadEvent();
    }

    bool IsSocketWriteableUnsafe() const noexcept
    {
        return this->GetReactorObservationStatus()
            .HasWriteEvent();
    }

    void ReadEventConsumed() noexcept
    {
        this->GetReactorUnsafe()
        .ConsumeObservationStatusUnsafe(
            this->reactor_handle_, EventType::kRead);
    }

    void WriteEventConsumed() noexcept
    {
        this->GetReactorUnsafe()
        .ConsumeObservationStatusUnsafe(
            this->reactor_handle_, EventType::kWrite);
    }

    /**
     * @brief Used by derived socket to notify base of start of incoming async send()/connect() operation
     * 
     */
    void AsyncWriteOperationStarted() noexcept
    {
        this->GetReactorUnsafe().OperationStarted(
            this->reactor_handle_, EventType::kWrite);
    }

    /**
     * @brief Used by derived socket to notify base of start of incoming read()/accept() async operation
     * 
     */
    void AsyncReadOperationStarted() noexcept
    {
        this->GetReactorUnsafe().OperationStarted(
            this->reactor_handle_, EventType::kRead);
    }

    /**
     * @brief Used by derived socket to notify reactor of an unhandled reactor event
     * 
     */
    void OnReactorEventIgnored(Events ev) noexcept
    {
        this->GetReactorUnsafe().EventIgnored(
            this->reactor_handle_, ev);
    }

    bool IsReactorHandleValid() const noexcept {return this->reactor_handle_ != ReactorNS::Types::kInvalidHandlerTag;}

    Result<void> RegisterToReactor() noexcept;

    void AcquireLock() noexcept {this->GetMutexUnsafe().lock();}
    void ReleaseLock() noexcept {this->GetMutexUnsafe().unlock();}

    /**
     * @brief Use only if certain that the reactor exits
     * 
     * @return MutexType& 
     */
    MutexType& GetMutex() noexcept
    {
        return this->reactive_socket_service_mtx_ ?
            *(this->reactive_socket_service_mtx_) :
            this->basic_socket_fallback_mtx_;
    }

    /**
     * @brief Use only if reactor exists
     * 
     * @return MutexType& 
     */
    MutexType& GetMutexUnsafe() noexcept
    {
        return *(this->reactive_socket_service_mtx_);
    }

    ReactorHandle reactor_handle_{ReactorNS::Types::kInvalidHandlerTag}; /* event notification handle registered with the reactor */

private:
    using derived = DerivedSocket;
    constexpr derived& Derived() {return static_cast<derived&>(*this);}

    void OnReactorEvent(std::unique_lock<MutexType>& lock, ReactorNS::Events events) noexcept;

    /* mutex to protect concurrent access on socket internal data from 
        async operations and reactor callbacks */
    Optional<Executor&> executor_{};
    Optional<Reactor&> reactor_{};
    MutexType basic_socket_fallback_mtx_;
    MutexType* reactive_socket_service_mtx_{nullptr};
    Optional<Protocol> protocol_{};
    NativeHandleType socket_handle_{asrt::kInvalidNativeHandle};
    bool isNonBlocking_{false};
    std::uint8_t basic_socket_state_{0u}; /* start with closed state */

    void SetBasicSockState(BasicSocketState new_state) noexcept;

    Result<void> DoOpenSocket(const Protocol& proto) noexcept;

    auto MakeReactorEventHandler() noexcept {
        return 
            [this](std::unique_lock<MutexType>& lock, Events ev, ReactorHandle handle) {
                assert(handle == this->reactor_handle_);
                this->OnReactorEvent(lock, ev);
            };
    }
};

template<typename Protocol, class DerivedSocket, class Executor>
inline void BasicSocket<Protocol, DerivedSocket, Executor>::
MoveSocketFrom(BasicSocket&& other) noexcept
{
    static_cast<void>(this->Close());
    if(!this->IsClosed()){
        LogFatalAndAbort("Failed to close original socket when moving from other socket");
    }

    this->socket_handle_ = other.socket_handle_;
    this->reactor_ = other.reactor_; //optiona<&> rebind semantics
    this->reactor_handle_ = other.reactor_handle_;
    this->basic_socket_state_ = other.basic_socket_state_;
    this->isNonBlocking_ = other.isNonBlocking_;

    this->reactor_.value().UpdateRegisteredHandler(
        this->reactor_handle_, this->MakeReactorEventHandler());
}

template<typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
Open(const Protocol& proto) noexcept -> Result<void>
{
    //todo mutex needs to be held !!
    return this->CheckSocketClosed() /* do not allow opening an already-open socket */
        .and_then([this, &proto]() {
            return this->DoOpenSocket(proto); /* sets sock state open here */  
        }); 
}

template<typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
Close() noexcept -> Result<void>
{
    //todo lock needs to be acquired!!!!
    return this->CheckSocketClosed() /* return success if socket already closed */
        .or_else([this](SockErrorCode) -> Result<void> { /* socket is open */
            Derived().OnCloseEvent(); /* notify derived socket of close event (so that it may perform neccesary clean-up if any) */
            if(this->HasReactor()){
                /* de-register handle and close socket asynchronously through reactor if safe/possible */
                return this->GetReactorUnsafe().Deregister(this->reactor_handle_, true)
                    .map([this](){
                        //todo socket may not be actually closed at this point
                        if(not this->GetReactorUnsafe().IsInUse(this->reactor_handle_)){
                            this->SetBasicSockState(BasicSocketState::kClosed);
                            this->socket_handle_ = asrt::kInvalidNativeHandle;
                        } 
                    })
                    .map_error([](SockErrorCode ec){
                        ASRT_LOG_ERROR("Failed to deregister socket during close, {}", ec);
                        return ec;
                    });
            }else{
                return OsAbstraction::Close(this->GetNativeHandle())
                    .map([this](){
                        this->SetBasicSockState(BasicSocketState::kClosed);
                        this->socket_handle_ = asrt::kInvalidNativeHandle;
                    })
                    .map_error([](SockErrorCode ec){
                        ASRT_LOG_ERROR("[BasicSocket]: Failed to close socket, {}", ec);
                        return ec;
                    });
            }
        });
}


template<typename Protocol, class DerivedSocket, class Executor>
inline void BasicSocket<Protocol, DerivedSocket, Executor>::
Destroy() noexcept
{
    //todo mutex needs to be held !!
    ASRT_LOG_TRACE("[BasicSocket]: Destroying socket, sockfd: {}", this->GetNativeHandle());
    if(not this->IsClosed()) [[likely]] {
        if(this->HasReactor()){
            /* de-register handle before closing socket */
            static_cast<void>(this->GetReactorUnsafe().Deregister(this->reactor_handle_, true));
        }else{
            /* man select: "If a file descriptor being monitored by select() is closed in
            another thread ... On Linux (and some other systems), closing the file
            descriptor in another thread has no effect on select()" */
            static_cast<void>(OsAbstraction::Close(this->GetNativeHandle()));
        }
    }
}

template<typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
CheckReactorAvailable() const noexcept -> Result<void>
{
    return this->reactor_.has_value() ?
        Result<void>{} :
        MakeUnexpected(SockErrorCode::reactor_not_available);
}

template<typename Protocol, class DerivedSocket, class Executor>
inline void BasicSocket<Protocol, DerivedSocket, Executor>::
OnReactorEvent(std::unique_lock<MutexType>& lock, ReactorNS::Events events) noexcept
{
    ASRT_LOG_TRACE("[BasicSocket]: Socket fd {} OnReactorEvent", this->GetNativeHandle());
    assert(lock.owns_lock());

    if(this->IsOpen()) [[likely]]
        this->Derived().OnReactorEventImpl(events, lock); /* internalls unlocks and relocks */

    assert(lock.owns_lock()); //check child socket re-locked at function exit
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized" /* gcc erroneously warns against unintialized "result" variable */

template<typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
AssignNativeHandle(const Protocol& protocol, NativeHandleType handle) noexcept -> Result<void>
{
    Result<void> result{};
    this->socket_handle_ = handle;

    if(this->HasReactor() && !this->IsReactorHandleValid()) [[likely]] {
        result = this->RegisterToReactor().map([this]{this->isNonBlocking_ = true;});
    }

    if(result.has_value()) [[likely]] {
        this->SetBasicSockState(BasicSocketState::kOpen);
        this->protocol_.emplace(protocol);
    }else{
        ASRT_LOG_ERROR(
            "[BasicSocket]: Failed to register socket with reactor, {}", result.error());

        OsAbstraction::Close(this->GetNativeHandle())
        .map_error([](SockErrorCode){
            ASRT_LOG_ERROR(
                "[BasicSocket]: Failed to close socket after unsuccessful reactor registration");
        });
    }
    return result;
}

#pragma GCC diagnostic pop

template<typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
GetBasicSocketState(void) const noexcept -> BasicSocketState
{
    if(this->basic_socket_state_ == 0x0u)
        return BasicSocketState::kClosed;
    else if((this->basic_socket_state_ & 0x2u) == 0x2u)
        return BasicSocketState::kBound;
    else if((this->basic_socket_state_ & 0x1u) == 0x1u)
        return BasicSocketState::kOpen;  
    else
        return BasicSocketState::kUndefined; 
}

template<typename Protocol, class DerivedSocket, class Executor>
inline bool BasicSocket<Protocol, DerivedSocket, Executor>::
IsInState(BasicSocketState state) const noexcept 
{
    switch(state)
    {
        case BasicSocketState::kOpen:
        case BasicSocketState::kBound:
            return (this->basic_socket_state_ & static_cast<decltype(this->basic_socket_state_)>(state)) == static_cast<decltype(this->basic_socket_state_)>(state);
        case BasicSocketState::kClosed:
            return this->basic_socket_state_ == static_cast<decltype(this->basic_socket_state_)>(BasicSocketState::kClosed);
        case BasicSocketState::kUndefined:
            return this->basic_socket_state_ == static_cast<decltype(this->basic_socket_state_)>(BasicSocketState::kUndefined);
        [[unlikely]] default:
            ASRT_LOG_WARN("checking invalid state!");
            return false;
    }
}

template<typename Protocol, class DerivedSocket, class Executor>
inline void BasicSocket<Protocol, DerivedSocket, Executor>::
SetBasicSockState(BasicSocketState new_state) noexcept
{
    switch(new_state)
    {
        case BasicSocketState::kOpen:
        case BasicSocketState::kBound:
            this->basic_socket_state_ = (this->basic_socket_state_ | static_cast<decltype(this->basic_socket_state_)>(new_state));
            break;
        case BasicSocketState::kClosed:
        case BasicSocketState::kUndefined:
            this->basic_socket_state_ = static_cast<decltype(this->basic_socket_state_)>(new_state);
            break;
        [[unlikely]] default:
            LogFatalAndAbort("Trying to set invalid state!");
            break;
    }
}

template<typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
CheckSocketOpen() const noexcept -> Result<void>
{
    return this->IsOpen() ?
        Result<void>{} :
        MakeUnexpected(SockErrorCode::socket_not_open);
}

template<typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
CheckSocketClosed() const noexcept -> Result<void>
{
    return this->IsClosed() ?
        Result<void>{} :
        MakeUnexpected(SockErrorCode::socket_already_open);
}

template<typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
TryOpenSocket(const Protocol& proto) noexcept -> Result<void>
{
    return this->CheckSocketOpen()
        .or_else([this, &proto](SockErrorCode){
            ASRT_LOG_TRACE("[BasicSocket]: Trying to open socket...");
            return this->DoOpenSocket(proto); /* sets sock state open here */
        });
}

template<typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
DoOpenSocket(const Protocol& proto) noexcept -> Result<void>
{
    int flags{this->HasReactor() ? 
        (SOCK_CLOEXEC | SOCK_NONBLOCK) : 
        SOCK_CLOEXEC
    };

    return OsAbstraction::Socket(proto, flags)
        .and_then([this, &proto](NativeHandleType sockfd){ 
            return this->AssignNativeHandle(proto, sockfd);
        })
        .map([this](){
            ASRT_LOG_TRACE("[BasicSocket]: Opened sockfd {}", this->GetNativeHandle());
        });
}

template<typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
Bind(const EndPointType& ep) noexcept -> Result<void>
{  
    //todo mutex needs to be held !!
    return this->CheckSocketNotAlreadyBound()
        .and_then([this]() -> Result<void> {
            /* open socket if socket not already open */
            return this->TryOpenSocket();
        })
        .and_then([this, &ep]() -> Result<void> {
            return this->CheckProtocolMatch(ep)
                .and_then([this, &ep]() -> Result<void> {
                    return Derived().BindToEndpointImpl(ep);
                });
        })
        .map([this](){
            this->SetBasicSockState(BasicSocketState::kBound);
        });
}

template<typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
SetBlocking() noexcept -> Result<void>
{
    return this->IsNonBlocking() ?
        this->ToggleNonBlockingModeInternal(false) :
        Result<void>{};
}

template<typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
SetNonBlocking() noexcept -> Result<void>
{
    return this->IsNonBlocking() ?
        Result<void>{} :
        this->ToggleNonBlockingModeInternal(true);
}

template<typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
CheckSocketNonblocking() const noexcept -> Result<void>
{
    return this->IsNonBlocking() ?
        Result<void>{} :
        MakeUnexpected(SockErrorCode::socket_in_blocking_mode);
}

template<typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
ToggleNonBlockingModeInternal(bool enable) noexcept -> Result<void>
{
    return
        this->CheckSocketOpen()
        .and_then([this]() -> Result<NativeHandleType> {
            return OsAbstraction::GetFileControl(this->socket_handle_);
        })
        .and_then([this, enable](int flags) -> Result<int> {
            if((flags & O_NONBLOCK) == static_cast<int>(enable))
                return Result<int>{}; //no need to set as flag already contains option

            flags = enable ?
                flags | O_NONBLOCK :
                flags & ~O_NONBLOCK;

            return OsAbstraction::SetFileControl(this->socket_handle_, flags);
        })
        .map([this, enable](int){
            this->isNonBlocking_ = enable;
        });
}

template<typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
RegisterToReactor() noexcept -> Result<void>
{
    return 
        this->reactor_.value().Register(
            this->socket_handle_, EventType::kReadEdge, /* edge-triggered mode + eager registration for read events */
            this->MakeReactorEventHandler()
        )
        .map([this](const auto& registry) {
            this->reactor_handle_ = registry.tag;
            this->reactive_socket_service_mtx_ = &(registry.mutex);
            ASRT_LOG_TRACE("[BasicSocket]: Sockfd {} registration with reactor success, reactor handle: {}", 
                this->socket_handle_, this->reactor_handle_);
        });
}


template<typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
CheckAsyncPreconditionsMet() noexcept -> Result<void>
{
    return
        this->CheckReactorAvailable()
        .and_then([this](){
            return this->CheckSocketNonblocking();
        });
}

template <typename Protocol, class DerivedSocket, class Executor>
template <typename SocketOption>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
SetOption(SocketOption option) noexcept -> Result<void>
{
    return this->CheckSocketOpen()
        .and_then([this, option](){
            return OsAbstraction::SetSocketOptions(this->socket_handle_, option);
        });
}

template <typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
GetLocalEndpoint() const noexcept -> EndPointType
{
    EndPointType endpoint{};
    Buffer::MutableBufferView address_view{endpoint.data(), endpoint.capacity()};

    if(OsAbstraction::GetSockName(this->socket_handle_, address_view).has_value()){
        endpoint.resize(address_view.size());
    }

    return endpoint;
}

template <typename Protocol, class DerivedSocket, class Executor>
inline auto BasicSocket<Protocol, DerivedSocket, Executor>::
GetRemoteEndpoint() const noexcept ->EndPointType
{
    EndPointType endpoint{};
    Buffer::MutableBufferView address_view{endpoint.data(), endpoint.capacity()};

    if(OsAbstraction::GetPeerName(this->socket_handle_, address_view).has_value()){
        endpoint.resize(address_view.size());
    }

    return endpoint;
}

} //end ns

#endif /* BDFB44F0_DCC3_4D2A_B6C0_F06452F7508F */
