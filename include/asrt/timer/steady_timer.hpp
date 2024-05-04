#ifndef DD3C12F0_8A08_4FEA_8F74_D35358B182D8
#define DD3C12F0_8A08_4FEA_8F74_D35358B182D8

#include <chrono>
#include "asrt/config.hpp"
#include "asrt/timer/basic_timer.hpp"
#include "asrt/reactor/epoll_reactor.hpp"
#include "asrt/executor/io_executor.hpp"

namespace Timer{

    using SteadyTimer = BasicTimer<std::chrono::steady_clock>;
    using TimerMode = SteadyTimer::TimerMode;
}

#endif /* DD3C12F0_8A08_4FEA_8F74_D35358B182D8 */
