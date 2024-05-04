#ifndef AFB0C165_CCC0_4077_9A25_8668A8CC3089
#define AFB0C165_CCC0_4077_9A25_8668A8CC3089

#include <cstdint>
#include <cstring>
#include <array>
#include <string>
#include <string_view>
#include <type_traits>
#include <netinet/in.h>
//#include <arpa/inet.h>
#include "asrt/sys/syscall.hpp"
#include "asrt/netbuffer.hpp"
#include "ipaddr.h"
#include "asrt/socket/address_types.hpp"

namespace IP{

namespace V4{
    using AddressUint = std::uint_least32_t;
    using AddressByteArray = std::array<unsigned char, 4u>;
    static constexpr std::uint8_t kAdressByteLength{4u};
    static constexpr AddressUint kLoopbackMask{0xFF000000};
    static constexpr AddressUint kMulticastMask{0xF0000000};
    static constexpr AddressUint kLoopbackAddrUint{0x7F000001};
    static constexpr AddressUint kBroadcastAddrUint{0xFFFFFFFF};
    static constexpr AddressUint kLoopbackRange{0x7F000000};
    static constexpr AddressUint kMulticastRange{0xE0000000};
}

class AddressV4
{
public:

    constexpr AddressV4() noexcept = default;
    
    /**
     * @brief Conversion constructor for _ipaddr literals; eg: AddressV4{"127.0.0.1"_ipaddr}
     * 
     */
    constexpr AddressV4(const ::in_addr& addr) noexcept : address_{addr} {}

    AddressV4(const char* addr) noexcept
    {
        if(!Libc::InetPtoN(AF_INET, addr, &this->address_).has_value())
            LogFatalAndAbort("Unrecognized ipv4 address string");
    }


    /**
     * @brief Construct TCP V4 address from host order u32
     * 
     */
    constexpr explicit AddressV4(V4::AddressUint addr) noexcept
    {
        this->address_.s_addr = AddressTypes::ToNetwork(addr);
    }

    /**
     * @brief Construct TCP V4 address from network order u32
     * 
     */
    constexpr explicit AddressV4(V4::AddressUint addr, AddressTypes::NetworkOrderConstructionTag) noexcept
    {
        this->address_.s_addr = addr;
    }  

    /**
     * @brief Construct TCP V4 address from std::array<u8, 4>
     * 
     */
    constexpr explicit AddressV4(const V4::AddressByteArray& addr) noexcept
    {
        std::uint32_t const host{(std::uint32_t{addr[0]} << 24)
                | (std::uint32_t{addr[1]} << 16)
                | (std::uint32_t{addr[2]} << 8)
                |  std::uint32_t{addr[3]}};
        this->address_.s_addr = AddressTypes::ToNetwork(host);
    }

    constexpr AddressV4(AddressV4 const&) = default;
    constexpr AddressV4(AddressV4&&) = default;
    constexpr AddressV4 &operator=(AddressV4 const &other) = default;
    constexpr AddressV4 &operator=(AddressV4 &&other) = default;
    constexpr ~AddressV4() noexcept = default;

    constexpr bool operator==(AddressV4 const &other) const noexcept
    {
        return this->address_.s_addr == other.address_.s_addr;
    }
    
    constexpr auto operator<=>(AddressV4 const &other) const noexcept
    {
        return this->address_.s_addr <=> other.address_.s_addr;
    }

    constexpr auto ToBytes() const noexcept -> V4::AddressByteArray
    {
        V4::AddressByteArray byte_array;
        if(auto host{this->ToUint()}; std::is_constant_evaluated()){
            byte_array[0] = static_cast<std::uint8_t>((host & 0xFF000000) >> 24);
            byte_array[1] = static_cast<std::uint8_t>((host & 0x00FF0000) >> 16);
            byte_array[2] = static_cast<std::uint8_t>((host & 0x0000FF00) >> 8);        
            byte_array[3] = static_cast<std::uint8_t>((host & 0x000000FF));        
        }else{
            std::memcpy(byte_array.data(), &(this->address_.s_addr), 4u);
        }
        return byte_array;
    }

    /**
     * @brief Get uint representation of address in host byte order
     * 
     * @return V4::AddressUint 
     */
    constexpr auto ToUint() const noexcept -> V4::AddressUint
    {
        if (std::is_constant_evaluated()){
            return AddressTypes::ToHost(this->address_.s_addr);
        } else {
            return Libc::NetworkToHostLong(this->address_.s_addr);
        }   
    }

    /**
     * @brief Get underlying address in network byte order
     * 
     * @return constexpr auto 
     */
    constexpr auto data() const noexcept 
    {
        return this->address_.s_addr;
    }

    /**
     * @brief returns empty string on error
    */
    auto ToString() const noexcept -> std::string
    {
        char dest[INET_ADDRSTRLEN]{};
        const auto result{Libc::InetNtoP(AF_INET, &(this->address_), Buffer::make_buffer(dest))};
        return result.has_value() ? 
            std::string{result.value()} : std::string{"Any"};
    }

    constexpr bool IsLoopback() const noexcept
    {
        return (this->ToUint() & V4::kLoopbackMask) == V4::kLoopbackRange; 
    }

    constexpr bool IsMulticast() const noexcept
    {
        return (this->ToUint() & V4::kMulticastMask) == V4::kMulticastRange; 
    }

    constexpr bool IsUnspecified() const noexcept
    {
        return (this->address_.s_addr == 0);
    }

    static constexpr auto 
    Loopback() noexcept -> AddressV4
    {
        return AddressV4{V4::kLoopbackAddrUint};
    }

    static constexpr auto 
    Broadcast() noexcept -> AddressV4
    {
        return AddressV4{V4::kBroadcastAddrUint};
    }

private:

    //AddressTypes::NetworkOrder<V4::AddressUint> address_;
    ::in_addr address_{};
};

/* depreacated */
inline auto
MakeAddressV4(const char* dot_decimal) noexcept -> AddressV4
{
    V4::AddressByteArray addr_bytes{};
    return Libc::InetPtoN(AF_INET, dot_decimal, &addr_bytes).has_value() ?
        AddressV4{addr_bytes} : AddressV4{};
}

inline auto
MakeAddressV4(const std::string& dot_decimal) noexcept -> AddressV4
{
    return MakeAddressV4(dot_decimal.c_str());
}

AddressV4 MakeAddressV4(std::string_view) = delete;

#if 0
constexpr auto addr_raw{"127.0.0.1"_ipaddr};
constexpr AddressV4 v4addr{"127.0.0.1"_ipaddr};
constexpr AddressV4 v4addr_arr{std::to_array<unsigned char>({127, 0, 0, 1})};
constexpr AddressV4 test{100, AddressV4::network_order_tag};

constexpr auto addr_uint{v4addr.ToUint()};

constexpr auto addr_bytes{v4addr.ToBytes()};
#endif
}
#endif /* AFB0C165_CCC0_4077_9A25_8668A8CC3089 */
