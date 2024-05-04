#include "asrt/executor/io_executor.hpp"

namespace ExecutorNS{

template <class Reactor>
inline IO_Executor<Reactor>::
IO_Executor(ExecutorConfig config, std::size_t concurrency_hint) noexcept
    :
#ifdef EXECUTOR_HAS_THREADS
          single_thread_{concurrency_hint == 1} 
#else
          single_thread_{concurrency_hint_ == 1 ? true : false} 
#endif
{
    if constexpr (std::is_same_v<MutexType, std::mutex>) {
        ASRT_LOG_INFO("[IO_Executor]: Using std::mutex to provide full thread safety");
    }else{
        ASRT_LOG_WARN("[IO_Executor]: Locking disabled for executor. Use with caution.");
    }

    if(static_cast<int>(config) & static_cast<int>(ExecutorConfig::ENABLE_REACTOR_SERVICE))
        this->UseReactorService();

    if(static_cast<int>(config) & static_cast<int>(ExecutorConfig::ENABLE_TIMER_SERVICE))
        this->UseTimerService();
}  

template <class Reactor>
inline auto IO_Executor<Reactor>::
CancelTimedJob(PeriodicTaskId task) noexcept -> Result<void>
{
    if(not this->IsPeriodicJobValid(task)) {
        return MakeUnexpected(ErrorCode::timer_not_exist);
    }

    return this->RemoveTimedTaskAsync(task)
        .map([this](){
            this->OnJobCompletion();
        });
}

template <class Reactor>
inline auto IO_Executor<Reactor>::
Run(void) noexcept -> Result<std::size_t>
{
    ASRT_LOG_TRACE("Initiating executor Run()");
    if(this->job_count_.load(std::memory_order_acquire) == 0){
        this->Stop();
        return 0;
    }

    ThreadInfo this_thread;
    ExecutionContext<ThreadInfo> ctx_{this_thread};

    std::unique_lock<MutexType> lock{this->mtx_};

    std::size_t jobs_processed{};
    for(; jobs_processed <= (std::numeric_limits<std::size_t>::max)(); jobs_processed++){ /* eplicit Stop() by user or ran out of jobs */
        Result<ProcessStatus> const result{this->ProcessNextOperation(lock, this_thread)}; /* blocks if no operation is available */

        if(!result.has_value()){
            ASRT_LOG_ERROR("Got error during operation processing: {}", result.error());
            return MakeUnexpected(result.error());
        }else{
            if(result.value() == ProcessStatus::kJobProcessed)
                ASRT_LOG_DEBUG("processed one operation");
            else //stop requested
                break;
            //just process the next task in next loop iteration
        }
    }

    ASRT_LOG_INFO("[Io_Executor]: Stopped, processed {} job(s)", jobs_processed);
    return jobs_processed;
}

template <class Reactor>
inline void IO_Executor<Reactor>::
RunOne() noexcept
{
    ASRT_LOG_TRACE("Initiating executor RunOne()");
    if(this->job_count_.load(std::memory_order_acquire) == 0){
        this->Stop();
        return;
    }

    ThreadInfo this_thread;
    ExecutionContext<ThreadInfo> ctx_{this_thread};

    std::unique_lock<MutexType> lock{this->mtx_};
    if(!this->operation_queue_.empty()){
        auto result{this->ProcessNextOperation(lock, this_thread)}; /* blocks if no operation is available */
        if(!result.has_value()){
            ASRT_LOG_ERROR("Got error during operation processing: {}", result.error());
        }else{
            if(result.value() == ProcessStatus::kJobProcessed)
                ASRT_LOG_DEBUG("processed one operation");
            else {}//stop requested
        }
    }
}

template <class Reactor>
inline auto IO_Executor<Reactor>::
UseReactorService() noexcept -> Reactor&
{
    ASRT_LOG_DEBUG("Using reactor service");
    static_assert(!std::is_same_v<Reactor, ReactorNS::NullReactor>, "Instantiating a NullReactor is prohibited!");

    static std::once_flag reactor_init_flag;
    std::call_once(reactor_init_flag, [this](){ /* to protect against concurrent access on the singleton */
        if(!reactor_service_.has_value()){
            /* failed service construction will trigger an abort */
            this->reactor_service_.emplace(*this, kReactorHandlerCount);
            this->has_reactor_service_ = true;
            ASRT_LOG_TRACE("Executor now has valid reactor");
            this->StartReactorTask(); /* make sure only one start task is enqueued in case of multiple reactor users */
        }
    });
    /* the following cast is valid since we just constructed a known reactor service in our reactor */
    return static_cast<Reactor &>(this->reactor_service_.value());
} 

template <class Reactor>
inline auto IO_Executor<Reactor>::
UseTimerService() noexcept -> TimerManager&
{
    static_assert(!std::is_same_v<TimerManager, Timer::NullTimer>, "Instantiating a NullTimer is prohibited!");

    ASRT_LOG_DEBUG("Using timer service");

    static std::once_flag timer_init_flag;
    std::call_once(timer_init_flag, [this](){
        if(!this->timer_manager_.has_value()) [[likely]] {
            /* failed service construction will trigger an abort */
            this->timer_manager_.emplace(*this, kConcurrentTimerCountHint);
            this->has_timer_service_ = true;
            ASRT_LOG_TRACE("Executor now has valid timer");
        }
    });
    
    return static_cast<TimerManager &>(this->timer_manager_.value());
} 

template <class Reactor>
inline void IO_Executor<Reactor>::
Stop() noexcept
{
    ASRT_LOG_TRACE("Stopping...");
    std::scoped_lock const lock{this->mtx_};
    this->stop_requested_ = true;
    this->WakeAll();
}

template <class Reactor>
inline void IO_Executor<Reactor>::
Restart() noexcept
{
    ASRT_LOG_INFO("Restarting...");
    std::scoped_lock const lock{this->mtx_};
    this->stop_requested_ = false;
}

template <class Reactor>
inline void IO_Executor<Reactor>::
OnJobArrival() noexcept
{
    this->job_count_.fetch_add(1, std::memory_order_acq_rel);
    ASRT_LOG_TRACE("OnJobArrival(), job count {}", this->job_count_.load(std::memory_order_acquire));
}

template <class Reactor>
inline void IO_Executor<Reactor>::
OnJobCompletion() noexcept
{
    ASRT_LOG_TRACE("OnJobCompletion");
    if(this->job_count_.fetch_sub(1, std::memory_order_acq_rel) == 0){
        this->Stop();
    }
}

/********************* private methods *********************/

template <class Reactor>
inline auto IO_Executor<Reactor>::
ProcessNextOperation(std::unique_lock<MutexType>& lock, ThreadInfo& this_thread) noexcept -> Result<ProcessStatus>
{
    if constexpr (EXECUTOR_HAS_THREADS == false) (void)this_thread;

    assert(lock.owns_lock()); /* Lock is held at function entry */

    while(!stop_requested_) [[likely]] {

        if(not this->operation_queue_.empty()){

            ASRT_LOG_TRACE("Processing one operation... job count: {}", this->job_count_.load(std::memory_order_acquire));
            ExecutorOperation op{std::move(this->operation_queue_.front())}; /* handler memory is released at block exit */
            this->operation_queue_.pop_front();
            const bool has_pending_jobs{!this->operation_queue_.empty()};

            if(this->IsNullTask(op)){ /* NullTask indicates start of reactor task processing and is not considered actual work */
                assert(this->reactor_service_.has_value()); //assert reactor is available before calling reactor Run()

                ASRT_LOG_TRACE("Got null operation, queue size {}", this->operation_queue_.size());
                /* notify other threads (if any) of unhandled work in queue */
                if(!single_thread_ && !this->operation_queue_.empty()){ 
                    this->WakeOne();
                }

                this->reactor_needs_interrupt_ = !has_pending_jobs; /* if queue empty, run is likely to block */

                lock.unlock(); /* release lock since the following call may block  */

                /* only one thread gets to execute the reactor Run() */ //todo how to ensure this?
                /* Only block if there are no other pending jobs in queue and we are not polling 
                    otherwise we want to return asap */
                const auto run_result{
                    this->reactor_service_.value().Run(
                        has_pending_jobs ? 0 : -1, 
                        EXECUTOR_HAS_THREADS ? this_thread.private_op_queue_ : this->operation_queue_) //todo check needs to be done at compile time
                };
                
                if(run_result.has_value()) [[likely]] {
                    ASRT_LOG_TRACE("Returned from reactor Run(), operation queue size {}", this->operation_queue_.size());
                }else [[unlikely]]{
                    ASRT_LOG_ERROR("Reactor Run() returned error: {}", run_result.error());
                    return MakeUnexpected(run_result.error());
                }

                if constexpr (EXECUTOR_HAS_THREADS) {
                    this->job_count_.fetch_add(this_thread.private_job_count_, std::memory_order_acq_rel);
                    this_thread.private_job_count_ = 0;
                }

                lock.lock(); /* reacquire lock for next iteration */

                if constexpr (EXECUTOR_HAS_THREADS) {
                    if(!this_thread.private_op_queue_.empty()) [[likely]] {
                        ASRT_LOG_TRACE("Transferring {} operation(s) from private queue to shared queue after reactor run",
                            this_thread.private_op_queue_.size());
                        std::ranges::move(this_thread.private_op_queue_, std::back_inserter(this->operation_queue_));
                        this_thread.private_op_queue_.clear();
                    }
                }
                /* race condition possible where flag is read before being set (since lock is released during reactor run operation), 
                    ie: some Post() operation thinks the reactor is inside Run() (and in need of interrupt when in fact it already returned from it)
                    but before acquiring the lock, thereby leading to a false wakeup */ //todo
                this->reactor_needs_interrupt_ = false;

                this->operation_queue_.push_back(nullptr); /* trigger reactor run after all operations are handled */
            }else{ /* we have actual work to do */

                ASRT_LOG_TRACE("Got job, queue size {}", this->operation_queue_.size());
                
                if constexpr (EXECUTOR_HAS_THREADS) {
                    if(!single_thread_ && has_pending_jobs){ /* there's still work left in the queue */
                        this->WakeOne();
                    }
                }
                
                lock.unlock(); /* release lock here to allow Executor APIs to be invoked from within the operation to avoid deadlocks */
                
                ASRT_LOG_TRACE("Calling operation...");
                op();

                if constexpr (EXECUTOR_HAS_THREADS) {
                    if(this_thread.private_job_count_ > 1) {
                        this->job_count_.fetch_add(this_thread.private_job_count_ - 1, std::memory_order_acq_rel);
                    }else if (this_thread.private_job_count_ == 0) {
                        this->OnJobCompletion(); /* stop executor on completion of all pending jobs */
                    } 

                    this_thread.private_job_count_ = 0;
                }

                lock.lock(); /* re-acquire lock so that this function maybe safely called in a loop */

                ASRT_LOG_TRACE("Called operation. Queue size: {}, job count {}", 
                    this->operation_queue_.size(), this->job_count_.load(std::memory_order_acquire));

                if constexpr (EXECUTOR_HAS_THREADS) {
                    if(!this_thread.private_op_queue_.empty()) [[unlikely]] {
                        ASRT_LOG_TRACE("Transferring {} operation(s) from thread priv queue to shared queue after invoking handler",
                            this_thread.private_op_queue_.size());

                        std::ranges::move(this_thread.private_op_queue_, 
                            std::back_inserter(this->operation_queue_));

                        this_thread.private_op_queue_.clear();
                    }
                }
                return ProcessStatus::kJobProcessed; /* processed one task; exit now */
            }
        }else{ /* operation queue empty */
            //assert(lock.owns_lock());
            this->cv_wait_count_++;
            ASRT_LOG_TRACE("Queue empty about to block waiting for incoming events");
            this->cv_.wait(lock, [this](){return not this->operation_queue_.empty();});  /* block on condvar; lock is unlocked inside cv */
            this->cv_wait_count_--;
        }
    } //while (not stop requested)

    assert(lock.owns_lock()); /* Lock is held at function exit */
    return ProcessStatus::kStopped;  /* exit detected */
}

template <class Reactor>
template <typename TimedTask, typename Duration>
inline auto IO_Executor<Reactor>::
StartTimedTaskAsync(TimedTask&& task, Duration period, TimedTaskType task_type) noexcept -> Result<PeriodicTaskId>
{
    return this->RegisterTimedTask(std::move(task), task_type)
        .and_then([this, period, task_type](Timer::Types::TimerTag handle) -> Result<PeriodicTaskId> {
            const auto task_period{(task_type == TimedTaskType::kRecurring) ? period : Duration{}};
            return this->timer_manager_.value().AddTimer(handle, Clock::now() + period, task_period)
                .map([this, handle]() {
                    this->periodic_job_ids_.push_back(handle);
                    this->OnJobArrival();
                    return handle;
                });
        })
        .map_error([](ErrorCode ec){
            ASRT_LOG_ERROR(
                "Failed to register timer with timer queue, {}", ec);
            return ec;
        });
}


template <class Reactor>
template <typename TimedTask>
inline auto IO_Executor<Reactor>::
RegisterTimedTask(TimedTask&& task, TimedTaskType task_type) noexcept -> Result<Timer::Types::TimerTag>
{
    if(task_type == TimedTaskType::kOnce){
        return this->timer_manager_.value().RegisterTimer(
            this->MakeOneShotOperation(std::move(task)));
    }else{
        return this->timer_manager_.value().RegisterTimer(
            this->MakePeriodicOperation(std::move(task)));
    }
}

template <class Reactor>
template <typename TimedTask>
inline const auto IO_Executor<Reactor>::
MakeOneShotOperation(TimedTask&& task) noexcept
{
    return 
        [this, one_time_task = std::move(task)](typename TimerManager::TimerTag tag){
            one_time_task();
            (void)this->RemoveTimedTaskAsync(tag);
        };
}

template <class Reactor>
template <typename TimedTask>
inline const auto IO_Executor<Reactor>::
MakePeriodicOperation(TimedTask&& task) noexcept
{
    return 
        [this, periodic_task = std::move(task)](typename TimerManager::TimerTag tag){
            periodic_task();
        };
}
template <class Reactor>
inline auto IO_Executor<Reactor>::
RemoveTimedTaskAsync(PeriodicTaskId task_id) noexcept -> Result<void>
{
    /* assumes task id is already validated on function entry */
    Util::QuickRemoveOne(this->periodic_job_ids_, task_id);
    return this->timer_manager_.value().RemoveTimer(task_id);
}

template <class Reactor>
inline void IO_Executor<Reactor>::
StartReactorTask() noexcept
{
    /* assumes lock held */
    assert(this->reactor_service_.has_value()); /* this function should never be called without a valid reactor */
    
    if(!this->stop_requested_ ){
        this->operation_queue_.emplace_back(nullptr);
        ASRT_LOG_TRACE("start reactor operation (pushed null operation)");
        //this->cv_.notify_all(); //todo notify_one or notify_all?
        this->WakeOne(); /* interrupts blocking epoll_wait() */
    }
}

template <class Reactor>
inline void IO_Executor<Reactor>::
WakeOne() noexcept
{
    /* assumes lock held */

    /* notify worker thread if any */
    if(this->cv_wait_count_ != 0){
        this->cv_.notify_one(); //todo maybe unlock before notification?
    }else{
        if(this->reactor_service_.has_value() && this->reactor_needs_interrupt_){
            this->reactor_service_.value().Wakeup(); /* interrupts reactor blocked in epoll_wait */
        }
    }
}

template <class Reactor>
inline void IO_Executor<Reactor>::
WakeAll() noexcept
{
    /* assumes lock held */
    this->cv_.notify_all();
    if(this->reactor_service_.has_value() && this->reactor_needs_interrupt_){ //todo
        this->reactor_service_.value().Wakeup();
    }
}

} //end ns