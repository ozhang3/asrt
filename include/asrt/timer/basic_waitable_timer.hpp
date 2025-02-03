#ifndef AE8EA98B_E336_4050_992B_8850BD707F5C
#define AE8EA98B_E336_4050_992B_8850BD707F5C


#include <cstdint>
#include <thread>
#include <memory>
#include <queue>

#include "asrt/common_types.hpp"
#include "asrt/timer/timer_types.hpp"
//#include "asrt/timer/timer_queue.hpp"
#include "asrt/sys/syscall.hpp"
#include "asrt/error_code.hpp"
#include "asrt/util.hpp"

namespace Timer{

using namespace Util::Expected_NS;
using namespace Util::Optional_NS;
using asrt::Result;

enum class TimerMode { kOneShot, kRecurring };

/**
 * @brief Implements functionalities of a basic waitable timer 
 * @param Clock: the type of clock associated with the timer instance
 * 
 */
template <
    typename Clock,
    typename Executor,
    TimerMode TimerModeType = TimerMode::kOneShot>
class BasicWaitableTimer {
public:
    typedef Clock ClockType;
    typedef Executor ExeucutorType;
    typedef Timer::Types::Nanoseconds DurationType;
    typedef typename Clock::time_point TimePointType;

    using ErrorCode = asrt::ErrorCodeType;

    BasicWaitableTimer() noexcept = delete;

    /**
     * @brief Construct a BasicWaitableTimer optionally specifying expiry and timer mode
     * 
     * @param executor 
     * @param duration 
     */
    explicit BasicWaitableTimer(Executor& executor, DurationType duration = {}) noexcept 
        : executor_{executor}, timer_manager_{executor.UseTimerService()}, period_{duration}
    {
        ASRT_LOG_TRACE("constructing basic timer");

        this->RegisterExpiryHandler()
        .map_error([](ErrorCode ec){
            LogFatalAndAbort("Failed to register expiry handler, {}", ec);
        });
    };

    /* non-copyable but movable */
    BasicWaitableTimer(BasicWaitableTimer const&) = delete;
    BasicWaitableTimer(BasicWaitableTimer&&) = default;
    BasicWaitableTimer &operator=(BasicWaitableTimer const &other) = delete;
    BasicWaitableTimer &operator=(BasicWaitableTimer &&other) = default;

    /**
     * @brief Disarms timer. Unregisters timer with timer queue.
     * 
     * @warning Destroying the timer when there are still outstanding async operation 
     *  associated with it is undefined behavior
     */
    ~BasicWaitableTimer() noexcept 
    {
        ASRT_LOG_TRACE("[Timer]: destructing timer");
        static_cast<void>(this->timer_manager_.RemoveTimer(this->timer_id_));
    }


    const ExeucutorType& GetExecutor() noexcept { return this->executor_; }

    /**
     * @brief Performs synchronous wait. Does not return until timer expires.
     * 
     * @note timer does not start counting down until one of its Wait() functions is invoked
     */
    void Wait() noexcept 
    {
        std::this_thread::sleep_for(this->period_); //internally calls nanosleep()
    }

    /**
     * @brief Invoke handler on timer expiry
     * 
     * @tparam ExpiryHandler 
     * @param handler 
     * @return Result<void> 
     */
    template <typename ExpiryHandler>
    auto WaitAsync(ExpiryHandler&& handler) noexcept -> Result<void> //todo
    {
        ASRT_LOG_TRACE("[Timer]: Initiaing async wait");
        
        TimePointType timer_expiry;
        DurationType timer_period;
   
        if(this->async_wait_in_progress_){
            return MakeUnexpected(ErrorCode::async_operation_in_progress); //
        }

        this->async_wait_in_progress_ = true;

        this->timer_handler_ = std::move(handler);

        timer_expiry = this->GetNextExpiry();
        
        if constexpr (TimerModeType == TimerMode::kRecurring) {
            timer_period = this->period_;
        }else{
            timer_period = DurationType{};
        }

        return this->timer_manager_.AddTimer(this->timer_id_, timer_expiry, timer_period)
            .map([this](){
                this->executor_.OnJobArrival(); /* inform executor of incoming async job */
            });
    }

    TimePointType Expiry() const noexcept
    {
        return this->expiry_;
    }

    /**
     * @brief Sets new expiry for the timer. Does not affect periodic timers.
     * 
     * @param duration 
     * @return Result<void> 
     */
    auto ExpiresAfter(DurationType duration) noexcept -> Result<void>
    {
        if(this->async_wait_in_progress_){
            return MakeUnexpected(ErrorCode::async_operation_in_progress); //
        }

        this->expiry_ = Clock::now() + duration;
        return Result<void>{};
    }

    /**
     * @brief Sets new expiry for the timer. Does not affect periodic timers.
     * 
     * @param time 
     * @return Result<void> 
     */
    auto ExpiresAt(TimePointType time) noexcept -> Result<void>
    {
        assert(time >= Clock::now());

        if(this->async_wait_in_progress_){
            return MakeUnexpected(ErrorCode::async_operation_in_progress); //
        }    

        this->expiry_ = time;
    }

private:
    using Handler = std::function<void()>;

    void OnTimerExpiry(Types::TimerTag timerid) noexcept
    {
        assert(timerid == this->timer_id_);

        ASRT_LOG_TRACE("[Timer]: On timer expiry"); 

        this->timer_handler_();
    }

    /**
     * @brief Get the next absolute expiry of this timer
     * 
     * @return TimePointType 
     */
    TimePointType GetNextExpiry() const noexcept
    {
        if(this->expiry_ > TimePointType{}){
            return this->expiry_;
        }else{
            return (this->period_.count() > 0) ? 
                (Clock::now() + this->period_) : TimePointType{};
        }
    }

    Result<void> RegisterExpiryHandler() noexcept
    {
        return this->timer_manager_.RegisterTimer(
                [this](Types::TimerTag timerid) -> void {
                    this->OnTimerExpiry(timerid);
                }
            )
            .map([this](Types::TimerTag timerid){
                ASRT_LOG_TRACE("[Timer]: Successful registration with timer queue");
                this->timer_id_ = timerid;
            });
    }

    //std::mutex basic_timer_mtx_; //todo is lock necessary if timer is intended for single-threaded use only? 
    Executor& executor_;
    typename Executor::TimerManager& timer_manager_;
    bool async_wait_in_progress_{false};
    TimePointType expiry_{};
    DurationType period_;
    Handler timer_handler_;
    Timer::Types::TimerTag timer_id_;
}; // class BasicWaitableTimer

template <typename C, typename E>
using BasicOneShotTimer = BasicWaitableTimer<C, E, TimerMode::kOneShot>;

template <typename C, typename E>
using BasicRecurringTimer = BasicWaitableTimer<C, E, TimerMode::kRecurring>;

} // end namespace Timer

#endif /* AE8EA98B_E336_4050_992B_8850BD707F5C */
