#ifndef ED194899_57EE_4AEC_B4A8_9FEF376ABD47
#define ED194899_57EE_4AEC_B4A8_9FEF376ABD47

#include "asrt/config.hpp"
#include "asrt/socket/basic_stream_socket.hpp"
#include "asrt/socket/basic_datagram_socket.hpp"
#include "asrt/socket/acceptor.hpp"
#include "asrt/socket/protocol.hpp"
#include "asrt/socket/unix_domain_endpoint.hpp"
#include "asrt/reactor/epoll_reactor.hpp"
#include "asrt/timer/timer_queue.hpp"
#include "asrt/executor/io_executor.hpp"

namespace Unix{
    
    using Executor = asrt::config::DefaultExecutor;
    using StreamProtocol = ProtocolNS::UnixStream;
    using DatagramProtocol = ProtocolNS::UnixDgram;

    using Acceptor = Socket::BasicAcceptorSocket<StreamProtocol, Executor>;
    using StreamSocket = Socket::BasicStreamSocket<StreamProtocol, Executor>;
    using DatagramSocket = Socket::BasicDgramSocket<DatagramProtocol, Executor>;


    static constexpr inline auto Stream() noexcept -> StreamProtocol
    {
        return StreamProtocol{};
    }

    static constexpr inline auto Datagram() noexcept -> DatagramProtocol
    {
        return DatagramProtocol{};
    }

    template <typename Protocol>
    using Endpoint = Unix::UnixDomainEndpoint<Protocol>;
}

template class Socket::BasicStreamSocket<Unix::StreamProtocol, Unix::Executor>;
//template class Socket::BasicDgramSocket<Unix::DatagramProtocol, Unix::Executor>;
template class Socket::BasicAcceptorSocket<Unix::StreamProtocol, Unix::Executor>;

#endif /* ED194899_57EE_4AEC_B4A8_9FEF376ABD47 */
