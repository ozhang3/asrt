#ifndef E033C8EB_6AB9_4086_985F_B5A47C17D651
#define E033C8EB_6AB9_4086_985F_B5A47C17D651

#include "asrt/executor/io_executor.hpp"
#include "asrt/reactor/epoll_reactor.hpp"
#include "asrt/timer/timer_queue.hpp"

namespace ExecutorConfig{

// /**
//  * @brief Some config options that switches Executor behavior at compile time
//  * 
//  */
// using DefaultReactorService = ReactorNS::EpollReactor;
// using DefaultTimerService = Timer::TimerQueue<DefaultReactorService>;

// /**
//  * @brief A general-purpose executor specialization of the IO_Executor template. 
//  * Both reactor and timer services are provided. Executor users should default to using this exeuctor when possible.
//  * 
//  * @details This executor uses EpollReactor for io events demultiplexing and TimerQueue for timer events handling
//  */
// //using DefaultExecutor = ExecutorNS::IO_Executor<DefaultReactorService, DefaultTimerService>;

// /**
//  * @brief This executor is suitable for when only socket operations are required. Time-related services are not supported.
//  * 
//  * @details Executor chooses the underlying reactor implementation based on supplied template parameter
//  * 
//  * @tparam Reactor The reactor implementation to use. Eg: EpollReactor, IoUringReactor
//  */
// template <typename ReactorService>
// using SocketExecutor = ExecutorNS::IO_Executor<ReactorService, ExecutorNS::NullTimer>;

// /**
//  * @brief Fully custimizable executor templated upon both the reactor and the timer service. 
//  * 
//  * @tparam ReactorService 
//  * @tparam TimerService 
//  */
// template <typename ReactorService, typename TimerService>
// using Executor = ExecutorNS::IO_Executor<ReactorService, TimerService>;

};

#endif /* E033C8EB_6AB9_4086_985F_B5A47C17D651 */
