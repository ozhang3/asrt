#ifndef DD3C12F0_8A08_4FEA_8F74_D35358B182D8
#define DD3C12F0_8A08_4FEA_8F74_D35358B182D8

#include <chrono>
#include "asrt/config.hpp"
#include "asrt/timer/basic_waitable_timer.hpp"
#include "asrt/reactor/epoll_reactor.hpp"
#include "asrt/executor/io_executor.hpp"

namespace Timer{
    using TimerExecutor = ExecutorNS::IO_Executor<ReactorNS::EpollReactor>;
    using SteadyTimer = BasicOneShotTimer<std::chrono::steady_clock, TimerExecutor>;
    using SteadyPeriodicTimer = BasicRecurringTimer<std::chrono::steady_clock, TimerExecutor>;
}

#endif /* DD3C12F0_8A08_4FEA_8F74_D35358B182D8 */
