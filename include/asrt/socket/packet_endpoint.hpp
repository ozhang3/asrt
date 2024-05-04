#ifndef B14B9C18_AFEB_473B_88D8_5EB66B731E2A
#define B14B9C18_AFEB_473B_88D8_5EB66B731E2A

#include <array>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <sys/socket.h>
#include <string>
#include <iostream>
#include <linux/if_packet.h>
#include <net/if.h>
#include <net/ethernet.h> /* the L2 protocols */
#include <arpa/inet.h>
#include "asrt/socket/types.hpp"
//#include "asrt/socket/protocol.hpp"
#include "asrt/sys/syscall.hpp"
#include "asrt/error_code.hpp"
//#include "asrt/util.hpp"

namespace Endpoint_NS{

template <typename T> using Result = Util::Expected_NS::Expected<T, ErrorCode_Ns::ErrorCode>;
namespace details{
    static constexpr int kMaxNetworkInterfaceNameLength{sizeof(decltype(::ifreq::ifr_name)) - 1}; //15 bytes

    enum class EtherType : int{
        kETH_P_ALL = ETH_P_ALL,
        kETH_P_TSN = ETH_P_TSN
    };
}


/**
 * @brief Endpoint implementation for AF_PACKET sockets
 * 
 */
template <typename Protocol>
class PacketEndpoint final
{
public:

    PacketEndpoint() = delete;
    /**
     * @brief Construct a endpoint for AF_PACKET sockets
     * @param interface_name a string containing the name of the interface, eg: "eth0"
     * @param protocol_type eg: ETH_P_ALL
     */
    constexpr PacketEndpoint(const char* interface_name, unsigned short protocol_type = ETH_P_ALL) noexcept 
        : protocol_{protocol_type}
    {
        this->SetInterface(interface_name);
    }

    explicit PacketEndpoint(const std::string& interface_name, unsigned short protocol_type = ETH_P_ALL) noexcept 
        : protocol_{protocol_type}
    {
        this->SetInterface(interface_name.c_str());
    }

    PacketEndpoint(PacketEndpoint const&) = delete;
    PacketEndpoint(PacketEndpoint&&) = delete;
    PacketEndpoint &operator=(PacketEndpoint const &other) = delete;
    PacketEndpoint &operator=(PacketEndpoint &&other) = delete;
    constexpr ~PacketEndpoint() noexcept = default;

    constexpr auto IfName() const noexcept -> const char* {return this->if_name_;}
    constexpr auto EtherProto() const noexcept -> unsigned short {return this->protocol_;}
    constexpr auto ProtoName() const noexcept -> typename Protocol::ProtoName {return Protocol{}.Name();}

    auto IfIndex() const noexcept -> unsigned int
    {
        return ::if_nametoindex(this->if_name_);
    }

    auto HwAddress(unsigned char* addr) const noexcept
    {
        OsAbstraction::GetIfHwAddr(this->if_name_, addr);
    }

    auto SockAddrLL(::sockaddr_ll& addr_ll, int sock_fd = 0) const noexcept -> Result<void>
    {
        return OsAbstraction::GetNetIfIndex(this->if_name_, sock_fd)
            .and_then([this, sock_fd, &addr_ll](int if_index){
                addr_ll.sll_ifindex = if_index;
                return OsAbstraction::GetIfHwAddr(this->if_name_, addr_ll.sll_addr, sock_fd);
            })
            .map([this, &addr_ll](){
                addr_ll.sll_family = AF_PACKET;
                addr_ll.sll_halen = ETH_ALEN;
            });
    }

    constexpr auto ToStringView() const noexcept -> std::string_view
    {
        return std::string_view{this->if_name_};
    }
    //auto Family() const -> std::uint8_t {return this->proto_.Family();};
    //auto ProtoName() const -> std::uint8_t {return this->proto_.Name();};

private:

    constexpr void SetInterface(const char* name) noexcept
    {
        const std::size_t name_len{std::char_traits<char>::length(name) + 1};
        assert((name_len > 0) && (name_len <= details::kMaxNetworkInterfaceNameLength));
        for(std::size_t i{}; i < name_len; i++)
            this->if_name_[i] = name[i];
        this->if_name_len_ = name_len;
    }

    unsigned short protocol_{0};
    char if_name_[details::kMaxNetworkInterfaceNameLength + 1]{}; //compensate for null termiantion
    std::uint8_t if_name_len_{};
};

}//end namespace
#endif /* B14B9C18_AFEB_473B_88D8_5EB66B731E2A */
