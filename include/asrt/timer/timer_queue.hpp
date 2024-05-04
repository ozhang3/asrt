#ifndef CF628894_E112_405D_BBAE_DC65E9D9563E
#define CF628894_E112_405D_BBAE_DC65E9D9563E

#include <cstdint>
#include <queue>
#include <vector>
#include <algorithm>

#include "asrt/config.hpp"
#include "asrt/executor/io_executor.hpp"
#include "asrt/executor/executor_task.hpp"
#include "asrt/util.hpp"
#include "asrt/error_code.hpp"
#include "asrt/sys/syscall.hpp"
#include "asrt/timer/timer_util.hpp"
#include "asrt/timer/timer_types.hpp"
#include "asrt/common_types.hpp"

namespace Timer{

namespace details{
    using namespace Timer::util;
}

using namespace Util::Expected_NS;

/**
 * @brief A thread-safe implementation of a priority queue for timers
 * 
 * @tparam Executor 
 */
template <typename Reactor>
class TimerQueue final : public ExecutorNS::TimerService<TimerQueue<Reactor>> 
{
public:
    using Executor = ExecutorNS::IO_Executor<Reactor>;
    using Handler = Timer::Types::EventHandler;
    using Clock = Timer::util::SteadyClock;
    using Expiry = Timer::Types::Expiry;
    using Duration = Timer::Types::Duration;
    using TimerTag = Types::TimerTag;
    using TimerIndex = std::underlying_type_t<TimerTag>;
    using TimerQSizeType = std::underlying_type_t<TimerTag>;
    using ErrorCode = ErrorCode_Ns::ErrorCode;
    using MutexType = typename Reactor::MutexType;
    using TimerUniqueLock = std::unique_lock<MutexType>;
    
    enum class TimerStatus{
        kPending,
        kComplete
    };

    enum class TimerChangeType{
        kAdd,
        kUpdate
    };
    //using ExecutorType = Executor;
    
    //using Reactor = ReactorNS::EpollReactor;
    template <typename T>
    using Result = Expected<T, ErrorCode>;

    struct TimerQueueEntry{
        Expiry expiry_{Types::kMaxExpiry}; /* max expiry to indicate invalid entry */
        Duration interval_{};
        Handler handler_;
        bool is_valid_;
        bool in_progress_{false};
    };

    struct QueuedTimerEntry{
        Expiry expiry_;
        TimerTag tag_;
    };

    TimerQueue(Executor& executor, TimerQSizeType size_hint = 25u) noexcept //todo
        : executor_{executor}, reactor_{executor.UseReactorService()}, timers_(Types::kMaxTimerCount) /* default initialize timer storage */
    {
        OsAbstraction::TimerFd_Create(CLOCK_MONOTONIC, TFD_CLOEXEC)
        .and_then([this, size_hint](asrt::NativeHandle timerfd){
            this->timer_fd_ = timerfd;
            return this->RegisterExpiryHandlerWithReactor();
        })
        .map([this, size_hint](){
            this->queued_timers_.reserve(size_hint);
        })
        .map_error([](ErrorCode ec){
            LogFatalAndAbort("Failed to construct timer queue, {}", ec);
        });
    }
    
    ~TimerQueue() noexcept {
        static constexpr ::itimerspec zero_timeout{};

        OsAbstraction::TimerFd_SetTime(timer_fd_, TFD_TIMER_ABSTIME , &zero_timeout, nullptr)
        .map_error([](ErrorCode ec){
            ASRT_LOG_ERROR("Failed to disarm timerfd, error: {}", ec);
        });
        
        /* ask reactor to release handler memory and closes timer fd 
            when it is safe to do so */
        //this->reactor_.DeregisterTimerHandler(this->reactor_handle_); //this causes double free as  reactor also tries to deallocate memory in its destructor
        
    }

