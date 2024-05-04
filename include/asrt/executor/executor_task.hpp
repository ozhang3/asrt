#ifndef C6AE3CC8_F584_4A93_81E4_93F33758AB6C
#define C6AE3CC8_F584_4A93_81E4_93F33758AB6C

#include <cstdint>
#include <deque>
#include <functional>

#include "asrt/type_traits.hpp"
#include "asrt/util.hpp"
#include "asrt/error_code.hpp"
#include "asrt/reactor/types.hpp"
#include "asrt/executor/types.hpp"
#include "asrt/timer/timer_types.hpp"
#include "asrt/executor/details.hpp"

#include "asrt/reactor/types.hpp"



namespace ExecutorNS{

using ReactorNS::Types::ReactorRegistry;
using ReactorNS::Types::EventHandlerLockType;
using ReactorNS::Types::Events;
using ReactorNS::Types::HandlerTag;

template <typename Reactor, typename Timer>
class Executor;

struct NullReactor;
struct NullTimer;

template <typename T> using Result = tl::expected<T, ErrorCode_Ns::ErrorCode>;

template <class Reactor>
class ReactorService
{
public:

  auto Run(int timeout_ms, OperationQueue& op_queue) -> Result<ReactorUnblockReason>
  {
    return Implementation().HandleEvents(timeout_ms, op_queue);
  }

  void Wakeup()
  {
    Implementation().Unblock();
  }

  template <typename Handler>
    requires asrt::MatchesSignature<Handler, void(EventHandlerLockType, Events, HandlerTag)>
  auto Register(Handler&& h) -> Result<ReactorRegistry>
  {
    return Implementation().RegisterSoftwareEventHandler(std::forward<Handler>(h));
  }

  auto Invoke(HandlerTag tag) -> Result<void>
  {
    return Implementation().TriggerSoftwareEvent(tag);
  }

private:
  constexpr Reactor& Implementation() { return static_cast<Reactor &>(*this); }
};

template <class TimerQueue>
class TimerService
{
public:
  using TimerTag = Timer::Types::TimerTag;
  using Expiry = Timer::Types::Expiry;
  using Duration = Timer::Types::Duration;

  //TimerService(IO_Executor& executor, std::uint8_t concurrent_timers_count) noexcept {};

  template <typename TimerHandler>
  auto RegisterTimer(TimerHandler&& handler) -> Result<TimerTag>
  {
    return Implementation().Reserve(std::move(handler));
  }

  auto AddTimer(TimerTag timer, Expiry expiry, Duration interval) -> Result<void>
  {
    return Implementation().Enqueue(timer, expiry, interval);
  }

  auto RemoveTimer(TimerTag timer)
  {
    return Implementation().Dequeue(timer);
  }

private:
  constexpr TimerQueue& Implementation() { return static_cast<TimerQueue &>(*this); }
};

}

#endif /* C6AE3CC8_F584_4A93_81E4_93F33758AB6C */
