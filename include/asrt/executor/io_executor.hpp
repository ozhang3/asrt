#ifndef FB4C9587_3948_4722_96D5_297415BA1CBB
#define FB4C9587_3948_4722_96D5_297415BA1CBB

#include <deque>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <memory>

#include "asrt/config.hpp"
#include "asrt/util.hpp"
#include "asrt/error_code.hpp"
#include "asrt/type_traits.hpp"
#include "asrt/executor/types.hpp"
#include "asrt/callstack.hpp"
#include "asrt/executor/executor_task.hpp"
#include "asrt/executor/details.hpp"
#include "asrt/executor/executor_interface.hpp"
#include "asrt/reactor/null_reactor.hpp"
#include "asrt/timer/timer_queue.hpp"
#include "asrt/timer/null_timer.hpp"

namespace ExecutorNS{

using namespace Util::Expected_NS;

/**
 * @brief Executes operations related to i/o objects, ie: sockets or timers
 * 
 * @tparam Reactor The underlying i/o event demultiplexer that handles socket and timer events
 * @tparam TimerManager Enables periodic task execution through this executor
 */
template <class Reactor>
class IO_Executor
{
public:
    using Self              = IO_Executor<Reactor>;
    using ReactorType       = Reactor;
    using TimerManager      = Timer::TimerQueue<Reactor>;
    using PeriodicTaskId    = Timer::Types::TimerTag;
    static constexpr PeriodicTaskId kInvalidPeriodicTaskId{Timer::Types::kInvalidTimerTag};

    using ScheduledJobId = ReactorNS::Types::HandlerTag;
    using Clock = Timer::Types::SteadyClock;
    using MutexType = asrt::config::ExecutorMutexType;

    enum class ExecutionMode : std::uint8_t { kBlockOnJobDepletion, kExitOnJobDepletion };

    enum class PeriodicExecutionMode : std::uint8_t { kImmediate, kDeferred };

    enum class ScheduledExecutionMode : std::uint8_t { kOneshot, kPersistent };

    enum class TimedTaskType : std::uint8_t {kOnce, kRecurring };

    enum class ProcessStatus : std::uint8_t { kJobProcessed, kStopped };

    enum class ExecutorConfig{
        DEFER_SERVICE_CONSTRUCTION = 0,
        ENABLE_REACTOR_SERVICE = 1,
        ENABLE_TIMER_SERVICE = 2,
        ENABLE_ALL_SERVICES = 3
    };

    explicit IO_Executor(
        ExecutorConfig config = ExecutorConfig::DEFER_SERVICE_CONSTRUCTION, 
        std::size_t concurrency_hint = 1) noexcept; //todo

    ~IO_Executor() noexcept = default; //todo

    /**
     * @brief Queue an operation for later execution
     * @details This function does not block. The execution of the operaration is guranteed not to occur within the function. 
     * 
     * @tparam Executable: must conform to signature void()
     * @param op the operation to be executed. Must NOT be an empty operation or a nullptr.
     */
    template <typename Executable>
        requires asrt::MatchesSignature<Executable, void()>
    void Post(Executable&& op) noexcept
    {
        ASRT_LOG_TRACE("Posting job");
        this->EnqueueOnJobArrival(std::forward<Executable>(op));
    }


    /**
     * @brief Tries to invoke the operation right away if inside executor Run(), otherwise Post()s it for later execution
     * 
     * @tparam Executable 
     * @param op 
     * @return void 
     */
    template <typename Executable>
        requires asrt::MatchesSignature<Executable, void()>
    void Dispatch(Executable&& op) noexcept
    {
        ASRT_LOG_TRACE("Dispatching job");
        this->DoInvokeOrEnqueueOnJobArrival(std::forward<Executable>(op));
    }

    //todo implement defer()

    /**
     * @brief Schedule an operation for later invocation (triggered by Invoke())
     * 
     * @tparam Executable 
     * @param op 
     * @param mode 
     * @return requires 
     */
    template <typename Executable>
        requires asrt::MatchesSignature<Executable, void()>
    void ScheduleOneShot(Executable&& op) noexcept
    {
        ASRT_LOG_TRACE("Scheduling job");
        this->DoScheduleOperation<ScheduledExecutionMode::kOneshot>(std::forward<Executable>(op))
        .map([this](){
            this->OnJobArrival();
        });
    }