    /**
     * @brief Obtain a TimerTag for a given timer handler
     * 
     * @param handler Must conform to void(TimerTag)
     * @return Result<TimerTag> 
     */
    auto Reserve(Handler&& handler) noexcept -> Result<TimerTag>
    {
        std::scoped_lock const lock{this->GetMutexUnsafe()};

        return this->GetNextAvailableTag()
            .map([this, &handler](TimerTag tag){
                auto& timer_to_reserve{this->timers_[ToTimerIndex(tag)]};
                timer_to_reserve.handler_ = std::move(handler);
                timer_to_reserve.in_progress_ = false; //todo necessary?
                return tag;
            });
    }

    /**
     * @brief Enqueues timer with given expiry and recurring interval
     * @details Check if timer is the next expiring timer. If yes update timerfd timeout
     * 
     * @param timer 
     * @param expiry 
     * @param interval 
     */
    auto Enqueue(TimerTag timer, Expiry expiry, Duration interval) noexcept -> Result<void>
    {

        TimerUniqueLock lock{this->GetMutexUnsafe()};

        if(expiry == Expiry{}) /* zero expiry timers */
        {   
            /* note we wont't allow zero-expiry zero-period timers (which doesn't make any sense)
                so if expiry == 0 we just ignore the interval parameter */
            (void)interval; 

            /* directly queue timer handler for executor invocation */
            this->HandleOneExpiry(lock, timer);

            return Result<void>{};
        }
       
        return this->DoAddTimer(timer, expiry, interval);
    }

    /**
     * @brief Removes timer from timer queue. All pending timers will still be called.
     * 
     * @param timer 
     */
    auto Dequeue(TimerTag timer) noexcept -> Result<void>
    {
        std::scoped_lock const lock(this->GetMutexUnsafe());

        return this->DoRemoveTimer(timer)
                .map([this, timer](){
                    this->RecycleTimerTag(timer);
                });
    }

private:

    using TimerStorage = std::vector<TimerQueueEntry>;
    using QueuedTimers = std::vector<QueuedTimerEntry>;
    using RecycledTimers = std::queue<TimerTag>;
    using ReactorHandle = typename Reactor::HandlerTag;

    MutexType& GetMutexUnsafe() noexcept
    {
        return *this->timerq_mutex_;
    }

    void HandleExpiry(ReactorHandle handle, TimerUniqueLock& lock) noexcept
    {
        assert(lock.owns_lock());
        ASRT_LOG_TRACE("[TimerQueue]: Handling expiry");
        assert(handle == this->reactor_handle_);

        ASRT_LOG_TRACE("[TimerQueue]: queue size {}", this->queued_timers_.size());

        auto timer{this->GetNextTimer()};
        while((this->queued_timers_.size() > 0) && this->IsExpired(timer)){
            ASRT_LOG_TRACE("[TimerQueue]: Timer {} expired? {}", timer, this->IsExpired(timer));
            this->HandleOneExpiry(lock, timer); /* rearms or removes timer depending on timer type */
            timer = this->GetNextTimer();
        }

        /* prime timerfd for next expiry */
        if(!this->queued_timers_.empty()){
            this->UpdateTimerFd(this->GetNextExpiry());
            /* a new asynchronous operation is started each time the timerfd is rearmed */
            this->executor_.OnJobArrival();
        }
    }

    /**
     * @brief arm/rearm a timer
     * 
     * @param timer 
     * @param expiry 
     * @param interval 
     * @return Result<void> 
     */
    Result<void> DoAddTimer(TimerTag timer, Expiry expiry, Duration interval) noexcept
    {
        Result<void> result{};
        ASRT_LOG_TRACE("[TimerQueue]: Adding timer {}", timer);
        /* assumes lock held */
        
        { /* update timer entry */
            auto& timer_to_update{this->timers_[ToTimerIndex(timer)]};
            timer_to_update.expiry_ = expiry;
            timer_to_update.interval_ = interval;
            timer_to_update.is_valid_ = true;
        }

        /* update timer fd if the timer to add is the next expiring timer */
        if(this->IsNextExpiring(expiry)){
            ASRT_LOG_TRACE("[TimerQueue]: (AddTimer) Updating timer fd for timer {}", timer);
            result = this->UpdateTimerFd(expiry);
        }

        this->queued_timers_.emplace_back(QueuedTimerEntry{expiry, timer});
        this->OnQueueUpdate();

        return result;
    }

