#ifndef E6461D5D_8265_4445_AD8B_89AC02509A9E
#define E6461D5D_8265_4445_AD8B_89AC02509A9E

#include <cstdint>
#include <sys/socket.h>

namespace SockOption
{
    template <class DerivedOption, int OptionLevel, int OptionName>
    struct SocketOption
    {
        constexpr int Level() const noexcept {return OptionLevel;}
        constexpr int Name() const noexcept {return OptionName;}
    
    private:
        using Self = DerivedOption;
        constexpr const Self &self() const noexcept { return static_cast<const Self &>(*this); }
    };

    template <int OptionLevel, int OptionName>
    struct BoolOption : 
        SocketOption<BoolOption<OptionLevel, OptionName>, OptionLevel, OptionName>
    {
        constexpr BoolOption() noexcept = default;
        constexpr explicit BoolOption(bool val) noexcept : value_{val ? 1 : 0} {}
        BoolOption& operator=(bool val) noexcept {value_ = static_cast<int>(val); return *this;}
        constexpr operator bool() const noexcept {return static_cast<bool>(value_);}
        constexpr bool Value() const noexcept {return static_cast<bool>(value_);}
        constexpr std::size_t Length() const noexcept {return sizeof(value_);}
        int* data() noexcept {return &value_;}
        const int* data() const noexcept {return &value_;}
    private:
        int value_{false};
    };

    template <int OptionLevel, int OptionName>
    struct IntOption : 
        SocketOption<IntOption<OptionLevel, OptionName>, OptionLevel, OptionName>
    {
        constexpr IntOption() noexcept = default;
        constexpr explicit IntOption(int val) noexcept : value_{val} {}
        IntOption& operator=(int val) noexcept {value_ = val; return *this;}
        constexpr int Value() const noexcept {return value_;}
        constexpr std::size_t Length() const noexcept {return sizeof(value_);}
        int* data() noexcept {return &value_;}
        const int* data() const noexcept {return &value_;}
    private:
        int value_{};
    };

    struct ConstSockOptionView
    {
        const void *data;
        ::socklen_t len;
    };

    struct MutableSockOptionView
    {
        void *data;
        ::socklen_t len;
    };

}

#endif /* E6461D5D_8265_4445_AD8B_89AC02509A9E */
