#include "asrt/executor/strand.hpp"


namespace ExecutorNS{

template <typename Executor>
template <typename Executable>
inline void Strand<Executor>::
Post(Executable&& op)
{
    ASRT_LOG_TRACE("Posting job through strand");

    if constexpr (not STRAND_HAS_THREADS){
        this->executor_.Post(std::move(op));
        return;
    }

    bool trigger_run{true};
    {
        std::scoped_lock const lock{this->this_strand_.mtx_};
        trigger_run = !this->this_strand_.is_running_;
        this->this_strand_.jobs_.emplace_back(std::move(op));
        this->this_strand_.is_running_ = true;
    }

    if(trigger_run){
        this->executor_.Post(
            [this](){
                this->ExecutePendingJobs();
            });
    }
}

template <typename Executor>
template <typename Executable>
inline void Strand<Executor>::
Dispatch(Executable&& op)
{
    ASRT_LOG_TRACE("Dispatching job through strand");

    if constexpr (not STRAND_HAS_THREADS) {
        this->executor_.EnqueueOnJobArrival(std::move(op));
        return;
    }

    /* check if we are inside one of the worker threads 
        exeucuting Executor run() */
    if(not this->executor_.IsExecutorContext()) [[unlikely]] {
        ASRT_LOG_TRACE("Dispatching job through strand post");
        /* if not we fallback to the good ole' Post() */
        this->Post(std::move(op));
        return;
    }

    /* then check if we are being invoked from the same thread
        that's executing a job of this strand 
        (eg: calling Dispatch() inside a strand operation) */
    if(this->IsContinuation()) {
        /* it's safe to directly execute the operation */
        ASRT_LOG_TRACE("Directly invoking operation");
        op();
        return;
    }
    
    {
        /* there are two possible scenarios:
            1. no operations belonging to this strand is currently being executed 
            2. strand operations are being executed in some other thread */
        bool execute_now;
        {
            std::scoped_lock const lock{this->this_strand_.mtx_};
            execute_now = !this->this_strand_.is_running_;
            if(!execute_now){
                /* This is case 2: 
                    we're not in the same thread currently executing this strand. 
                    So we can't possibly invoke the operation here. Just enqueue and bail. */
                this->this_strand_.jobs_.emplace_back(std::move(op));
            }
            this->this_strand_.is_running_ = true;
        }

        if(execute_now) {
            /* This is case 1: no other threads are currently executing the strand */
            /* so we'll go ahead and initiate the strand execution 
                and indicate that through the context marker */
            ASRT_LOG_TRACE("Initiating strand execution");
            ExecutionContext<Strand> execution_context{*this};
            op();
            this->ExecutePendingJobs();
        }     
    }
}    

template <typename Executor>
inline void Strand<Executor>::
ExecutePendingJobs()
{
    ExecutionContext<Strand> const execution_context{*this};

    for(;;){
        StrandJob job_to_run;
        {
            std::scoped_lock const lock{this->this_strand_.mtx_};
            assert(this->this_strand_.is_running_);
            if(this->this_strand_.jobs_.size()){
                job_to_run = std::move(this->this_strand_.jobs_.front());
                this->this_strand_.jobs_.pop_front();
            }else{
                this->this_strand_.is_running_ = false;
                break;
            }
        }
        job_to_run();
        ASRT_LOG_TRACE("Executed 1 strand job");
    }
}

}//end ns