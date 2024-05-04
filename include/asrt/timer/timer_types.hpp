#ifndef CB153FD7_0121_4FDC_97FD_DAA6990F7BA6
#define CB153FD7_0121_4FDC_97FD_DAA6990F7BA6
#include <cstdint>
#include <mutex>

#include "asrt/config.hpp"
#include "asrt/timer/timer_util.hpp"

namespace Timer::Types{

    enum class TimerTag : std::uint8_t {};

    using TimerTagUnderlying = std::underlying_type_t<TimerTag>;
    using Timer::util::Nanoseconds;
    using Timer::util::SteadyClock;
    using Expiry = Timer::util::TimePointInNsec<SteadyClock>;
    using Duration = Nanoseconds;
    using EventHandler = std::function<void(TimerTag)>;

    static constexpr TimerTagUnderlying kMaxTimerCount{asrt::config::kMaxTimerQueueSize - 1};
    static constexpr TimerTag kInvalidTimerTag{std::numeric_limits<TimerTagUnderlying>::max()};
    static constexpr Expiry kMaxExpiry{Nanoseconds{std::numeric_limits<Nanoseconds::rep>::max()}};

}

#endif /* CB153FD7_0121_4FDC_97FD_DAA6990F7BA6 */
