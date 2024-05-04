#ifndef B0FD1DF0_9960_4684_A2AB_EFA1DF150260
#define B0FD1DF0_9960_4684_A2AB_EFA1DF150260

#include <cstdint>
#include <cstring>
#include <cassert>
#include <array>
#include <sys/socket.h>
#include <sys/un.h>
#include <string>
#include <sstream>
#include <iostream>

#include "asrt/netbuffer.hpp"
#include "asrt/socket/address.hpp"
#include "asrt/socket/types.hpp"
#include "asrt/concepts.hpp"

namespace ProtocolNS
{
    enum class ProtoType : std::uint8_t;
};

namespace IP{

using namespace Socket::Types;
using ProtocolNS::ProtoType;

/**
 * @brief Implements IP Address + port number abstraction 
 *          which servers can bind to and clients can connect to
 * 
 * @tparam Protocol: Internet domain protocols such as TCP/UDP
 */
template<InternetDomain Protocol>
class BasicEndpoint
{
public:
    using ProtocolType = Protocol;
    using SockAddrBase = ::sockaddr;
    using SockAddrV4 = ::sockaddr_in;
    using SockAddrV6 = ::sockaddr_in6;
    using PortNumber = unsigned short;

    constexpr BasicEndpoint() noexcept = default;
    constexpr BasicEndpoint(const Protocol& proto, PortNumber port_num) noexcept;
    constexpr BasicEndpoint(const IP::AddressV4& addr, PortNumber port_num) noexcept;
    constexpr BasicEndpoint(const IP::AddressV6& addr, PortNumber port_num) noexcept;
    constexpr BasicEndpoint(const IP::Address& addr, PortNumber port_num) noexcept;
    BasicEndpoint(BasicEndpoint const& other) noexcept = default;
    BasicEndpoint(BasicEndpoint&&) noexcept = default;
    BasicEndpoint &operator=(BasicEndpoint const &other) noexcept = default;
    BasicEndpoint &operator=(BasicEndpoint &&other) noexcept = default;
    constexpr ~BasicEndpoint() noexcept = default;

    SockAddrType* data() noexcept { return &this->addr_.base_; }
    const SockAddrType* data() const noexcept { return &this->addr_.base_; }

    std::size_t capacity() const noexcept {return sizeof(addr_);}

    void resize(std::size_t new_size) const noexcept {}

    constexpr bool operator==(BasicEndpoint const &other) const noexcept
    {
        return this->Address() == other.Address() && this->Port() == other.Port();
    }
    
    constexpr auto operator<=>(BasicEndpoint const &other) const noexcept
    {
        if(auto res{this->Address() <=> other.Address()}; res != 0)
            return res;
        return this->Port() <=> other.Port();
    }

    bool IsV4() const noexcept {return addr_.base_.sa_family == AF_INET;}
    bool IsV6() const noexcept {return addr_.base_.sa_family == AF_INET6;}

    std::size_t size() const noexcept {return IsV4() ? sizeof(SockAddrV4) : sizeof(SockAddrV6);}
    auto Port() const noexcept;
    auto DataView() const noexcept -> Buffer::ConstBufferView;
    auto DataView() noexcept -> Buffer::MutableBufferView;
    auto Address() const noexcept -> IP::Address;
    auto ToString() const noexcept -> std::string;

    //constexpr auto Protocol() const -> Protocol {return Protocol{};}
    constexpr auto Family() const noexcept -> int {return Protocol{}.Family();};
    constexpr auto ProtoName() const noexcept -> typename Protocol::ProtoName {return Protocol{}.Name();};

private:

    constexpr void InitV4(const V4::AddressUint addr, PortNumber port_num) noexcept
    {
        addr_.v4_.sin_family = AF_INET;
        addr_.v4_.sin_port = AddressTypes::ToNetwork(port_num);
        addr_.v4_.sin_addr.s_addr = addr;
        for(unsigned int i{}; i < sizeof(SockAddrV4::sin_zero); i++){
            addr_.v4_.sin_zero[i] = '\0';
        }
    }

