#ifndef IPADDR_H_
#define IPADDR_H_

#include <array>
#include <bit>
#include <type_traits>
#include <cstdint>
#include <arpa/inet.h>

namespace IPAddr {

    namespace details {

        template <typename T>
        static constexpr T host_to_net(T hostval)
        {
            static_assert(std::is_integral<T>::value && (sizeof(T) == 2 || sizeof(T) == 4));

        #ifdef __cpp_lib_endian
            if constexpr ( std::endian::native == std::endian::big )
        #else
            if constexpr ( __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ )
        #endif
                return hostval;

        #ifdef __cpp_lib_byteswap
            return std::byteswap<T>(hostval);
        #else
            if constexpr ( sizeof(T) == 2 )
                return __builtin_bswap16(hostval);
            else if constexpr ( sizeof(T) == 4 )
                return __builtin_bswap32(hostval);
        #endif
        }

        template <typename T>
        static constexpr T net_to_host(T netval)
        {
            return host_to_net(netval);
        }

        static constexpr bool isdigit(char c)
        {
            return c >= '0' && c <= '9';
        }

        static constexpr bool isxdigit(char c)
        {
            return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        }

        static constexpr char islower(char c)
        {
            return (c >= 'a' && c <= 'z');
        }

        static constexpr char toupper(char c)
        {
            if ( !islower(c) )
                return c;

            return c ^ 0x20;
        }

        template <size_t N>
        static constexpr ssize_t rfind_chr(const char (&str)[N], size_t from, char c)
        {
            for ( ssize_t i = from; i >= 0; i-- )
                if ( str[i] == c )
                    return i;

            return -1;
        }

        template <size_t N>
        static constexpr ssize_t find_chr(const char (&str)[N], size_t from, char c)
        {
            for ( size_t i = from; i < N; i++ )
                if ( str[i] == c )
                    return i;

            return -1;
        }

        template <int base>
        static constexpr bool is_valid_digit(char c)
        {
            static_assert(base == 8 || base == 10 || base == 16, "Invalid base parameter");

            if constexpr ( base == 8 )
                return (c >= '0' && c <= '7');
            else if constexpr ( base == 10 )
                return isdigit(c);
            else if constexpr ( base == 16 )
                return isxdigit(c);
        }

        template <int base>
        static constexpr int convert_digit(char c)
        {
            static_assert(base == 8 || base == 10 || base == 16, "Invalid base parameter");

            if ( !is_valid_digit<base>(c) )
                return -1;

            if constexpr ( base == 8 || base == 10 ) {
                return c - '0';
            }
            else if constexpr ( base == 16 )
            {
                if ( isdigit(c) )
                    return convert_digit<10>(c);
                else if ( c >= 'A' && c <= 'F' )
                    return c - 'A' + 10;
                else
                    return c - 'a' + 10;
            }
        }

        template <int base, char sep, unsigned max_value, size_t max_length = 0, size_t N>
        static constexpr long long parse_address_component(const char (&str)[N], size_t idx)
        {
            long long res = 0;

            if ( N - 1 - idx <= 0 || str[idx] == sep )
                return -1;

            for ( size_t i = idx; i < N-1 && str[i] != sep; i++ )
            {
                if ( max_length > 0 && (i - idx + 1) > max_length )
                   return -1;

                if ( !is_valid_digit<base>(str[i]) )
                    return -1;

                res *= base;
                res += convert_digit<base>(str[i]);

                if ( res > max_value )
                    return -1;
            }

            return res;
        }

        template <int base, unsigned max_value, size_t N>
        static constexpr int parse_inet_component_base(const char (&str)[N], size_t idx)
        {
            return parse_address_component<base, '.', max_value>(str, idx);
        }

        template <unsigned max_value, size_t N>
        static constexpr int parse_inet_component_oct(const char (&str)[N], size_t idx)
        {
            return parse_inet_component_base<8, max_value>(str, idx);
        }

        template <unsigned max_value, size_t N>
        static constexpr int parse_inet_component_dec(const char (&str)[N], size_t idx)
        {
            return parse_inet_component_base<10, max_value>(str, idx);
        }

        template <unsigned max_value, size_t N>
        static constexpr int parse_inet_component_hex(const char (&str)[N], size_t idx)
        {
            return parse_inet_component_base<16, max_value>(str, idx);
        }

        //
        // Parse a component of an IPv4 address.
        //
        template <unsigned max_value = 255, size_t N>
        static constexpr int parse_inet_component(const char (&str)[N], size_t idx)
        {
            if ( (N - idx) > 2 && str[idx] == '0' && (toupper(str[idx+1]) == 'X') )
                return parse_inet_component_hex<max_value>(str, idx + 2);
            else if ( (N - idx) > 2 && str[idx] == '0' && isdigit(str[idx+1]) && str[idx+1] != '0' )
                return parse_inet_component_oct<max_value>(str, idx + 1);
            else
                return parse_inet_component_dec<max_value>(str, idx);
        }