    /**
     * @brief Update expiry/interval info for a reserved timer
     * 
     * @param timer 
     * @param expiry 
     * @param interval 
     */
    void UpdateTimerExpiry(TimerTag timer, Expiry expiry, Duration interval) noexcept
    {
        auto& timer_to_update{this->timers_[ToTimerIndex(timer)]};
        timer_to_update.expiry_ = expiry;
        timer_to_update.interval_ = interval;

        auto timer_it{std::find_if(this->queued_timers_.begin(), this->queued_timers_.end(),
            [this, timer, expiry](auto& timer_in_queue){
                if(timer_in_queue.tag_ == timer){
                    timer_in_queue.expiry_ = expiry;
                    this->OnQueueUpdate();
                    return true;
                }else return false;
            })};
        
        /* it's an API error if the timer is not found */
        assert(timer_it != this->queued_timers_.end());
    }

    /**
     * @brief Retrieves the upcoming expiry. Assumes timer queue not empty.
     * 
     * @return expiry value in nanoseconds
     */
    Expiry GetNextExpiry() const noexcept
    {
        return this->queued_timers_.front().expiry_;
    }

    /**
     * @brief Rearms timerfd with new timeout
     * @details There are three scenarios that (may) warrant a call to UpdateTimerFd():
     *          1. When a timer is first being added to the queue.
     *          2. Every time a timer is expired.
     *          3. After a timer is removed. 
     */
    Result<void> UpdateTimerFd(Expiry expiry) noexcept
    {
        bool zero_expiry{expiry.time_since_epoch().count() == 0};
        const ::itimerspec new_timeout{
            .it_interval = {}, /* do not set interval here. manually rearm timer if necessary instead */
            .it_value = zero_expiry ? ::timespec{} : util::ToTimeSpec(expiry)
        };

        /* this will trigger reactor run() to return when timer is expired */
        return OsAbstraction::TimerFd_SetTime(timer_fd_, TFD_TIMER_ABSTIME , &new_timeout, nullptr) 
            .map_error([](ErrorCode ec){
                ASRT_LOG_ERROR("[TimerQueue]: Failed to set timeout for timer! Error: {}", ec);
                return ec;
            });
    }

    /**
     * @brief 1. Disarm timerfd if necessary
     *        2. release handler memory if safe
     *        3. Update timer queue
     * @param timer 
     * @return Result<void> 
     */
    Result<void> DoRemoveTimer(TimerTag timer) noexcept
    {
        /* assumes lock held */

        ASRT_LOG_TRACE("[TimerQueue]: Removing timer {}", timer);

        { /* release handler memory */
            auto& timer_to_remove{this->timers_[ToTimerIndex(timer)]};

            if(timer_to_remove.is_valid_){
                if(!timer_to_remove.in_progress_){
                    Handler temp_handler = std::move(timer_to_remove.handler_);
                    /* do not recycle the tag just yet. 
                    leave that responsibility to Dequeue() */
                    ////this->RecycleTimerTag(timer_tag); 
                }else{ /* do it later when timer is expired */
                    this->release_handler_memory_ = true;
                }
                timer_to_remove.is_valid_ = false;
            }else{ /* timer already de-registered */
                return {}; //there's nothing to do we can exit now
            }
        }

        return this->RemoveTimerFromQueue(timer)
            .and_then([this, timer]() -> Result<void> {
                return this->IsNextExpiring(timer) ?
                    this->UpdateTimerFd(Expiry{}) : /* disarm timer */
                    Result<void>{};
            });
    }

