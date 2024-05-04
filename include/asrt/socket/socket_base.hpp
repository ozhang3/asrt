#ifndef D7B3D564_0000_4BB7_AFF9_C6B83759F6F8
#define D7B3D564_0000_4BB7_AFF9_C6B83759F6F8

#include <string_view>
#include "asrt/socket/socket_option.hpp"

namespace Socket{

static constexpr std::string_view kSockOptionStrings[11]{
    "Broadcast",
    "Debug",
    "DoNotRoute",
    "KeepAlive",
    "ReuseAddress",
    "OutofBandInline",
    "SendBuffSize",
    "SendLowWatermark",
    "RecvBuffSize",
    "RecvLowWatermark",
    "SocketError"
};

class SocketBase
{
public:
    SocketBase() = default;
    SocketBase(SocketBase const &) = delete;
    SocketBase(SocketBase &&) = delete;
    SocketBase &operator=(SocketBase const &) = delete;
    SocketBase &operator=(SocketBase &&) = delete;
    
    using Broadcast = SockOption::BoolOption<SOL_SOCKET, SO_BROADCAST>;
    using Debug = SockOption::BoolOption<SOL_SOCKET, SO_DEBUG>;  
    using DoNotRoute = SockOption::BoolOption<SOL_SOCKET, SO_DONTROUTE>;  
    using KeepAlive = SockOption::BoolOption<SOL_SOCKET, SO_KEEPALIVE>;  
    using ReuseAddress = SockOption::BoolOption<SOL_SOCKET, SO_REUSEADDR>;  
    using OutofBandInline = SockOption::BoolOption<SOL_SOCKET, SO_OOBINLINE>;  
    using SendBuffSize = SockOption::IntOption<SOL_SOCKET, SO_SNDBUF>;  
    using SendLowWatermark = SockOption::IntOption<SOL_SOCKET, SO_SNDLOWAT>;  
    using RecvBuffSize = SockOption::IntOption<SOL_SOCKET, SO_RCVBUF>;  
    using RecvLowWatermark = SockOption::IntOption<SOL_SOCKET, SO_RCVLOWAT>;
    using SocketError = SockOption::IntOption<SOL_SOCKET, SO_ERROR>;

    /**
     * @brief Maximum length of pending incoming connection queue
     * 
     */
    static constexpr int kMaxListenConnections{SOMAXCONN};

protected:
    ~SocketBase() = default;
};


}



#endif /* D7B3D564_0000_4BB7_AFF9_C6B83759F6F8 */