    constexpr void InitV6(const V6::AddressByteArray* addr, PortNumber port_num) noexcept
    {
        addr_.v6_.sin6_family = AF_INET6;
        addr_.v6_.sin6_port = AddressTypes::ToNetwork(port_num);
        addr_.v6_.sin6_flowinfo = 0;
        if(addr == nullptr){
            addr_.v6_.sin6_addr = IN6ADDR_ANY_INIT;
            // for(unsigned int i{}; i < V6::kAdressByteLength; i++)
            //     addr_.v6_.sin6_addr.s6_addr[i] = 0u;
        }else{
            for(unsigned int i{}; i < V6::kAdressByteLength; i++)
                addr_.v6_.sin6_addr.s6_addr[i] = (*addr)[i];
        }
        addr_.v6_.sin6_scope_id = 0;
    }

    union addr_union
    {
        SockAddrBase base_;
        SockAddrV4 v4_;
        SockAddrV6 v6_;
    } addr_{};
};

template<InternetDomain Protocol>
constexpr BasicEndpoint<Protocol>::
BasicEndpoint(const Protocol& proto, PortNumber port_num) noexcept
    : addr_{}
{
   if (proto.Family() == AF_INET){
        this->InitV4(INADDR_ANY, port_num);
   }else if (proto.Family() == AF_INET6){
        this->InitV6(nullptr, port_num);
   }else{
        LogFatalAndAbort("Unrecognized protocol type!");
   }
}

template<InternetDomain Protocol>
constexpr BasicEndpoint<Protocol>::
BasicEndpoint(const IP::Address& addr, PortNumber port_num) noexcept
    : addr_{}
{
   if (addr.IsV4()){
        this->InitV4(addr.V4().data(), port_num);
   }else if (addr.IsV6()){
        const auto addr6_arr{addr.V6().data()};
        this->InitV6(&addr6_arr, port_num);
   }else{
        LogFatalAndAbort("Unrecognized protocol type!");
   }
}

template<InternetDomain Protocol>
constexpr BasicEndpoint<Protocol>::
BasicEndpoint(const IP::AddressV4& addr, PortNumber port_num) noexcept
    : addr_{}
{
    this->InitV4(addr.data(), port_num);
}

template<InternetDomain Protocol>
constexpr BasicEndpoint<Protocol>::
BasicEndpoint(const IP::AddressV6& addr, PortNumber port_num) noexcept
    : addr_{}
{
    const auto addr6_arr{addr.data()};
    this->InitV6(&addr6_arr, port_num);
}

template<InternetDomain Protocol>
inline auto BasicEndpoint<Protocol>::
Port() const noexcept
{
    if (IsV4()){
        return addr_.v4_.sin_port;
    }else{
        return addr_.v6_.sin6_port;
    }
}

template<InternetDomain Protocol>
inline auto BasicEndpoint<Protocol>::
Address() const noexcept -> IP::Address
{
    if (IsV4()){
        return IP::AddressV4{
            addr_.v4_.sin_addr.s_addr, 
            AddressTypes::network_order_tag};
    }else{
        return IP::AddressV6{
            AddressTypes::network_order_tag,
            addr_.v6_.sin6_addr.s6_addr, 
            addr_.v6_.sin6_scope_id,
};
    }
}

template<InternetDomain Protocol>
inline auto BasicEndpoint<Protocol>::
DataView() const noexcept -> Buffer::ConstBufferView
{
    return Buffer::ConstBufferView{&(this->addr_.base_), this->size()};
}

template<InternetDomain Protocol>
inline auto BasicEndpoint<Protocol>::
DataView() noexcept -> Buffer::MutableBufferView
{
    return Buffer::MutableBufferView{&(this->addr_.base_), this->size()};
}

template<InternetDomain Protocol>
inline auto BasicEndpoint<Protocol>::
ToString() const noexcept -> std::string
{
    std::ostringstream os;
    os << '[' << this->Address() << ':' << this->Port() << ']'; 
    return os.str();
}

template <typename Elem, typename Traits, InternetDomain InternetProtocol>
inline std::basic_ostream<Elem, Traits>& operator<<(
    std::basic_ostream<Elem, Traits>& os,
    const BasicEndpoint<InternetProtocol>& endpoint)
{
    return os << endpoint.ToString().c_str();
}

}

#endif /* B0FD1DF0_9960_4684_A2AB_EFA1DF150260 */