    template <typename Executable>
        requires asrt::MatchesSignature<Executable, void()>
    void SchedulePersitent(Executable&& op) noexcept
    {
        ASRT_LOG_TRACE("Scheduling job");
        this->DoScheduleOperation<ScheduledExecutionMode::kPersistent>(std::forward<Executable>(op))
        .map([this](){
            this->OnJobArrival();
        });
    }

    auto CancelScheduled(ScheduledJobId job) noexcept -> Result<void>
    {
        return this->reactor_service_.value().DeregisterSoftwareEvent(job)
            .map([this](){
                this->OnJobCompletion();
            });
    }

    /**
     * @brief Trigger the scheduled operation
     * 
     * @param job_id 
     */
    Result<void> Invoke(ScheduledJobId job_id) noexcept
    {
        return this->DoInvokeScheduledOperation(job_id)
            .map([this](){
                this->WakeOne();
            });
    }

    /**
     * @brief Queue an operation for periodic execution. TimerServices must be enabled, ie: UseTimerService() prior to calling this API.
     * 
     * @tparam Executable: must conform to signature void()
     * @tparam Duration std::chrono::duration 
     * @param periodic_op the periodic operation to be executed. Must NOT be an empty operation or a nullptr.
     * @param period specifies the period of the periodic task
     * @param mode kDeferred: start execution at next period; kImmediate: execute as soon as possible
     * 
     * @return PeriodicTaskId: a unique id assigned to the periodic task. Can be used to cancel the task later.
     *          The task id is considered valid until the task is cancelled.
     */
    template <typename Executable, typename Duration>
    auto PostPeriodic(Duration period, Executable&& periodic_op,
        PeriodicExecutionMode mode = PeriodicExecutionMode::kDeferred) noexcept -> Result<PeriodicTaskId>
    {
        ASRT_LOG_TRACE("Posting periodic job");
        (void)this->UseTimerService(); /* enable timer services if not already enabled */

        std::scoped_lock const lock{this->mtx_};

        if(mode == PeriodicExecutionMode::kImmediate){ 
            this->operation_queue_.push_back(std::forward<Executable>(periodic_op));
            /* this is considered a separate job from the subsequent periodical jobs 
                therefore it warrants a separte job arrival indication */
            this->OnJobArrival();
            this->WakeOne();
        }

        /* returns a job id on success */
        return this->StartTimedTaskAsync(
            std::forward<Executable>(periodic_op), period, TimedTaskType::kRecurring);   
    }

    /**
     * @brief Cancels pending periodic tasks. Tasks already queued for execution are not affected.
     * 
     * @param task The unique task id associated with the task. Task id is no longer valid post cancellation.
     * @return Expected<void, ErrorCode> 
     */
    auto CancelTimedJob(PeriodicTaskId task) noexcept -> Result<void>;

    /**
     * @brief Queue an operation for delayed execution.
     * 
     * @tparam Executable signature: void()
     * @tparam Duration std::chrono::duration
     * @param period The length of time to delay 
     * @param periodic_op The operation to execute
     * @return Result<PeriodicTaskId> 
     */
    template <typename Executable, typename Duration>
    auto PostDeferred(Duration duration, Executable&& periodic_op) noexcept -> Result<PeriodicTaskId>
    {
        ASRT_LOG_TRACE("Posting deferred job");
        (void)this->UseTimerService(); /* enable timer services if not already enabled */
        std::scoped_lock const lock{this->mtx_};
        return this->StartTimedTaskAsync(
           std::forward<Executable>(periodic_op), 
            duration, 
            TimedTaskType::kOnce);  /* returns a job id on success */
    }


    /**
     * @brief Start the Executor's event processing loop. May block. 
     *  Must not start with an empty task queue. Returns when stopped.
     * 
     * @details The Run() function blocks until all work has finished and 
     * there are no more handlers to be dispatched, 
     * or until the io_executor has been stopped. 
     * Multiple threads may call the Run() function to set up a pool of threads 
     * from which the io_context may execute handlers. 
     * 
     * @return Expected<std::size_t, ErrorCode> returns number of tasks executed
     */
    auto Run(void) noexcept -> Result<std::size_t>;
    
    /**
     * @brief Process the first task in queue and return
     * 
     */
    void RunOne() noexcept;

    /**
     * @brief Called by i/o objects to obtain a shared reactor instance managed by this executor
     * 
     * @tparam Reactor 
     * @return Reactor& 
     */
    auto UseReactorService() noexcept -> Reactor&; 