    constexpr Result<void> RemoveTimerFromQueue(TimerTag timer) noexcept 
    {
        auto timer_in_queue{
                std::find_if(this->queued_timers_.begin(), this->queued_timers_.end(),
                    [timer](const auto& entry){
                        return entry.tag_ == timer;
                    }
            )};

        if(timer_in_queue != this->queued_timers_.end()) {
            *timer_in_queue = std::move(this->queued_timers_.back());
            this->queued_timers_.pop_back();
            this->OnQueueUpdate();
            return Result<void>{};
        }else{
            return MakeUnexpected(ErrorCode::api_error); /* tag doesn't belong to a timer that is known to this timer queue */
        }
    }

    Result<TimerTag> GetNextAvailableTag() noexcept
    {
        TimerTag tag;
        if(this->tag_end_ <= Types::kMaxTimerCount){ /* check there still is capacity */

            if(!this->timer_tag_recycle_bin_.empty()){ /* reuse a recycled tag when possible */
                tag = this->timer_tag_recycle_bin_.front();
                this->timer_tag_recycle_bin_.pop();
            }else{ /* just assign tag at end of tag container */
                tag = TimerTag{this->tag_end_};
                this->tag_end_++;
            }

            ASRT_LOG_TRACE("[TimerQueue]: Reserved tag {} for timer",
                std::underlying_type_t<TimerTag>(tag));

        }else{
            return MakeUnexpected(ErrorCode::capacity_exceeded);
        }
        
        return tag;
    }

    void HandleOneExpiry(TimerUniqueLock& lock, TimerTag tag) noexcept
    {
        /* assumes lock held */ /* Lock is held at function entry */

        auto& expired_timer{this->timers_[ToTimerIndex(tag)]};
        expired_timer.in_progress_ = true; /* this ensures the timer and its handler stays valid after the mutex is unlocked */
        ASRT_LOG_TRACE("[TimerQueue]: Calling Timer {} OnTimerExpiry()", tag);

        lock.unlock(); /* unlock to avoid deadlock when calling handler */

        auto prev_expiry{Clock::now()}; /* take an early timestamp to account for time spent invoking the handler */ //todo
        expired_timer.handler_(tag); /* call Timer.OnTimerExpiry() */

        lock.lock();
        expired_timer.in_progress_ = false;

        if(this->release_handler_memory_){ /* this is our cue (no pun intended ;P) to release handler memory */
            /* arriving here means the timer has already been removed from queued_timers
                also timerfd has already been disarmed so the only thing left to do 
                is to release memory and recycle tag */
            Handler placeholder;
            ASRT_LOG_TRACE("Releasing timer handler memory for timer {}", tag);
            placeholder = std::move(expired_timer.handler_);
            this->release_handler_memory_ = false;
            return; 
        }

        if(expired_timer.interval_.count() == 0){ /* one-shot timer */
            ASRT_LOG_TRACE("Removing expired timer {} from queue", tag);
            this->RemoveTimerFromQueue(tag);
        }else{ /* recurring timer */
            ASRT_LOG_TRACE("Rearming timer {}", tag);
            auto new_expiry{prev_expiry + expired_timer.interval_};
            this->UpdateTimerExpiry(tag, new_expiry, expired_timer.interval_);
        }
    }

    Result<void> RegisterExpiryHandlerWithReactor() noexcept
    {
        return this->reactor_.RegisterTimerHandler(this->timer_fd_,
                [this](ReactorHandle handle, TimerUniqueLock& lock) -> void {
                    this->HandleExpiry(handle, lock);
                }
            )
            .map([this](typename Reactor::ReactorRegistry registry) {
                this->reactor_handle_ = registry.tag;
                this->timerq_mutex_ = &(registry.mutex);
                ASRT_LOG_TRACE("[TimerQueue]: Reactor handle: {:#x}", 
                    this->reactor_handle_);
            });
    }

