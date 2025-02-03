#ifdef ASRT_HEADER_ONLY
    #error "Cannot compile asrt.cpp when ASRT_HEADER_ONLY is defined!"
#endif

#include "asrt/impl/basic_acceptor_socket.ipp"
#include "asrt/impl/basic_packet_socket.ipp"
#include "asrt/impl/basic_signalset.ipp"
#include "asrt/impl/basic_stream_socket.ipp"
#include "asrt/impl/io_executor.ipp"
#include "asrt/impl/strand.ipp"
#include "asrt/impl/client_interface.ipp"
#include "asrt/impl/server_interface.ipp"

//todo more includes to add

/* explicit template instantiation for class IO_Executor */
#include "asrt/reactor/epoll_reactor.hpp"
template class ExecutorNS::IO_Executor<ReactorNS::EpollReactor>;

#include <chrono>
#include "asrt/timer/basic_waitable_timer.hpp"
namespace Timer {
    template <> class BasicWaitableTimer<
        std::chrono::steady_clock, 
        ExecutorNS::IO_Executor<ReactorNS::EpollReactor>, 
        TimerMode::kOneShot>;

    template <> class BasicWaitableTimer<
        std::chrono::steady_clock, 
        ExecutorNS::IO_Executor<ReactorNS::EpollReactor>, 
        TimerMode::kRecurring>;
}
