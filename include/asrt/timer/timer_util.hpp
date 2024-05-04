#ifndef D23FFBFE_3967_47F3_A3BC_523D81F92370
#define D23FFBFE_3967_47F3_A3BC_523D81F92370

#include <chrono>
#include <bits/types/struct_itimerspec.h>
#include <spdlog/spdlog.h>

namespace Timer::util{

using Nanoseconds = std::chrono::nanoseconds;
using Seconds = std::chrono::seconds;
using SteadyClock = std::chrono::steady_clock;
template <typename Clock>
using TimePointInNsec = std::chrono::time_point<Clock, std::chrono::nanoseconds>;


/**
 * @brief duration -> timespec
 * 
 * @param nsec 
 * @return ::timespec 
 */
inline constexpr auto ToTimeSpec(Nanoseconds nsec) noexcept -> ::timespec
{
    Seconds const sec{std::chrono::duration_cast<Seconds>(nsec)}; //lossy conversion; get integral seconds
    return {sec.count(), (nsec - sec).count()};
}

/**
 * @brief timepoint -> timespec
 * 
 * @tparam Clock 
 * @param timepoint 
 * @return ::timespec 
 */
template <typename Clock>
inline constexpr auto ToTimeSpec(TimePointInNsec<Clock> timepoint) noexcept -> ::timespec
{
    auto const sec{std::chrono::time_point_cast<std::chrono::seconds>(timepoint)}; //lossy conversion; get integral seconds
    auto const nsec{timepoint - std::chrono::time_point_cast<Nanoseconds>(sec)};
    return {sec.time_since_epoch().count(), nsec.count()};
}

/**
 * @brief timespec -> duration
 * 
 * @param time_spec 
 * @return std::chrono::nanoseconds 
 */
inline constexpr auto ToDuration(::timespec& time_spec) noexcept -> Nanoseconds
{
    const auto duration{Seconds{time_spec.tv_sec} + Nanoseconds{time_spec.tv_nsec}};
    return std::chrono::duration_cast<Nanoseconds>(duration);
}

/**
 * @brief timespec -> timepoint
 * 
 * @param time_spec 
 * @return TimePointInNsec<SteadyClock> 
 */
inline constexpr auto ToTimePoint(::timespec& time_spec) noexcept -> TimePointInNsec<SteadyClock>
{
    return TimePointInNsec<SteadyClock>{
        std::chrono::duration_cast<SteadyClock::duration>(ToDuration(time_spec))};
}

/**
 * @brief 
 * 
 * @param timepoint 
 * @return ::timespec 
 */
inline auto ToTimeSpecInterval(TimePointInNsec<SteadyClock> timepoint) noexcept -> ::timespec
{
    const auto interval_ns{timepoint - std::chrono::steady_clock::now()};
    return ToTimeSpec(interval_ns);
}

struct ScopedTimer{
    using Clock = std::chrono::steady_clock;
    using Time = std::chrono::time_point<Clock>;
    using FloatMilli = std::chrono::duration<float, std::ratio<1, 1000>>;

    ScopedTimer() noexcept
        : start_{Clock::now()} {}

    ~ScopedTimer() noexcept {
        spdlog::info("{}ms", std::chrono::duration_cast<FloatMilli>(Clock::now() - start_).count());
    }

    Time const start_;
};

struct ResuableTimer{
    using Clock = std::chrono::steady_clock;
    using Time = std::chrono::time_point<Clock>;
    using FloatMilli = std::chrono::duration<float, std::ratio<1, 1000>>;
    using Unit = FloatMilli;

    void Start() noexcept { start_ = Clock::now(); }
    
    auto GetElapsed() noexcept
    { 
        return std::chrono::duration_cast<FloatMilli>(Clock::now() - start_).count(); 
    }

    Time start_;
};

struct StopWatch{
    using Clock = std::chrono::steady_clock;
    using Time = std::chrono::time_point<Clock>;
    using FloatMilli = std::chrono::duration<float, std::ratio<1, 1000>>;

    StopWatch() noexcept : start_{Clock::now()} {}

    auto GetElapsed() noexcept {
        return std::chrono::duration_cast<FloatMilli>(Clock::now() - start_).count();
    }

    Time const start_;
};

}

#endif /* D23FFBFE_3967_47F3_A3BC_523D81F92370 */
