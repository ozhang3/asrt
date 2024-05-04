#ifndef C0C1F112_CCCA_431A_B516_0076470CCB2F
#define C0C1F112_CCCA_431A_B516_0076470CCB2F

#include <cstdint>
#include <string_view>
#include <iomanip>
#include <spdlog/fmt/bundled/core.h>

#include "asrt/concepts.hpp"
#include "asrt/reactor/types.hpp"


namespace Socket::PacketSocket{
    template <typename Protocol, typename Reactor>
    class BasicPacketSocket;
}

namespace ErrorCode_Ns{
    enum class ErrorCode : int;
    inline std::string ToString(ErrorCode);
    constexpr std::string_view ToStringView(ErrorCode);
}

namespace Timer::Types{
    enum class TimerTag : std::uint8_t;
} 

namespace IP{
    template<InternetDomain Protocol>
    class BasicEndpoint;
}

namespace ClientServer{
    class GenericMessage;
}

namespace Endpoint_NS{
    template <typename Protocol>
    class PacketEndpoint;
};

template<typename Protocol, typename Reactor>
struct fmt::formatter<Socket::PacketSocket::BasicPacketSocket<Protocol, Reactor>> : fmt::formatter<std::string>
{
    auto format(const Socket::PacketSocket::BasicPacketSocket<Protocol, Reactor>& sock, format_context &ctx) const -> decltype(ctx.out())
    {
        return format_to(ctx.out(), "[sock fd: {}]", static_cast<int>(sock.GetNativeHandle()));
    }
};

template<>
struct fmt::formatter<ReactorNS::Types::Events> : fmt::formatter<std::string>
{
    auto format(ReactorNS::Types::Events ev, format_context &ctx) const -> decltype(ctx.out())
    {
        return format_to(ctx.out(), "[{}]", ReactorNS::Types::details::ToString((ReactorNS::Types::details::EventType)ev.ExtractEpollEvent()));
    }
};

// template<>
// struct fmt::formatter<ErrorCode_Ns::ErrorCode> : fmt::formatter<std::string>
// {
//     auto format(ErrorCode_Ns::ErrorCode ec, format_context &ctx) const -> decltype(ctx.out())
//     {
//         return format_to(ctx.out(), "{}", ErrorCode_Ns::ToString(ec));
//     }
// };

template<>
struct fmt::formatter<ErrorCode_Ns::ErrorCode> : fmt::formatter<std::string_view>
{
    auto format(ErrorCode_Ns::ErrorCode ec, format_context &ctx) const -> decltype(ctx.out())
    {
        return format_to(ctx.out(), "{}", ErrorCode_Ns::ToStringView(ec));
    }
};

template<>
struct fmt::formatter<Timer::Types::TimerTag> : fmt::formatter<std::string>
{
    auto format(Timer::Types::TimerTag tag, format_context &ctx) const -> decltype(ctx.out())
    {
        return format_to(ctx.out(), "{}", std::underlying_type_t<Timer::Types::TimerTag>(tag));
    }
};

template<InternetDomain Protocol>
struct fmt::formatter<IP::BasicEndpoint<Protocol>> : fmt::formatter<std::string>
{
    auto format(const IP::BasicEndpoint<Protocol>& ep, format_context &ctx) const -> decltype(ctx.out())
    {
        return format_to(ctx.out(), "{}", ep.ToString());
    }
};

template<typename Protocol>
struct fmt::formatter<Endpoint_NS::PacketEndpoint<Protocol>> : fmt::formatter<std::string_view>
{
    auto format(const Endpoint_NS::PacketEndpoint<Protocol>& ep, format_context &ctx) const -> decltype(ctx.out())
    {
        return format_to(ctx.out(), "{}", ep.ToStringView());
    }
};

// template<>
// struct fmt::formatter<ClientServer::GenericMessage> : fmt::formatter<std::string>
// {
//     auto format(ClientServer::GenericMessage& msg, format_context &ctx) const -> decltype(ctx.out())
//     {
//         return format_to(ctx.out(), "{}", msg.ToString());
//     }
// };
#endif /* C0C1F112_CCCA_431A_B516_0076470CCB2F */
