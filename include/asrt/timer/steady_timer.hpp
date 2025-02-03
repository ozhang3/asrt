#ifndef DD3C12F0_8A08_4FEA_8F74_D35358B182D8
#define DD3C12F0_8A08_4FEA_8F74_D35358B182D8

#include <chrono>
#include "asrt/config.hpp"
#include "asrt/executor/io_executor.hpp"
#include "asrt/reactor/epoll_reactor.hpp"
#include "asrt/timer/basic_waitable_timer.hpp"

namespace Timer{

    using ClockType = std::chrono::steady_clock;
    using ExecutorType = ExecutorNS::IO_Executor<ReactorNS::EpollReactor>;
    using SteadyTimer = BasicOneShotTimer<ClockType, ExecutorType>;
    using SteadyPeriodicTimer = BasicRecurringTimer<ClockType, ExecutorType>;
    
}

#endif /* DD3C12F0_8A08_4FEA_8F74_D35358B182D8 */