    TimerTag GetNextTimer() const noexcept
    {
        return (this->queued_timers_.size() == 0) ?
                Timer::Types::kInvalidTimerTag :
                this->queued_timers_.front().tag_;
    }

    static TimerIndex ToTimerIndex(TimerTag tag) noexcept
    {
        return static_cast<TimerIndex>(tag); //todo for now timer tag simply equates with the timer position in underlying storage but this may change in the future
    }

    typename TimerStorage::iterator ToTimerIt(TimerTag tag) const noexcept 
    {
        return this->timers_.begin() + ToTimerIndex(tag);
    }

    bool IsExpired(TimerTag timer) const noexcept //todo timer validity check
    {
        auto expiry{this->timers_[ToTimerIndex(timer)].expiry_};
        return IsTimerValid(timer) ? (expiry <= Clock::now()) : false;
    }

    bool IsTimerValid(TimerTag timer) const noexcept
    {
        return timer != Timer::Types::kInvalidTimerTag;
    }

    /**
     * @brief Checks whether the expiry to check is sooner than that of the earliest expiry in queue
     * 
     * @pre Assumes the queue is heapified and that the expiry has not yet been enqueued 
     */
    bool IsNextExpiring(Expiry expiry) const noexcept
    {
        /* do not report true if the expiry evaluates to be the same 
            as that of the next expiry to avoid a unnecessary subsequent syscall */
        return this->queued_timers_.empty() ?
            true : (expiry < this->GetNextExpiry()); 
        //! assumes that the timerfd never notifies us earlier than we asked for (later is ok)
    }

    auto GetTimerExpiry(TimerTag tag) const noexcept
    {
        return this->timers_[tag].expiry_;
    }

    /**
     * @brief Checks whether the timer is the next one to expire
     * 
     * @pre Assumes the queue is non-empty and heapified and that the timer is already in queue
     * @return false if not next expiring
     */
    bool IsNextExpiring(TimerTag tag) const noexcept
    {
        /* do not report true if the expiry evaluates to be the same 
            as that of the next expiry to avoid a unnecessary subsequent syscall */
        return tag == this->queued_timers_.front().tag_; 
        //! assumes that the timerfd never notifies us earlier than we asked for (later is ok)
    }

    /**
     * @brief Needs to be called whenvever queued_timers_ is updated
     * @details Heapifies the timer queue so that the timer with least expiry bubbles up
     */
    void OnQueueUpdate() noexcept
    {
        std::make_heap(this->queued_timers_.begin(), this->queued_timers_.end(), 
            [](const QueuedTimerEntry& timer1, const QueuedTimerEntry& timer2){
                return timer1.expiry_ > timer2.expiry_; /* entry with smaller expiry bubbles up */
            });
    }

    /**
     * @brief Make timer tag available for next reservation
     * 
     * @warning Recycling an already recycled tag is undefined behavior
     * 
     * @param tag the tag to return to pool of available tags
     */
    void RecycleTimerTag(TimerTag tag) noexcept
    {
        //assert(!this->timer_tag_recycle_bin_.contains(tag));
        ASRT_LOG_TRACE("tag {} recycled", tag);
        this->timer_tag_recycle_bin_.push(tag);
    }

    Executor& executor_;
    Reactor& reactor_;
    ReactorHandle reactor_handle_{Reactor::kInvalidReactorHandle};
    asrt::NativeHandle timer_fd_{asrt::kInvalidNativeHandle};
    TimerStorage timers_;
    RecycledTimers timer_tag_recycle_bin_;

    /**
     * @brief A collection of timers with pending asynchronous operations
     */
    QueuedTimers queued_timers_;

    MutexType* timerq_mutex_;
    
    /**
     * @brief points to the last used timer in timer storage
     */
    TimerIndex tag_end_{};

    bool release_handler_memory_{false};
};


}
#endif /* CF628894_E112_405D_BBAE_DC65E9D9563E */