    /**
     * @brief Returns a reference to the timer manager maintained by this executor
     * 
     * @return TimerManager& 
     */
    auto UseTimerService() noexcept -> TimerManager&;

    /**
     * @brief Signals the exectutor to stop. Does not block.
     * @details This function wakes up threads blocked in Run()/RunOne(), which should return as soon as the stop "signal" is caught.
     *          A stopped executor still maintains the unfinished (if any) operations with the possibility to be Restart()ed later
     */
    void Stop() noexcept;

    /**
     * @brief Restart the Executor in preparation for a subsequent Run() invocation.
     * 
     * @warning This function must not be called while there are any unfinished calls to the Run() function.
     */
    void Restart() noexcept;

    /**
     * @brief Indicates presense of pending jobs
    */
    void OnJobArrival() noexcept; //unsafe

    /**
     * @brief Decrement job count and stop executor on completing all jobs
     * @note make sure lock held on invovation !!!
    */
    void OnJobCompletion() noexcept;


    /**
     * @brief A thread safe API for underlying reactor to inject tasks directly into op queue
     * 
     * @tparam Operation: void()
     * @param op the op to enqueue
     */
    template <typename Operation>
    void EnqueueOperation(Operation&& op) noexcept
    {
        if(this->IsExecutorContext()){
            this->EnqueuePrivate(std::forward<Operation>(op));
            return;
        } 

        ASRT_LOG_TRACE("Pushing to shared queue");
        std::scoped_lock const lock{this->mtx_};
        this->operation_queue_.push_back(std::forward<Operation>(op));

        /* We don't call OnJobArrival() here. Instead 
            we're relying on the fact that job arrival had 
            already been accounted for when the async operation 
            was initiated by the underlying socket/timer */
    }
    
    template <typename Operation>
    void DoInvokeOrEnqueueOnJobArrival(Operation&& op) noexcept
    {
        if constexpr (EXECUTOR_HAS_THREADS) {
            /* check if we're inside executor Run() */
            if(ThreadInfo* thread_info{ExecutionContext<ThreadInfo>::RetrieveContent()}) {
                ASRT_LOG_TRACE("Directly invoking handler in executor context");
                std::forward<Operation>(op)();
                return;
            } 
        }

        this->OnJobArrival();
        std::scoped_lock const lock{this->mtx_};
        this->operation_queue_.push_back(std::forward<Operation>(op));
        this->WakeOne();
    }

    template <typename Operation>
    void EnqueueOnJobArrival(Operation&& op) noexcept
    {
        if constexpr (EXECUTOR_HAS_THREADS) {
            /* check if we're inside executor Run() */
            if(ThreadInfo* thread_info{ExecutionContext<ThreadInfo>::RetrieveContent()}) {
                ASRT_LOG_TRACE("Pushing to thread private queue");
                thread_info->private_job_count_++;
                thread_info->private_op_queue_.push_back(std::forward<Operation>(op));
                return;
            } 
        }

        this->OnJobArrival();
        std::scoped_lock const lock{this->mtx_};
        this->operation_queue_.push_back(std::forward<Operation>(op));
        this->WakeOne();
    }

    template <typename Operation>
    void EnqueuePostJobArrival(Operation&& op) noexcept
    {
        if constexpr (EXECUTOR_HAS_THREADS) {
            if(this->single_thread_){
                /* check if we're inside executor Run() */
                if(ThreadInfo* thread_info{ExecutionContext<ThreadInfo>::RetrieveContent()}) [[likely]] {
                    ASRT_LOG_TRACE("Pushing to thread private queue");
                    /* we do not increment job count here */
                    thread_info->private_op_queue_.push_back(std::forward<Operation>(op));
                    return;
                } 
            }
        }

        /* We don't call OnJobArrival() here. Instead 
        we're relying on the fact that job arrival had 
        already been accounted for when the async operation 
        was initiated by the underlying socket/timer */
        std::scoped_lock const lock{this->mtx_};
        this->operation_queue_.push_back(std::forward<Operation>(op));
        this->WakeOne();
    }

    template <ScheduledExecutionMode Mode, typename Operation>
    Result<ScheduledJobId> DoScheduleOperation(Operation&& op) noexcept
    {
        static const auto operation_callback {
            [op = std::move(op)](EventHandlerLockType lock, Events events, HandlerTag tag){
                assert(lock.owns_lock() && events.HasSoftwareEvent());
                lock.unlock();
                op();
                lock.lock();
            }};
            
       if constexpr (Mode == ScheduledExecutionMode::kOneshot) {
            return this->reactor_service_.value().RegisterOneShotSoftwareEvent(operation_callback);
        } else {
            return this->reactor_service_.value().RegisterPersistentSoftwareEvent(operation_callback);
        }
    }

    
    Result<void> DoInvokeScheduledOperation(ScheduledJobId job_id) noexcept
    {
        return this->reactor_service_.value().Invoke(job_id);
    }

