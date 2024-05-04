#ifndef B1F92125_FFFA_4495_9F5F_3F1030737633
#define B1F92125_FFFA_4495_9F5F_3F1030737633

#include <cstdint>
#include <type_traits>
#include <concepts>
#include <bit>

namespace AddressTypes{

    namespace details{

        // template <typename T>
        // struct is_swappable_integral : std::false_type;


        template <typename T>
        concept swappable_integral = 
            (std::is_integral_v<T> &&\
                ( sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8));

        // template <typename T>
        // inline constexpr bool is_swappable_integral_v = swappable_integral<T>::value;

        //constexpr auto a{is_swappable_integral_v<int[]>};
        
        static constexpr inline bool
        IsNativeBigEndian() {
    #ifdef __cpp_lib_endian        
            return std::endian::native == std::endian::big;
    #else
            return __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__;
    #endif
        }

        template <swappable_integral T>
        static constexpr inline T SwapInt(const T hostval)
        {
    #ifdef __cpp_lib_byteswap
            return std::byteswap<T>(hostval);
    #else
            if constexpr ( sizeof(T) == 1 )
                return hostval;
            else if constexpr ( sizeof(T) == 2 )
                return __builtin_bswap16(hostval);
            else if constexpr ( sizeof(T) == 4 )
                return __builtin_bswap32(hostval);
            else if constexpr ( sizeof(T) == 8 )
                return __builtin_bswap64(hostval);
    #endif
        }
    }

    template <details::swappable_integral T>
    static constexpr inline T ToNetwork(const T hostval)
    {
        if constexpr (details::IsNativeBigEndian())
            return hostval;
        return details::SwapInt(hostval);
    }

    template <details::swappable_integral T>
    static constexpr inline T ToHost(const T netval)
    {
        if constexpr (details::IsNativeBigEndian())
            return netval;
        return details::SwapInt(netval);
    }

    template <details::swappable_integral T> 
    class NetworkOrder
    {
    public:
        //static_assert(std::is_standard_layout_v<NetworkOrder<T>>, "class needs to be standard layout!");

        constexpr NetworkOrder() noexcept = default;
        constexpr NetworkOrder(T host) noexcept : rep_{AddressTypes::ToNetwork(host)} {}
        constexpr operator T() const noexcept {return AddressTypes::ToHost(this->rep_);}
        constexpr T ToHost() const noexcept {return AddressTypes::ToHost(this->rep_);}
        void FromHost(T host) noexcept {rep_ = AddressTypes::ToNetwork(host);}
        constexpr void Set(T net) noexcept {this->rep_ = net;}
        constexpr T Get() const noexcept {return this->rep_;};
        T* data() noexcept {return &rep_;}
        const T* data() const noexcept {return &rep_;}
        static constexpr std::size_t size() noexcept {return sizeof(T);}
    private:
        T rep_{};
    };

    // constexpr NetworkOrder<std::uint16_t> test_var{1234u};
    // constexpr std::size_t test_var_size{test_var.size()};
    // constexpr std::size_t test_var_size_s{NetworkOrder<std::uint16_t>::size()};

    static_assert(sizeof(NetworkOrder<std::uint16_t>) == sizeof(std::uint16_t), "Alignment error!");
    static_assert(sizeof(NetworkOrder<std::uint32_t>) == sizeof(std::uint32_t), "Alignment error!");
    static_assert(sizeof(NetworkOrder<std::uint64_t>) == sizeof(std::uint64_t), "Alignment error!");

    template <details::swappable_integral T> 
    class HostOrder
    {
    public:
        //static_assert(std::is_standard_layout_v<HostOrder<T>>, "class needs to be standard layout!");

        constexpr HostOrder() noexcept = default;
        constexpr HostOrder(T net) noexcept : rep_{ToHost(net)} {}
        constexpr operator T() const noexcept {return ToNetwork(this->rep_);}
    private:
        T rep_{};
    };

    enum class ByteOrder : std::uint8_t{
        Host,
        Network
    };

    struct NetworkOrderConstructionTag {};
    static constexpr NetworkOrderConstructionTag network_order_tag{};

    struct HostOrderConstructionTag {};
    static constexpr HostOrderConstructionTag host_order_tag{};
}

#endif /* B1F92125_FFFA_4495_9F5F_3F1030737633 */
