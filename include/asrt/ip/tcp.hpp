#ifndef A95C3023_EDB7_4B3B_95C6_218CC8B2C043
#define A95C3023_EDB7_4B3B_95C6_218CC8B2C043

#include "asrt/config.hpp"
#include "asrt/socket/protocol.hpp"
#include "asrt/socket/acceptor.hpp"
#include "asrt/socket/internet_endpoint.hpp"
#include "asrt/socket/basic_stream_socket.hpp"
#include "asrt/timer/timer_queue.hpp"
#include "asrt/socket/socket_option.hpp"
#include "asrt/reactor/epoll_reactor.hpp"

/* explicit instantiations */
template class ExecutorNS::IO_Executor<ReactorNS::EpollReactor>;
template class Socket::BasicStreamSocket<ProtocolNS::TCP, ExecutorNS::IO_Executor<ReactorNS::EpollReactor>>;
template class Socket::BasicAcceptorSocket<ProtocolNS::TCP, ExecutorNS::IO_Executor<ReactorNS::EpollReactor>>;
template class IP::BasicEndpoint<ProtocolNS::TCP>;

namespace ip::tcp{

using DefaultReactor = ReactorNS::EpollReactor;
using DefaultExecutor = ExecutorNS::IO_Executor<DefaultReactor>;
using ProtocolType = ProtocolNS::TCP;
using executor = asrt::config::DefaultExecutor;
using socket = Socket::BasicStreamSocket<ProtocolType, executor>;
using acceptor = Socket::BasicAcceptorSocket<ProtocolType, executor>;
using endpoint = IP::BasicEndpoint<ProtocolType>;
using no_delay = SockOption::BoolOption<IPPROTO_TCP, TCP_NODELAY>;
using reuse_addr = Socket::SocketBase::ReuseAddress;
using port = endpoint::PortNumber;


static inline constexpr auto 
v4() noexcept -> ProtocolType
{
    return ProtocolType{AF_INET};
}

static inline constexpr auto 
v6() noexcept -> ProtocolType
{
    return ProtocolType{AF_INET6};
}


}

#endif /* A95C3023_EDB7_4B3B_95C6_218CC8B2C043 */