    bool IsExecutorContext() const noexcept
    {
        return ExecutionContext<ThreadInfo>::IsInContext();
    }

private:

    /**
     * @brief Process the next operation in queue. Blocks if queue is empty.
     * @details Both jobs posted by user and operations enqueued by reactor are considfered valid operations
     * 
     * @note assumes lock held at function entry; may unlock during exectuion; re-acquries lock at exit
     * 
     * @param lock 
     * @return Expected<void, ErrorCode> 
     */
    Result<ProcessStatus> ProcessNextOperation(std::unique_lock<MutexType>& lock, ThreadInfo& this_thread) noexcept;

    template <typename TimedTask, typename Duration>
    Result<PeriodicTaskId> StartTimedTaskAsync(TimedTask&& task, Duration period, TimedTaskType task_type) noexcept;

    template <typename TimedTask>
    Result<Timer::Types::TimerTag> RegisterTimedTask(TimedTask&& task, TimedTaskType task_type) noexcept;

    template <typename TimedTask>
    const auto MakeOneShotOperation(TimedTask&& task) noexcept;

    template <typename TimedTask>
    const auto MakePeriodicOperation(TimedTask&& task) noexcept;

    auto RemoveTimedTaskAsync(PeriodicTaskId task) noexcept -> Result<void>;

    bool IsPeriodicJobValid(PeriodicTaskId jobid) const noexcept
    {
        const auto it{std::find(
            this->periodic_job_ids_.begin(), 
            this->periodic_job_ids_.end(), jobid)};
        return it != this->periodic_job_ids_.end();
    }

    void StartReactorTask() noexcept;

    void WakeOne() noexcept;

    void WakeAll() noexcept;

    bool IsNullTask(ExecutorOperation& op) const noexcept {return op == nullptr;} //todo what if op is not something that recognizes nullptr (std function does)
    using ReactorService = Util::Optional_NS::Optional<Reactor>;
    using TimerService = Util::Optional_NS::Optional<TimerManager>;

    ReactorService reactor_service_{}; //reactor needs to be declared before timer manager as the latter has dependencies on the former
    TimerService timer_manager_{};
    std::uint8_t concurrency_hint_{};
    MutexType mtx_; /* protects access on shared members of this class (eg: operation_queue_)*/
    std::condition_variable_any cv_;
    std::atomic_bool shutdown_requested_{false};
    bool stop_requested_{false};

    /**
     * @brief whether the executor supports io object operations (eg: sockets)
     * 
     */
    bool has_reactor_service_{false};

    /**
     * @brief whether the executor supports timed tasks (periodic/deferred tasks)
     * 
     */
    bool has_timer_service_{false};

    /**
     * @brief Storage of pending periodic job tokens (can be used to cancel job)
     * 
     */
    std::vector<PeriodicTaskId> periodic_job_ids_;

    /**
     * @brief Storage of pending scheduled job tokens (can be used to cancel job)
     * 
     */
    std::vector<ScheduledJobId> scheduled_job_ids_;

    /**
     * @brief Storage of all pending jobs
     * 
     */
    OperationQueue operation_queue_;

    /**
     * @brief The number of jobs pending to be executed
     * @note the job count does not necessarily equate with the number of operations
     * currently enqueued. It is possible for example for job to arrive, ie: job_count ++
     *  (eg: on start of async operation), before the actual operation is enqueued 
     *  (ie: after successful return from reactor run()).
     */
    std::atomic<std::uint32_t> job_count_{};

    /**
     * @brief indicates whether the reactor is blocked inside Run() and therefore in need of interrupt()
     */
    bool reactor_needs_interrupt_{false};

    /**
     * @brief flag indicating whether executor is intended to be run on strictly one thread only
     * 
     */
    const bool single_thread_{true}; /* default to single thread */
    
    std::size_t cv_wait_count_{0};
};

}

#if defined(ASRT_HEADER_ONLY)
# include "asrt/impl/io_executor.ipp"
#endif // defined(ASRT_HEADER_ONLY)


#endif /* FB4C9587_3948_4722_96D5_297415BA1CBB */