        //
        // Parse a component of an IPv4 address in its canonical form.
        // Leading zeros are not allowed, and component must be expressed in decimal form.
        //
        template <size_t N>
        static constexpr int parse_inet_component_canonical(const char (&str)[N], size_t idx)
        {
            if ( (N - idx) > 2 && str[idx] == '0' && isdigit(str[idx + 1]) )
                return -1;

            return parse_address_component<10, '.', 255, 3>(str, idx);
        }

        //
        // Parse a component of an IPv6 address.
        //
        template <size_t N>
        static constexpr int parse_inet6_hexlet(const char (&str)[N], size_t idx)
        {
            return parse_address_component<16, ':', 0xFFFF, 4>(str, idx);
        }

        template <size_t N>
        static constexpr int inet_addr_canonical_at(const char (&str)[N], ssize_t idx, in_addr_t& s_addr)
        {
            // Split and parse each component according to POSIX rules.
            ssize_t sep3 = rfind_chr(str, N-1, '.'),
                    sep2 = rfind_chr(str, sep3-1, '.'),
                    sep1 = rfind_chr(str, sep2-1, '.');

            if ( sep3 < idx+1 || sep2 < idx+1 || sep1 < idx+1 || rfind_chr(str, sep1-1, '.') >= idx )
                return -1;

            long long c1 = parse_inet_component_canonical(str, idx),
                      c2 = parse_inet_component_canonical(str, sep1 + 1),
                      c3 = parse_inet_component_canonical(str, sep2 + 1),
                      c4 = parse_inet_component_canonical(str, sep3 + 1);

            if ( c1 < 0 || c1 < 0 || c2 < 0 || c3 < 0 )
                return -1;

            s_addr = host_to_net(static_cast<uint32_t>((c1 << 24) | (c2 << 16) | (c3 << 8) | c4));
            return 0;
        }

        template <size_t N>
        static constexpr int inet_addr_canonical(const char (&str)[N], in_addr_t& s_addr)
        {
            return inet_addr_canonical_at(str, 0, s_addr);
        }

        //
        // Parse an IPv4 address.
        // We split the address into its different components and parse them separately.
        // Supported syntax: a, a.b, a.b.c, a.b.c.d
        //
        // Individual components can be expressed in decimal, octal and hexadecimal.
        //
        template <size_t N>
        static constexpr int inet_addr_impl(const char (&str)[N], in_addr_t& s_addr)
        {
            long long c1 = 0, c2 = 0, c3 = 0, c4 = 0;

            // The address string cannot be empty or start/end with a separator.
            if ( N == 0 || str[0] == '.' || str[N-1] == '.' )
                return -1;

            // Split and parse each component according to standard rules.
            ssize_t sep3 = rfind_chr(str, N-1, '.');
            if ( sep3 > 0 ) {
                c1 = parse_inet_component(str, 0);
                c4 = parse_inet_component(str, sep3+1);

                ssize_t sep2 = rfind_chr(str, sep3-1, '.');
                if ( sep2 > 0 ) {
                    ssize_t sep1 = rfind_chr(str, sep2-1, '.');
                    if ( sep1 > 0 ) {
                        // Cannot have more than three separators.
                        if ( rfind_chr(str, sep1-1, '.') != -1 )
                            return -1;

                        c2 = parse_inet_component(str, sep1+1);
                        c3 = parse_inet_component(str, sep2+1);
                    }
                    else {
                        c2 = parse_inet_component(str, sep2+1);
                    }
                }
            }
            else {
                c4 = parse_inet_component<0xFFFFFFFF>(str, 0);
            }

            if ( c1 < 0 || c2 < 0 || c3 < 0 || c4 < 0 )
                return -1;

            s_addr = host_to_net(static_cast<uint32_t>((c1 << 24) | (c2 << 16) | (c3 << 8) | c4));
            return 0;
        }

        template <size_t N>
        static constexpr int inet_aton_impl(const char (&str)[N], struct in_addr& in)
        {
            return inet_addr_impl(str, in.s_addr);
        }

        template <size_t N>
        static constexpr int inet_aton_canonical(const char (&str)[N], struct in_addr& in)
        {
            return inet_addr_canonical(str, in.s_addr);
        }

        static constexpr void inet6_array_to_saddr(std::array<uint16_t, 8> const& ip6_comps, struct in6_addr& in6)
        {
            for ( size_t i = 0; i < ip6_comps.size(); i++ ) {
                uint16_t hexlet = ip6_comps[i];

                in6.s6_addr[i * 2] = hexlet >> 8;
                in6.s6_addr[i * 2 + 1] = hexlet & 0xff;
            }
        }

        template <typename T, size_t N>
        static constexpr void rshift_array(std::array<T, N>& a, size_t from, size_t shift)
        {
            if ( from > N - 1 )
                return;

            for ( ssize_t pos = N - 1; pos >= static_cast<ssize_t>(from + shift); pos-- ) {
                if ( pos - shift >= 0 ) {
                    a[pos] = a[pos - shift];
                    a[pos - shift] = 0;
                }
                else
                    a[pos] = 0;
            }
        }

