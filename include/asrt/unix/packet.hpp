#ifndef A5595ADE_A10F_46D7_B971_D817E00AE664
#define A5595ADE_A10F_46D7_B971_D817E00AE664

#include "asrt/timer/timer_queue.hpp"
#include "asrt/reactor/epoll_reactor.hpp"
#include "asrt/executor/io_executor.hpp"
#include "asrt/socket/protocol.hpp"
#include "asrt/socket/packet_endpoint.hpp"
#include "asrt/socket/basic_packet_socket.hpp"
#include "asrt/config.hpp"

namespace Packet{

    using Raw = ProtocolNS::PacketRaw;
    using Dgram = ProtocolNS::PacketDgram;

    template <typename Protocol>
    using Endpoint = Endpoint_NS::PacketEndpoint<Protocol>;

    using Executor = asrt::config::DefaultExecutor;

    using RawSocket = Socket::PacketSocket::BasicPacketSocket<Raw, Executor>;
    using DatagramSocket = Socket::PacketSocket::BasicPacketSocket<Dgram, Executor>;

    using ErrorCode = ErrorCode_Ns::ErrorCode;
    
    using namespace Socket::PacketSocket::details;
}

#endif /* A5595ADE_A10F_46D7_B971_D817E00AE664 */
