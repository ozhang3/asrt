#ifndef BB654D12_3FB5_46D6_A4B0_F42DC3240883
#define BB654D12_3FB5_46D6_A4B0_F42DC3240883

#include <cstdint>

#include "asrt/common_types.hpp"
#include "asrt/util.hpp"
#include "asrt/error_code.hpp"
#include "asrt/socket/basic_socket.hpp"
#include "asrt/reactor/reactor_interface.hpp"
#include "asrt/socket/types.hpp"
#include "asrt/sys/syscall.hpp"
#include "asrt/type_traits.hpp"

namespace Socket{

using namespace Util::Expected_NS;
using namespace Util::Optional_NS;

namespace details
{
    static constexpr int kDefaultListenConnections{16};
    static_assert(kDefaultListenConnections < SocketBase::kMaxListenConnections, 
        "default max connection backlog execeeded");

    enum class AcceptorSocketState : std::uint8_t
    {
        kDisconnected,
        kBound,
        kListening,
        kAccepting
    };

    inline auto ToString(const AcceptorSocketState state) -> std::string
    {
        std::string printable;
        switch (state)
        {
        case AcceptorSocketState::kDisconnected:
            printable = "Disconnected";
            break;
        case AcceptorSocketState::kBound:
            printable = "Bound";
            break;
        case AcceptorSocketState::kListening:
            printable = "Listening";
            break;
        case AcceptorSocketState::kAccepting:
            printable = "Accepting";
            break;
        [[unlikely]] default:
            printable = "Invalid";
            break;
        }
        return printable;
    }

