#ifndef BB839C15_031F_4984_B8D1_C3019D6D8467
#define BB839C15_031F_4984_B8D1_C3019D6D8467

#include <cstdint>
#include <cstring>
#include <array>
#include <string>
#include <netinet/in.h>
//#include <arpa/inet.h>
#include "asrt/sys/syscall.hpp"
#include "asrt/netbuffer.hpp"

namespace IP{


namespace V6{
    static constexpr std::uint8_t kAdressByteLength{16u};
    using ScopeId = std::uint_least32_t;
    using AddressByteArray = std::array<std::uint8_t, kAdressByteLength>;
    using AddressCArray = std::uint8_t[kAdressByteLength];
    static constexpr AddressByteArray kAddressUnspec{};
    static constexpr AddressByteArray kAddressLoopback{0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
    static constexpr AddressByteArray kAddressBroadcast{0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu};
}

class AddressV6
{
public:

    constexpr AddressV6() noexcept = default;

    constexpr explicit AddressV6(const V6::AddressCArray& addr, V6::ScopeId scope_id = 0) noexcept 
        : scope_id_{scope_id}
    {
        for(auto i = 0; i < V6::kAdressByteLength; i++){
            this->address_.s6_addr[i] = addr[V6::kAdressByteLength - 1 - i];
        }
    }

    /**
     * @brief Construct TCP V6 address from network order u32
     * 
     */
    constexpr explicit AddressV6(AddressTypes::NetworkOrderConstructionTag, const V6::AddressCArray& addr, V6::ScopeId scope_id = 0) noexcept
        : scope_id_{scope_id}
    {
        for(auto i = 0; i < V6::kAdressByteLength; i++){
            this->address_.s6_addr[i] = addr[i];
        }
    }  

    constexpr explicit 
    AddressV6(const V6::AddressByteArray& addr, V6::ScopeId scope_id = 0) noexcept 
        : scope_id_{scope_id}
    {
        for(auto i = 0; i < V6::kAdressByteLength; i++){
            this->address_.s6_addr[i] = addr[V6::kAdressByteLength - 1 - i];
        }
    }

    constexpr explicit 
    AddressV6(AddressTypes::NetworkOrderConstructionTag, const V6::AddressByteArray& addr, V6::ScopeId scope_id = 0) noexcept 
        : scope_id_{scope_id}
    {
        for(auto i = 0; i < V6::kAdressByteLength; i++){
            this->address_.s6_addr[i] = addr[i];
        }
    }

    constexpr AddressV6(const ::in6_addr& addr) noexcept
    {
        for(auto i = 0; i < V6::kAdressByteLength; i++){
            this->address_.s6_addr[i] = addr.s6_addr[i];
        }
    }

    constexpr AddressV6(AddressV6 const&) = default;
    constexpr AddressV6(AddressV6&&) = default;
    constexpr AddressV6 &operator=(AddressV6 const &other) = default;
    constexpr AddressV6 &operator=(AddressV6 &&other) = default;
    constexpr ~AddressV6() noexcept = default;

    constexpr auto data() const noexcept
    {
        return std::to_array(this->address_.s6_addr);
    }

    constexpr auto ToBytes() const noexcept -> V6::AddressByteArray
    {
        V6::AddressByteArray byte_array{};
        for(auto i = 0; i < V6::kAdressByteLength; i++){
            byte_array[i] = this->address_.s6_addr[V6::kAdressByteLength - 1 - i];
        }
        //std::memcpy(byte_array.data(), &address_.s6_addr, V6::kAdressByteLength);
        return byte_array;
    }


    auto ToString() const noexcept -> std::string
    {
        char dest[INET6_ADDRSTRLEN]{};
        auto result{Libc::InetNtoP(AF_INET6, &(this->address_.s6_addr), Buffer::make_buffer(dest))};
        return result.has_value() ?
            std::string{result.value()} : std::string{"Any"};
    }

    constexpr bool IsLoopback() const noexcept
    {
        return (this->ToBytes() == V6::kAddressLoopback); 
    }

    constexpr bool IsMulticast() const noexcept
    {
        return (this->ToBytes() == V6::kAddressLoopback); 
    }

    constexpr bool IsUnspecified() const noexcept
    {
        return (this->ToBytes() == V6::kAddressUnspec); 
    }

    static constexpr auto Loopback() noexcept -> AddressV6
    {
        return AddressV6{V6::kAddressLoopback};
    }

    static constexpr auto Broadcast() noexcept -> AddressV6
    {
        return AddressV6{V6::kAddressBroadcast};
    }

    constexpr bool operator==(const AddressV6& other) const noexcept
    {
        return this->ToBytes() == other.ToBytes() &&
               this->scope_id_ == other.scope_id_;
    }

    constexpr auto operator<=>(const AddressV6& other) const noexcept
    {
        if(auto res{this->ToBytes() <=> other.ToBytes()}; res != 0)
            return res;
        return this->scope_id_ <=> other.scope_id_;
    }

private:
    ::in6_addr address_{};
    V6::ScopeId scope_id_{};
};

#if 0

constexpr ::in6_addr loopback_const{0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};

constexpr AddressV6 addrv6{AddressV6::Loopback()};

constexpr AddressV6 addrv6_2{AddressV6::Broadcast()};

constexpr bool same_{addrv6 == addrv6_2};

constexpr V6::AddressByteArray bytes{addrv6.ToBytes()};

#endif

}


#endif /* BB839C15_031F_4984_B8D1_C3019D6D8467 */
