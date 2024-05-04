// Copyright(c) 2022-present, Allan Zhang (oulunzhang@126.com)
// Distributed under the MIT License (http://opensource.org/licenses/MIT)
// ASRT library version 0.9.0

#ifndef D5FEF4F2_44E5_40A2_8ED2_1CFE8826AF96
#define D5FEF4F2_44E5_40A2_8ED2_1CFE8826AF96

#include <memory>

#include "asrt/config.hpp"
#include "asrt/executor/io_executor.hpp"
#include "asrt/reactor/epoll_reactor.hpp"
#include "asrt/reactor/io_uring_reactor.hpp"
#include "asrt/details/registry.hpp"

namespace asrt {

using Executor = details::ExecutorRegistry::Executor;

inline auto DefaultExecutor() noexcept -> std::shared_ptr<Executor>
{
    return details::ExecutorRegistry::Instance().GetDefaultExecutor();
}

inline auto DefaultExecutorRaw() noexcept -> Executor*
{
    return details::ExecutorRegistry::Instance().GetDefaultExecutorUnsafe();
}

inline void SetDefaultExecutor(std::shared_ptr<Executor> new_executor) noexcept 
{
    return details::ExecutorRegistry::Instance().SetDefaultExecutor(new_executor);
}

template <typename Task>
inline void Post(Task&& task) noexcept 
{
    DefaultExecutorRaw()->Post(std::forward<Task>(task));
}

template <typename PeriodicTask, typename Period>
inline auto PostPeriodic(Period period, PeriodicTask&& task) noexcept 
{
    return DefaultExecutorRaw()->PostPeriodic(period, std::forward<PeriodicTask>(task));
}

} //end ns asrt

#endif /* D5FEF4F2_44E5_40A2_8ED2_1CFE8826AF96 */