        //
        // Parse an IPv6 address.
        // Format can be:
        //  1. x:x:x:x:x:x:x:x with each component being a 16-bit hexadecimal number
        //  2. Contiguous zero components can be compacted as "::", allowed to appear only once in the address.
        //  3. First 96 bits in above representation and last 32 bits represented as an IPv4 address.
        //
        template <size_t N>
        static constexpr int inet6_aton(const char (&str)[N], struct in6_addr& in6)
        {
            std::array<uint16_t, 8> comps = { 0 };
            int shortener_pos = -1;
            size_t idx = 0;
            in_addr_t v4_addr = -1;
            auto remaining_chars = [](size_t pos) constexpr { return N - 1 - pos; };

            // The address must contain at least two chars, cannot start/end with a separator alone.
            if ( N < 3 || (str[0] == ':' && str[1] != ':') || (str[N-1] == ':' && str[N-2] != ':') )
                return -1;

            for ( unsigned i = 0; i < comps.size(); i++ ) {

                // We have reached the end of the string before parsing all the components.
                // That is possible only if we have previously encountered a shortener token.
                if ( idx == N-1 ) {
                    if ( shortener_pos == -1 )
                        return -1;
                    else {
                        rshift_array(comps, shortener_pos, comps.size() - i);
                        break;
                    }
                }

                // Check if we have an embedded IPv4 address.
                if ( (i == 6 || (i < 6 && shortener_pos != -1)) && inet_addr_canonical_at(str, idx, v4_addr) != -1 )
                {
                    v4_addr = net_to_host(v4_addr);

                    comps[i++] = (v4_addr >> 16) & 0xffff;
                    comps[i++] = v4_addr & 0xffff;

                    if ( shortener_pos != -1 )
                        rshift_array(comps, shortener_pos, comps.size() - i);

                    idx = N - 1;
                    break;
                }

                // A shortener token (::) is encountered.
                if ( remaining_chars(idx) >= 2 && str[idx] == ':' && str[idx+1] == ':' )
                {
                    // The address shortener syntax can only appear once.
                    if ( shortener_pos != -1 )
                        return -1;

                    // It cannot be followed by another separator token.
                    if ( remaining_chars(idx) >= 3 && str[idx+2] == ':' )
                        return -1;

                    // Save the component position where the token was found.
                    shortener_pos = i;

                    idx += 2;
                }
                else
                {
                    int hexlet = parse_inet6_hexlet(str, idx);
                    if ( hexlet == -1 )
                        return -1;

                    comps[i] = hexlet;

                    ssize_t next_sep = find_chr(str, idx, ':');
                    if ( next_sep == -1 ) {
                        idx = N-1;
                    }
                    else if ( remaining_chars(next_sep) >= 2 && str[next_sep+1] == ':' ) {
                        idx = next_sep;
                    }
                    else {
                        idx = next_sep + 1;
                    }
                }
            }

            // Once all components have been parsed, we must be pointing at the end of the string.
            if ( idx != N-1 )
                return -1;

            inet6_array_to_saddr(comps, in6);
            return 0;
        }
    } //end namespace Details

    template <size_t N>
    static constexpr in_addr_t inet_addr(const char (&str)[N])
    {
        in_addr_t addr = -1;

        details::inet_addr_impl(str, addr);
        return addr;
    }

    template <size_t N>
    static constexpr struct in_addr inet_aton(const char (&str)[N])
    {
        struct in_addr in = { 0xFFFFFFFF };

        details::inet_aton_impl(str, in);
        return in;
    }

    template <int AddressF, size_t N>
    static constexpr auto inet_pton(const char (&str)[N])
    {
        static_assert(AddressF == AF_INET || AddressF == AF_INET6, "Unsupported address family.");

        if constexpr (AddressF == AF_INET ) {
            struct in_addr in = {};
            details::inet_aton_canonical(str, in);

            return in;
        }
        else {
            struct in6_addr in6 = {};
            details::inet6_aton(str, in6);

            return in6;
        }
    }

    template <size_t N>
    static constexpr bool is_valid_ip4addr(const char (&str)[N])
    {
        struct in_addr in = {};

        return details::inet_aton_impl(str, in) != -1;
    }

    template <size_t N>
    static constexpr bool is_valid_ip6addr(const char (&str)[N])
    {
        struct in6_addr in6 = {};

        return details::inet6_aton(str, in6) != -1;
    }

}

#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wpedantic"
#    ifdef __clang__
#        pragma GCC diagnostic ignored "-Wgnu-string-literal-operator-template"
#    endif
template <typename CharT, CharT... Cs>
static constexpr auto operator "" _ipaddr()
{
    constexpr char str[] = { Cs..., 0 };

    static_assert(IPAddr::is_valid_ip4addr(str) || IPAddr::is_valid_ip6addr(str), "Invalid IP address format.");

    if constexpr ( IPAddr::is_valid_ip4addr(str) )
        return IPAddr::inet_aton(str);
    else
        return IPAddr::inet_pton<AF_INET6>(str);
}
#    pragma GCC diagnostic pop

static constexpr uint16_t operator "" _ipport(unsigned long long port)
{
    if ( port > 65535 )
        return 0;

    return IPAddr::details::host_to_net(static_cast<uint16_t>(port));
}

#endif

