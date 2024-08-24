#include <span>
#include <spdlog/spdlog.h>
#include <asrt/socket/basic_datagram_socket.hpp>
#include <asrt/ip/udp.hpp>
#include <asrt/client_server/datagram_server.hpp>
//#include <asrt/config.hpp>

using namespace asrt::ip;
using asrt::Result;

struct ServerImpl
{
    ServerImpl(udp::Executor& executor, udp::Endpoint const& address)
        : server_{executor, address, *this} {}

    void OnMessage(udp::Endpoint const& peer, ClientServer::ConstMessageView message)
    {
        spdlog::info("From {}: {}", peer, spdlog::to_hex(message));
    }

    using Server = ClientServer::DatagramServer<
        udp::Executor, udp::ProtocolType, ServerImpl, 1500, &ServerImpl::OnMessage>;

    Server server_;    
};

int main() {

    spdlog::set_level(spdlog::level::trace);
    udp::Executor executor;
    udp::Endpoint ep{udp::v4(), 50000u};
    ServerImpl server{executor, ep};

    executor.Run();

}