#ifndef F33AF3B3_329B_414E_A871_DF327079C4C2
#define F33AF3B3_329B_414E_A871_DF327079C4C2

#include "asrt/basic_datagram_socket.hpp"
#include "asrt/basic_stream_socket.hpp"
#include "asrt/protocol.hpp"
#include "asrt/acceptor.hpp"
#include "asrt/types.hpp"
#include "asrt/reactor/epoll_reactor.hpp"
#include <type_traits>

namespace Socket
{
    namespace UnixSock
    {
        #if 0
        /* forward declarations */
        template <typename Reactor> class UnixDgramSocket;
        template <typename Reactor> class UnixStreamSocket;
        template <typename Reactor> class UnixAcceptorSocket;

        using namespace Util::Expected_NS;
        using SockErrorCode = ErrorCode_Ns::ErrorCode;
        //using BasicStreamSock = BasicStreamSocket<ProtocolNS::UnixStream>;
        //using BasicDgramSock = DatagramSocket::BasicDgramSocket<ProtocolNS::UnixDgram>;
        //using BasicAcceptorSock = Acceptor_NS::BasicAcceptorSocket<ProtocolNS::UnixStream>;

        inline auto BindSocketToPath(Types::NativeSocketHandleType sockfd, const char *path) -> Expected<void, SockErrorCode>
        {
            ASRT_LOG_TRACE("binding directly to path...");
            auto addr{Types::MakeUnixSockAddr(path)};
            return OsAbstraction::Bind(sockfd, Types::ConstUnixSockAddrView{&addr});
        }

        inline auto ConnectSocketToPath(Types::NativeSocketHandleType sockfd, const char *path) -> Util::Expected_NS::Expected<void, SockErrorCode>
        {
            ASRT_LOG_TRACE("connecting directly to path...");
            return OsAbstraction::Connect(sockfd, Types::MakeUnixSockAddr(path));
        }

        template <typename Reactor>
        class UnixDgramSocket final : 
            public DatagramSocket::BasicDgramSocket<ProtocolNS::UnixDgram, UnixDgramSocket<Reactor>, Reactor>
        {
            template <typename T, typename E>
            using Expected = Util::Expected_NS::Expected<T, E>;
            using Protocol = ProtocolNS::UnixStream;
            using Base = DatagramSocket::BasicDgramSocket<Protocol, UnixDgramSocket<Reactor>, Reactor>;
            using typename Base::Executor;

        public:
            /* @brief A default constructed socket can NOT perform socket operations without first being Open()ed */
            UnixDgramSocket() noexcept = default;
            explicit UnixDgramSocket(Executor& executor) noexcept : Base(executor) {};
            explicit UnixDgramSocket(Reactor& reactor) noexcept : Base(reactor) {};
            UnixDgramSocket(UnixDgramSocket const&) = delete;
            UnixDgramSocket(UnixDgramSocket&& other) : Base{std::move(other)} {};
            UnixDgramSocket &operator=(UnixDgramSocket const &other) = delete;
            UnixDgramSocket &operator=(UnixDgramSocket &&other) = delete;
            ~UnixDgramSocket() noexcept = default;

            auto BindToAddrImpl_lv1(const char *path) -> Expected<void, SockErrorCode>
            {
                return BindSocketToPath(this->GetNativeHandle(), path);
            }

            auto ConnectToAddrImpl(const char* path) -> Expected<void, SockErrorCode>
            {
                return ConnectSocketToPath(this->GetNativeHandle(), path);
            }

        };

        template <typename Reactor>
        class UnixStreamSocket final : 
            public BasicStreamSocket<ProtocolNS::UnixStream, UnixStreamSocket<Reactor>, Reactor>
        {
            template <typename T, typename E>
            using Expected = Util::Expected_NS::Expected<T, E>;
            using Protocol = ProtocolNS::UnixStream;
            using Base = BasicStreamSocket<Protocol, UnixStreamSocket<Reactor>, Reactor>;
            using typename Base::SockErrorCode;
            using typename Base::Executor;

        public:
            /* @brief A default constructed socket can NOT perform socket operations without first being Open()ed */
            UnixStreamSocket() noexcept = default;
            explicit UnixStreamSocket(Executor& executor) noexcept : Base(executor) {};
            explicit UnixStreamSocket(Reactor& reactor) noexcept : Base(reactor) {};
            UnixStreamSocket(UnixStreamSocket const&) = delete;
            UnixStreamSocket(UnixStreamSocket&& other) : Base{std::move(other)} {};
            UnixStreamSocket &operator=(UnixStreamSocket const &other) = delete;
            UnixStreamSocket &operator=(UnixStreamSocket &&other) = delete;
            ~UnixStreamSocket() noexcept = default;

            auto BindToAddrImpl_lv1(const char *path) -> Expected<void, SockErrorCode>
            {
                return BindSocketToPath(this->GetNativeHandle(), path);
            }

            
            auto ConnectToAddrImpl(const char* path) -> Expected<void, SockErrorCode>
            {
                return ConnectSocketToPath(this->GetNativeHandle(), path);
            }

        };

        template <typename Reactor>
        class UnixAcceptorSocket final : 
            public Acceptor_NS::BasicAcceptorSocket<ProtocolNS::UnixStream, UnixAcceptorSocket<Reactor>, Reactor>
        {
            template <typename T, typename E>
            using Expected = Util::Expected_NS::Expected<T, E>;
            using Protocol = ProtocolNS::UnixStream;
            using Base = Acceptor_NS::BasicAcceptorSocket<Protocol, UnixAcceptorSocket<Reactor>, Reactor>;
            using AcceptedSocketType = UnixStreamSocket<Reactor>;
            using typename Base::SockErrorCode;
            using typename Base::Executor;

            static_assert(std::is_same<AcceptedSocketType, typename Base::AcceptedSocketType>::value, 
                "Accepted socket type mismatch. Possibly wrong template protocol parameter was used during class instantiation.");


        public:

            /* @brief A default constructed socket can NOT perform socket operations without first being Open()ed */
            UnixAcceptorSocket() noexcept = default;
            explicit UnixAcceptorSocket(Executor& executor) noexcept : Base(executor) {};
            explicit UnixAcceptorSocket(Reactor& reactor) noexcept : Base(reactor) {};
            UnixAcceptorSocket(UnixAcceptorSocket const&) = delete;
            UnixAcceptorSocket(UnixAcceptorSocket&& other) noexcept : Base{std::move(other)} {};
            UnixAcceptorSocket &operator=(UnixAcceptorSocket const &other) = delete;
            UnixAcceptorSocket &operator=(UnixAcceptorSocket &&other) = delete;
            ~UnixAcceptorSocket() noexcept = default;
            
            auto BindToAddrImpl_lv1(const char *path) -> Expected<void, SockErrorCode>
            {
                return BindSocketToPath(this->GetNativeHandle(), path);
            }

        };

        #endif
    }
}

#endif /* F33AF3B3_329B_414E_A871_DF327079C4C2 */
