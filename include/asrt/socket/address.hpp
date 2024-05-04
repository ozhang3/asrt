#ifndef FE20CC12_EB35_4F78_A1E1_4FF485D919F0
#define FE20CC12_EB35_4F78_A1E1_4FF485D919F0

#include <cstdint>
#include <variant>
#include <compare>
#include <sstream>

#include "asrt/socket/address_v4.hpp"
#include "asrt/socket/address_v6.hpp"
#include "ipaddr.h"

namespace IP{


class Address
{
public:
    constexpr Address() noexcept = default;
    constexpr Address(const AddressV4& ipv4_address) noexcept;
    constexpr Address(const AddressV6& ipv6_address) noexcept;
    constexpr Address(const Address& other) noexcept = default;
    constexpr Address(Address&& other) noexcept = default;
    constexpr Address& operator=(const Address& other) noexcept = default;
    constexpr Address& operator=(Address&& other) noexcept = default;
    Address& operator=(const AddressV4& ipv4_address) noexcept;
    Address& operator=(const AddressV6& ipv6_address) noexcept;
    constexpr auto operator<=>(const Address& other) const = default;
    constexpr ~Address() noexcept = default;

    constexpr auto V4() const -> const AddressV4& {return std::get<AddressV4>(this->address_);}
    constexpr auto V6() const -> const AddressV6& {return std::get<AddressV6>(this->address_);}
    constexpr bool IsV4() const noexcept {return std::holds_alternative<AddressV4>(this->address_);}
    constexpr bool IsV6() const noexcept {return std::holds_alternative<AddressV6>(this->address_);}
    constexpr bool IsLoopback() const noexcept;
    constexpr bool IsMulticast() const noexcept;
    constexpr bool IsUnspecified() const noexcept;

    auto ToString() const -> std::string;

private:
    std::variant<AddressV4, AddressV6> address_;
};

constexpr inline Address::
Address(const AddressV4& ipv4_address) noexcept
    : address_{std::in_place_index<0>, ipv4_address} {}

constexpr inline Address::
Address(const AddressV6& ipv6_address) noexcept
    : address_{std::in_place_index<1>, ipv6_address} {}

inline Address& Address::
operator=(const AddressV4& ipv4_address) noexcept //cannot be marked constexpr due to non-constexpr emplace (as of gcc 11.4; will be fixed in gcc 12)
{   
    this->address_.emplace<AddressV4>(ipv4_address);
    return *this;
}

inline Address& Address::
operator=(const AddressV6& ipv6_address) noexcept
{
    this->address_.emplace<AddressV6>(ipv6_address);
    return *this;
}

inline auto Address::
ToString() const -> std::string
{
    return this->IsV4() ?
        std::get<AddressV4>(this->address_).ToString() :
        std::get<AddressV6>(this->address_).ToString();
}

constexpr inline bool Address::
IsLoopback() const noexcept
{
    return this->IsV4() ?
        std::get<AddressV4>(this->address_).IsLoopback() :
        std::get<AddressV6>(this->address_).IsLoopback();
}

constexpr inline bool Address::
IsMulticast() const noexcept
{
    return this->IsV4() ?
        std::get<AddressV4>(this->address_).IsMulticast() :
        std::get<AddressV6>(this->address_).IsMulticast();
}

constexpr inline bool Address::
IsUnspecified() const noexcept
{
    return this->IsV4() ?
        std::get<AddressV4>(this->address_).IsUnspecified() :
        std::get<AddressV6>(this->address_).IsUnspecified();
}

template <typename Elem, typename Traits>
inline std::basic_ostream<Elem, Traits>& operator<<(
    std::basic_ostream<Elem, Traits>& os, const Address& addr)
{
    return os << addr.ToString().c_str();
}


//constexpr Address adddd{"127.0.0.1"_ipaddr};
}

#endif /* FE20CC12_EB35_4F78_A1E1_4FF485D919F0 */
