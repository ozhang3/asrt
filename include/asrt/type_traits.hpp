#ifndef B8E8ABDD_A3ED_430E_ADDB_0F3234415CCB
#define B8E8ABDD_A3ED_430E_ADDB_0F3234415CCB

#include <sys/un.h>
#include <type_traits>
#include <linux/if_ether.h>

/* forward declare */
namespace Socket
{
    namespace UnixSock
    {
        template <typename Reactor> class UnixAcceptorSocket;
        template <typename Reactor> class UnixStreamSocket;
        template <typename Reactor> class UnixDgramSocket;
    }

    namespace Types{
        struct ConstGenericSockAddrView;
        struct MutableGenericSockAddrView;
        struct ConstSockAddrView;
        struct MutableSockAddrView;
        struct ConstUnixSockAddrView;
        struct MutableUnixSockAddrView;
        struct ConstPacketSockAddrView;
        struct MutablePacketSockAddrView;    
    }
}

namespace ProtocolNS
{
    struct UnixStream;
    struct UnixDgram;
    struct PacketRaw;
    struct PacketDgram;
    struct TCP;
    struct UDP;
}

namespace Buffer
{
    class MutableBufferView;
    class ConstBufferView;
}

namespace SocketTraits
{

    template <typename SocketType>
    struct is_stream_based : std::false_type {};

    template <typename Reactor>
    struct is_stream_based<Socket::UnixSock::UnixAcceptorSocket<Reactor>> : std::true_type {};

    template <typename Reactor>
    struct is_stream_based<Socket::UnixSock::UnixStreamSocket<Reactor>> : std::true_type {};

    template <typename Reactor>
    struct is_stream_based<Socket::UnixSock::UnixDgramSocket<Reactor>> : std::false_type {};

}

namespace ProtocolTraits
{
    template <typename ProtocolType>
    struct is_valid : std::false_type {};

    template <typename ProtocolType>
    struct is_stream_based : std::false_type {};

    template <typename ProtocolType>
    struct is_internet_domain : std::false_type {};

    template <typename ProtocolType>
    struct is_unix_domain : std::false_type {};

    template<typename ProtocolType>
    struct is_packet_level : std::false_type {};

    template <>
    struct is_packet_level<ProtocolNS::PacketRaw> : std::true_type {};

    template <>
    struct is_packet_level<ProtocolNS::PacketDgram> : std::true_type {};

    template <>
    struct is_stream_based<ProtocolNS::TCP> : std::true_type {};

    template <>
    struct is_stream_based<ProtocolNS::UnixStream> : std::true_type {};

    template <>
    struct is_internet_domain<ProtocolNS::TCP> : std::true_type {};

    template <>
    struct is_internet_domain<ProtocolNS::UDP> : std::true_type {};

    template <>
    struct is_unix_domain<ProtocolNS::UnixStream> : std::true_type {};

    template <>
    struct is_unix_domain<ProtocolNS::UnixDgram> : std::true_type {};

    template <>
    struct is_valid<ProtocolNS::TCP> : std::true_type {};

    template <>
    struct is_valid<ProtocolNS::UDP> : std::true_type {};

    template <>
    struct is_valid<ProtocolNS::UnixStream> : std::true_type {};

    template <>
    struct is_valid<ProtocolNS::UnixDgram> : std::true_type {};

    template <>
    struct is_valid<ProtocolNS::PacketRaw> : std::true_type {};

    template <>
    struct is_valid<ProtocolNS::PacketDgram> : std::true_type {};

}

namespace SocketAddressViewTraits{

    template <typename SocketAddressView>
    struct is_valid : std::false_type {};

    template <typename SocketAddressView>
    struct is_mutable : std::false_type {};

    template <>
    struct is_valid<Socket::Types::ConstGenericSockAddrView> : std::true_type {};

    template <>
    struct is_valid<Socket::Types::MutableGenericSockAddrView> : std::true_type {};

    template <>
    struct is_valid<Socket::Types::ConstSockAddrView> : std::true_type {};

    template <>
    struct is_valid<Socket::Types::MutableSockAddrView> : std::true_type {};

    template <>
    struct is_valid<Socket::Types::ConstUnixSockAddrView> : std::true_type {};

    template <>
    struct is_valid<Socket::Types::MutableUnixSockAddrView> : std::true_type {};

    template <>
    struct is_valid<Socket::Types::MutablePacketSockAddrView> : std::true_type {};

    template <>
    struct is_valid<Socket::Types::ConstPacketSockAddrView> : std::true_type {};

    template <>
    struct is_mutable<Socket::Types::ConstGenericSockAddrView> : std::false_type {};

    template <>
    struct is_mutable<Socket::Types::MutableGenericSockAddrView> : std::true_type {};

    template <>
    struct is_mutable<Socket::Types::ConstSockAddrView> : std::false_type {};

    template <>
    struct is_mutable<Socket::Types::MutableSockAddrView> : std::true_type {};

    template <>
    struct is_mutable<Socket::Types::ConstUnixSockAddrView> : std::false_type {};

    template <>
    struct is_mutable<Socket::Types::MutableUnixSockAddrView> : std::true_type {};


}

namespace SocketAddressTraits{

    template <typename SocketAddressView>
    struct is_valid : std::false_type {};

    template <>
    struct is_valid<struct sockaddr> : std::true_type {};

    template <>
    struct is_valid<struct sockaddr_storage> : std::true_type {};

    template <>
    struct is_valid<struct sockaddr_un> : std::true_type {};


}

namespace BufferViewTraits{

    template <typename BufferView>
    struct is_valid : std::false_type {};

    template <typename BufferView>
    struct is_mutable : std::false_type {};

    template <>
    struct is_valid<Buffer::ConstBufferView> : std::true_type {};

    template <>
    struct is_valid<Buffer::MutableBufferView> : std::true_type {};

    template <>
    struct is_mutable<Buffer::ConstBufferView> : std::false_type {};

    template <>
    struct is_mutable<Buffer::MutableBufferView> : std::true_type {};

}


namespace ServiceTraits{

    template <typename Service>
    struct is_valid : std::false_type {};

    
    //template <>
    //struct is_valid<ReactorNS::EpollReactor> : std::true_type {};

}

namespace asrt{

template<typename F, typename Signature>
struct is_signature_match;

template <typename F, typename R, typename... Args>
struct is_signature_match<F, R(Args...)> : std::false_type {};

template <typename F, typename R, typename... Args>
    requires std::is_invocable_r_v<R, F, Args...>
struct is_signature_match<F, R(Args...)> : std::true_type {};

template<typename F, typename S>
inline constexpr bool is_signature_match_v = is_signature_match<F, S>::value;

template<typename Func, typename Signature>
concept MatchesSignature = is_signature_match_v<Func, Signature>;

template<typename V>
concept ViewLike = requires (V v) 
    {
        v.data();
        v.size();
    };

// typedef void (*fv) (int);
// static_assert(MatchesSignature<fv, void(int)>);

}






#endif /* B8E8ABDD_A3ED_430E_ADDB_0F3234415CCB */
