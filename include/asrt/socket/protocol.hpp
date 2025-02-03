#ifndef B829CA35_48F3_416E_9D8A_04433BA076D9
#define B829CA35_48F3_416E_9D8A_04433BA076D9

#include <cstdint>
#include <sys/socket.h>
#include <string>
#include <iostream>
#include <linux/if_ether.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "asrt/type_traits.hpp"
#include "asrt/concepts.hpp"

namespace Unix{
    template <typename Protocol>
    class UnixDomainEndpoint;
}

namespace IP{
    template <InternetDomain Protocol>
    class BasicEndpoint;
}
namespace Endpoint_NS{
    template <typename Protocol>
    class PacketEndpoint;
}

namespace Socket{
    template <typename Protocol, typename Executor>
    class BasicStreamSocket;
    
    template <typename Protocol, typename Executor>
    class BasicDgramSocket;
    
    template <typename Protocol, typename Executor>
    class BasicAcceptorSocket;
}

namespace ProtocolNS{

enum class ProtoType : std::uint8_t
{
    invalid = 0u,
    tcp = 1u,
    udp = 2u,
    unix_stream = 3u,
    unix_dgram = 4u,
    packet_raw = 5u,
    packet_dgram = 6u,
    proto_max
};

inline auto MapProtoTypeToString(ProtoType proto) -> std::string
{
    std::string printable;
    switch(proto)
    {
        case ProtoType::tcp:
            printable = "TCP";
            break;
        case ProtoType::udp:
            printable = "UDP";
            break;
        case ProtoType::unix_stream:
            printable = "UNIX_STREAM";
            break;
        case ProtoType::unix_dgram:
            printable = "UNIX_DATAGRAM";
            break;
        case ProtoType::packet_raw:
            printable = "PACKET_RAW";
            break;
        case ProtoType::packet_dgram:
            printable = "PACKET_DATAGRAM";
            break;
        [[unlikely]] default:
            printable = "Invalid Protocol";
            break;            
    }
    return printable;
}

/* support for unix_stream & unix_dgram only */
template<typename UnderlyingProto>
class Protocol
{
public:
    using ProtoName = ProtoType;
    //using Address = typename UnderlyingProto::AddressType;

    constexpr int Family() const {return self().family_;}
    constexpr int GetType() const {return self().type_;}
    constexpr int ProtoNumber() const {return self().proto_num_;}
    constexpr ProtoType Name() const {return self().name_;}

private:
    using Self = UnderlyingProto;
    constexpr const Self& self() const {return static_cast<const Self&>(*this);}
};

template<typename UnderlyingProto>
inline std::ostream& operator<<(std::ostream& os, const Protocol<UnderlyingProto>& proto) noexcept
{
    os << "[Name]: " << MapProtoTypeToString(proto.Name()) << " "
       << "[Family]: " << proto.Family() << " "
       << "[Type]: " << proto.GetType() << " "
       << "[ProtoNum]: " << proto.ProtoNumber() << "\n";
    return os;
}

/* Protocol Unix Stream */
struct UnixStream : Protocol<UnixStream>
{
    using AddressType = const char*;
    using Endpoint = Unix::UnixDomainEndpoint<UnixStream>;

    template <typename Executor>
    using DataTransferSocketType = Socket::BasicStreamSocket<UnixStream, Executor>;

    template <typename Executor>
    using AcceptorType = Socket::BasicAcceptorSocket<UnixStream, Executor>;

    static constexpr int family_{AF_UNIX};
    static constexpr int type_{SOCK_STREAM};
    static constexpr int proto_num_{0}; /* let kernel choose appropriate protocol */
    static constexpr ProtoType name_{ProtoType::unix_stream};
};

/* Protocol Unix Datagram */
struct UnixDgram : Protocol<UnixDgram>
{
    using AddressType = const char*; 
    using Endpoint = Unix::UnixDomainEndpoint<UnixDgram>;

    template <typename Executor>
    using SocketType = Socket::BasicDgramSocket<UnixDgram, Executor>;

    static constexpr int family_{AF_UNIX};
    static constexpr int type_{SOCK_DGRAM};
    static constexpr int proto_num_{0}; /* let kernel choose appropriate protocol */
    static constexpr ProtoType name_{ProtoType::unix_dgram};
};

/* Protocol Packet Raw */
struct PacketRaw : Protocol<PacketRaw>
{
    using AddressType = const char*; 
    using Endpoint = Endpoint_NS::PacketEndpoint<PacketRaw>;
    //using SocketType = Socket::PacketSocket::BasicPacketSocekt<PacketRaw>;

    static constexpr int family_{AF_PACKET};
    static constexpr int type_{SOCK_RAW};
    
    /* The protocol is set to 0.  This means we will receive no
	 * packets until we "bind" the socket with a non-zero
	 * protocol.  This allows us to setup the ring buffers without
	 * dropping any packets. */
    static constexpr int proto_num_{0}; /* receive no packets upon socket creation; get proto num from endpoint */
    static constexpr ProtoType name_{ProtoType::packet_raw};
};

/* Protocol Packet Datagrtam */
struct PacketDgram : Protocol<PacketDgram>
{
    using AddressType = const char*; 
    using Endpoint = Endpoint_NS::PacketEndpoint<PacketDgram>;
    //using SocketType = Socket::PacketSocket::BasicPacketSocekt<PacketDgram>;

    static constexpr int family_{AF_UNIX};
    static constexpr int type_{SOCK_DGRAM};
    static constexpr int proto_num_{0}; /* receive no packets upon socket creation; get proto num from endpoint */   
    static constexpr ProtoType name_{ProtoType::packet_dgram};
};

/* Protocol tcp */
struct TCP : Protocol<TCP>
{
    using Endpoint = IP::BasicEndpoint<TCP>;
    using AddressType = const char*;

    template <typename Executor>
    using DataTransferSocketType = Socket::BasicStreamSocket<TCP, Executor>;

    template <typename Executor>
    using AcceptorType = Socket::BasicAcceptorSocket<TCP, Executor>;

    constexpr explicit TCP() = default;
    /**
     * @param protocol_family AF_INET / AF_INET6
    */
    constexpr explicit TCP(int protocol_family = AF_INET) noexcept : 
        family_{protocol_family} {}

    int family_{AF_INET};
    static constexpr int type_{SOCK_STREAM};
    static constexpr int proto_num_{IPPROTO_TCP};
    static constexpr ProtoType name_{ProtoType::tcp};
};

/* Protocol udp */
struct UDP : Protocol<UDP>
{
    using Endpoint = IP::BasicEndpoint<UDP>;
    using AddressType = const char*;
    constexpr explicit UDP(int protocol_family = AF_INET) noexcept 
        : family_{protocol_family} {}

    int family_;
    static constexpr int type_{SOCK_DGRAM};
    static constexpr int proto_num_{IPPROTO_UDP}; /* receive no packets upon socket creation; get proto num from endpoint */   
    static constexpr ProtoType name_{ProtoType::udp};
};

// template <typename Protocol>
// inline constexpr std::string_view ToStringView(Protocol const& protocol) noexcept
// {
//     if constexpr
// }

} //end ns ProtocolNS

// template<typename Protocol>
// struct fmt::formatter<Endpoint_NS::PacketEndpoint<Protocol>> : fmt::formatter<std::string_view>
// {
//     auto format(const Endpoint_NS::PacketEndpoint<Protocol>& ep, format_context &ctx) const -> decltype(ctx.out())
//     {
//         return format_to(ctx.out(), "{}", ep.ToStringView());
//     }
// };

#endif /* B829CA35_48F3_416E_9D8A_04433BA076D9 */