    inline std::ostream& operator<<(std::ostream& os, const AcceptorSocketState socket_state)
    {
        os << ToString(socket_state);
        return os;
    }
}

template <
    typename Protocol,
    class Executor>
class BasicAcceptorSocket final : 
    public BasicSocket<
        Protocol, 
        BasicAcceptorSocket<Protocol, Executor>,
        Executor>
{
    static_assert(ProtocolTraits::is_stream_based<Protocol>::value, "Invalid protocol used. Stream based protocol only!");
public:

    /* Class scope using directives */
    using NativeHandleType = asrt::NativeHandle;
    using Base = BasicSocket<Protocol, BasicAcceptorSocket, Executor>;
    using BoundEndpointType = typename Protocol::Endpoint;
    using AcceptedEndpointType = typename Protocol::Endpoint;
    using typename Base::SockErrorCode;
    using AcceptedSocketType = typename Protocol::template DataTransferSocketType<Executor>;
    using Events = ReactorNS::Events;
    using typename Base::AddressType;
    using typename Base::BasicSocketState;
    using typename Base::Reactor;
    using typename Base::EventType;
    using typename Base::MutexType;
    using AcceptorSocketState = Socket::details::AcceptorSocketState;
    template <typename T> using Result = typename Base::template Result<T>;
    
    struct AcceptedPeerInfo{
        AcceptedEndpointType peer_endpoint_;
        AcceptedSocketType peer_socket_;
    };

    using AcceptCompletionHandler = std::function<void(Result<void>&&)>;

    enum class AcceptorOptions : std::uint8_t
    {
        kNone,
        kReuseAddress
    };

    /* @brief A default constructed socket can NOT perform socket operations without first being Open()ed
       @brief A default constructed socket can NOT perform asynchronous socket operations
    */
    BasicAcceptorSocket() noexcept = default;
    explicit BasicAcceptorSocket(Executor& executor) noexcept : Base{executor} {};
    //explicit BasicAcceptorSocket(Reactor& reactor) noexcept : Base(reactor) {};
    BasicAcceptorSocket(Executor& executor, const BoundEndpointType& endpoint, 
        AcceptorOptions options = AcceptorOptions::kReuseAddress) noexcept;
    BasicAcceptorSocket(BasicAcceptorSocket const&) = delete;
    BasicAcceptorSocket(BasicAcceptorSocket&& other) noexcept;
    BasicAcceptorSocket &operator=(BasicAcceptorSocket const &other) = delete;
    BasicAcceptorSocket &operator=(BasicAcceptorSocket &&other) = delete;
    ~BasicAcceptorSocket() noexcept
    {
        Base::Destroy();     
    }

    auto Listen(void) noexcept -> Result<void>;

    auto Accept(AcceptedSocketType& peer_socket) noexcept -> Result<void>;

    auto Accept(AcceptedSocketType& peer_socket, AcceptedEndpointType& peer_endpoint) noexcept -> Result<void>;

    //todo provide AcceptAsync overload for when only handler is passed. Handler will be provided with a new socket.

    template <typename AcceptCompletionCallback>
    auto AcceptAsync(
        AcceptedSocketType& peer_socket, 
        AcceptCompletionCallback&& accept_handler) noexcept -> Result<void>;
    
    template <typename AcceptCompletionCallback>
    auto AcceptAsync(
        AcceptedSocketType& peer_socket, 
        AcceptedEndpointType& peer_endpoint, 
        AcceptCompletionCallback&& accept_handler) noexcept -> Result<void>;

    void OnReactorEventImpl(Events events, std::unique_lock<MutexType>& lock) noexcept;

    Result<void> BindToEndpointImpl(const AcceptedEndpointType& ep) noexcept;

    void SetAcceptorSocketState(AcceptorSocketState state) noexcept { this->acceptor_sockstate_ = state; }

    void OnCloseEvent() noexcept;

    AcceptorSocketState GetAcceptorSocketState() const noexcept { return this->acceptor_sockstate_; }

    friend std::ostream& operator<<(std::ostream& os, const BasicAcceptorSocket& acceptor)
    {
        os << "[Socket type: " << "Acceptor" 
            << ", protocol: " << Protocol{}
            << ", socket fd: " << acceptor.GetNativeHandle()
            << ", socket state: " << acceptor.GetAcceptorSocketState()
            << ", blocking: " << std::boolalpha << !acceptor.IsNonBlocking() << "]";
        return os;
    }
 
private:

    Result<void> AcceptSyncInternal(AcceptedSocketType& peer_socket, AcceptedEndpointType* peer_endpoint) noexcept;
    
    template <typename AcceptCompletionCallback>
    Result<void> AcceptAsyncInternal(
        AcceptedSocketType& peer_socket, 
        AcceptedEndpointType* peer_endpoint, 
        AcceptCompletionCallback&& accept_handler) noexcept;  

    Result<void> DoListen() noexcept;

    Result<AcceptedPeerInfo> DoAccept(Optional<Reactor&> reactor) noexcept;

    Result<void> DoAccept(AcceptedSocketType& peer_socket, AcceptedEndpointType* peer_endpoint) noexcept;

    template <typename AcceptCompletionCallback>
    void DoAsyncAccept(AcceptedSocketType& peer_socket, AcceptedEndpointType* peer_endpoint, AcceptCompletionCallback&& callback) noexcept;

    void HandleAysncAccept(std::unique_lock<MutexType>& lock) noexcept;

    AcceptorSocketState acceptor_sockstate_{AcceptorSocketState::kDisconnected};
    AcceptCompletionHandler on_accept_complete_{};
    AcceptedSocketType* peer_socket_{nullptr};
    AcceptedEndpointType* peer_info_{nullptr};
    bool speculative_accept_{false};
};

template <typename Protocol, class Executor>
template <typename AcceptCompletionCallback>
inline auto BasicAcceptorSocket<Protocol, Executor>::
AcceptAsync(AcceptedSocketType& peer_socket, AcceptCompletionCallback&& accept_handler) noexcept -> Result<void>
{
    std::scoped_lock const lock{Base::GetMutexUnsafe()};

    assert(Base::IsAsyncPreconditionsMet());

    return this->AcceptAsyncInternal(peer_socket, nullptr, std::move(accept_handler));
}

template <typename Protocol, class Executor>
template <typename AcceptCompletionCallback>
inline auto BasicAcceptorSocket<Protocol, Executor>::
AcceptAsync(
    AcceptedSocketType& peer_socket, 
    AcceptedEndpointType& peer_endpoint, 
    AcceptCompletionCallback&& accept_handler) noexcept -> Result<void>
{
    std::scoped_lock const lock{Base::GetMutexUnsafe()};

    assert(Base::IsAsyncPreconditionsMet());

    return this->AcceptAsyncInternal(peer_socket, &peer_endpoint, std::move(accept_handler));
}

template <typename Protocol, class Executor>
template <typename AcceptCompletionCallback>
inline void BasicAcceptorSocket<Protocol, Executor>::
DoAsyncAccept(
    AcceptedSocketType& peer_socket, 
    AcceptedEndpointType* peer_endpoint, 
    AcceptCompletionCallback&& callback) noexcept
{
    bool async_needed{false};
    Result<void> accept_result;

    if(this->speculative_accept_) [[unlikely]] {
        ASRT_LOG_TRACE("Speculative accept");
        accept_result = this->DoAccept(peer_socket, peer_endpoint);
        if(!accept_result.has_value() && 
            ErrorCode_Ns::IsBusy(accept_result.error())) [[unlikely]] {
                async_needed = true;
        }
        this->speculative_accept_ = false;
    }else [[likely]] {
        async_needed = true;
    }

    if(async_needed) [[likely]] {
        this->peer_socket_ = &peer_socket;
        this->peer_info_ = peer_endpoint;
        this->on_accept_complete_ = std::move(callback);
        ASRT_LOG_TRACE("[Acceptor]: Started async accept");
        Base::AsyncReadOperationStarted(); 
    }else{
        ASRT_LOG_TRACE("Posting accept handler for immediate completion.");
        Base::PostImmediateExecutorJob(
            [callback = std::move(callback), result = std::move(accept_result)]() mutable {
                callback(std::move(result));
            });
    }
}

template <typename Protocol, class Executor>
template <typename AcceptCompletionCallback>
inline auto BasicAcceptorSocket<Protocol, Executor>::
AcceptAsyncInternal(
    AcceptedSocketType& peer_socket, 
    AcceptedEndpointType* peer_endpoint, 
    AcceptCompletionCallback&& accept_handler) noexcept -> Result<void>
{    
    switch(this->acceptor_sockstate_)
    {
        case AcceptorSocketState::kListening:
            this->SetAcceptorSocketState(AcceptorSocketState::kAccepting);
            this->DoAsyncAccept(peer_socket, peer_endpoint, std::move(accept_handler));
            return Result<void>{};
        [[likely]]case AcceptorSocketState::kBound:
            return this->DoListen()
                .map([this, &peer_socket, peer_endpoint, &accept_handler](){
                    ASRT_LOG_TRACE("[Acceptor]: Listen success");
                    this->SetAcceptorSocketState(AcceptorSocketState::kAccepting);
                    this->DoAsyncAccept(peer_socket, peer_endpoint, std::move(accept_handler));
                });
        [[unlikely]]case AcceptorSocketState::kAccepting:
            return MakeUnexpected(SockErrorCode::accept_operation_ongoing);
        [[unlikely]]case AcceptorSocketState::kDisconnected:
            return MakeUnexpected(SockErrorCode::socket_not_bound); 
        [[unlikely]] default:
            ASRT_LOG_WARN("[Acceptor]: Unrecognized acceptor socket state!");
            return MakeUnexpected(SockErrorCode::api_error);
    }
}

}//end ns

#if defined(ASRT_HEADER_ONLY)
# include "asrt/impl/basic_acceptor_socket.ipp"
#endif // defined(ASRT_HEADER_ONLY)

#endif /* BB654D12_3FB5_46D6_A4B0_F42DC3240883 */
