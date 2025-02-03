#ifndef A95C3023_EDB7_4B3B_95C6_218CC8B2C043
#define A95C3023_EDB7_4B3B_95C6_218CC8B2C043

#include "asrt/config.hpp"
#include "asrt/socket/protocol.hpp"
#include "asrt/socket/acceptor.hpp"
#include "asrt/socket/internet_endpoint.hpp"
#include "asrt/socket/basic_datagram_socket.hpp"
#include "asrt/timer/timer_queue.hpp"
#include "asrt/socket/socket_option.hpp"
#include "asrt/reactor/epoll_reactor.hpp"

/* explicit instantiations */
template class ExecutorNS::IO_Executor<ReactorNS::EpollReactor>;
template class Socket::BasicDgramSocket<ProtocolNS::UDP, ExecutorNS::IO_Executor<ReactorNS::EpollReactor>>;
template class IP::BasicEndpoint<ProtocolNS::UDP>;

namespace asrt::ip::udp{

using DefaultReactor = ReactorNS::EpollReactor;
using DefaultExecutor = ExecutorNS::IO_Executor<DefaultReactor>;
using ProtocolType = ProtocolNS::UDP;
using executor = asrt::config::DefaultExecutor;
using socket = Socket::BasicDgramSocket<ProtocolType, executor>;
using endpoint = IP::BasicEndpoint<ProtocolType>;
using reuse_addr = Socket::SocketBase::ReuseAddress;
using port = endpoint::PortNumber;


inline constexpr ProtocolType v4() noexcept
{
    return ProtocolType{AF_INET};
}

inline constexpr ProtocolType V6() noexcept
{
    return ProtocolType{AF_INET6};
}


}

#endif /* A95C3023_EDB7_4B3B_95C6_218CC8B2C043 */
