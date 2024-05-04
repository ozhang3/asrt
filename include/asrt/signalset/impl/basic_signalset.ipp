
#include "asrt/signalset/basic_signalset.hpp"


template <class Executor>
inline BasicSignalSet<Executor>::
BasicSignalSet(Executor& executor) noexcept : 
    executor_{executor}, reactor_{executor.UseReactorService()}
{
    ASRT_LOG_TRACE("[BasicSignalSet]: construction from exeuctor only");
    OsAbstraction::SigEmptySet(&this->signal_set_);
};


template <class Executor>
template <std::same_as<int> ...SigNum>
inline BasicSignalSet<Executor>::
BasicSignalSet(Executor& executor, SigNum... signals) noexcept :
    executor_{executor}, reactor_{executor.UseReactorService()}
{
    ASRT_LOG_TRACE("[BasicSignalSet]: construction from exeuctor and signals");
    
    this->DoAddSignals(signals...)
    .and_then([this](){
        return this->AcquireNativeHandle(SFD_CLOEXEC | SFD_NONBLOCK); /* default to non-blocking */
    })
    .and_then([this](){
        return this->RegisterToReactor();
    })
    .map_error([](ErrorCode ec){
        LogFatalAndAbort("Failed to construct signalset, {}", ec);
    });
};

template <class Executor>
inline BasicSignalSet<Executor>::
~BasicSignalSet() noexcept
{
    if(asrt::IsFdValid(this->native_handle_)) [[likely]] {
        if(this->reactor_.has_value()){
            /* de-register handle before closing */
            this->reactor_.value().Deregister(this->reactor_handle_, true)
            .map_error([](ErrorCode ec){
                ASRT_LOG_ERROR("Failed to deregister signalfd from reactor: {}", ec);
            });
        }else{
            OsAbstraction::Close(this->native_handle_);
        }
    }
}

template <class Executor>
template <std::same_as<int> ...SigNum>
inline auto BasicSignalSet<Executor>::
Add(SigNum... signals) -> Result<void>
{
    std::scoped_lock const lock{this->GetMutex()};
    return this->DoAddSignals(signals...)
        .and_then([this](){
            return OsAbstraction::SetSignalFd(this->native_handle_, &this->signal_set_);
        });
}

template <class Executor>
inline auto BasicSignalSet<Executor>::
Wait() -> Result<int>
{
    ::signalfd_siginfo siginfo;
    std::scoped_lock const lock{this->GetMutex()};
    return OsAbstraction::Read(this->native_handle_, &siginfo, sizeof(siginfo))
        .and_then([&siginfo](std::size_t bytes_read) -> Result<int> {
            if(bytes_read < sizeof(siginfo))
                return MakeUnexpected(ErrorCode::read_insufficient_data);
            return siginfo.ssi_signo;
        });
}

template <class Executor>
template <typename WaitCompletionCallback>
inline void BasicSignalSet<Executor>::
WaitAsync(WaitCompletionCallback&& signal_handler)
{
    std::scoped_lock const lock{this->GetMutexUnsafe()};
    assert(this->IsAsyncPreconditionsMet());
    
    const auto immediate_completion{
        [this](WaitCompletionCallback&& handler, Result<int>&& res){
            this->executor_.value().EnqueueOnJobArrival(
                [handler = std::move(handler), res = std::move(res)](){
                    handler(std::move(res));
                });
        }};
    
    if(this->is_wait_ongoing_) [[unlikely]] {  //todo what does this mean??
        immediate_completion(
            std::move(signal_handler), 
            MakeUnexpected(ErrorCode::async_operation_in_progress));
        return;
    }

    this->DoReadSignalsAsync(std::move(signal_handler), std::move(immediate_completion));
}

template <class Executor>
inline void BasicSignalSet<Executor>::
Cancel()
{
    ASRT_LOG_TRACE("Cancelling async wait operation");
    std::scoped_lock const lock{this->GetMutex()};
    if(this->is_wait_ongoing_){
        /* if async wait is ongoing we know both reactor and executor are valid */
        this->reactor_.value().RemoveMonitoredEvent(this->reactor_handle_, EventType::kRead)
        .map([this](){
            this->executor_.value().EnqueuePostJobArrival(
                [handler = std::move(this->wait_completion_handler_)](){
                    handler(MakeUnexpected(ErrorCode::operation_cancelled));
                });
        })
        .map_error([](ErrorCode ec){
            ASRT_LOG_ERROR(
                "Failed to deregister reactor event during operation cancellation, {}", ec);
        });
    }
}

template <class Executor>
inline auto BasicSignalSet<Executor>::
SetCurrentThreadMask() -> Result<void>
{
    return OsAbstraction::PthreadSigmask(SIG_SETMASK, &this->signal_set_, nullptr)
        .map_error([](ErrorCode ec){
            ASRT_LOG_ERROR("Failed to set thread mask, {}", ec);
            return ec;
        });
}

template <class Executor>
template <std::same_as<int> ...SigNum>
inline auto BasicSignalSet<Executor>::
SetCurrentThreadMask(SigNum... signals) -> Result<void>
{
    ::sigset_t mask;
    return OsAbstraction::SigEmptySet(&mask)
        .and_then([&mask, signals...](){
            return OsAbstraction::SigAddSet(&mask, signals...);
        })
        .and_then([&mask](){
            return OsAbstraction::PthreadSigmask(SIG_SETMASK, &mask, nullptr);
        })
        .map_error([](ErrorCode ec){
            ASRT_LOG_ERROR("Failed to set thread mask, {}", ec);
            return ec;
        });
}

template <class Executor>
inline void BasicSignalSet<Executor>::
OnReactorEvent(std::unique_lock<MutexType>& lock, Events events)
{
    ASRT_LOG_TRACE("Handling reactor event");
    assert(events == EventType::kRead);
    
    /* make sure we take advantage of the incoming event 
        even if we are not currently monitoring it */
    this->speculative_read_ = true; 

    if(this->is_wait_ongoing_) [[likely]] {
        this->DoReadSignalsAsync(
            std::move(this->wait_completion_handler_),
            [this, &lock](auto&& callback, Result<int>&& res){
                this->is_wait_ongoing_ = false;
                ASRT_LOG_TRACE("Notifying wait completion");
                lock.unlock();
                /* this is executor context so we directly invoke the handler */
                callback(std::move(res));
                lock.lock();
            });
    }else [[unlikely]] {
        ASRT_LOG_INFO("Got uninteresting signal event");
        this->reactor_.value().EventIgnored(
            this->reactor_handle_, events);
    }
}

template <class Executor>
template <std::same_as<int> ...SigNum>
inline auto BasicSignalSet<Executor>::
DoAddSignals(SigNum... signals) -> Result<void>
{
    return OsAbstraction::SigAddSet(&this->signal_set_, signals...);
}

template <class Executor>
inline auto BasicSignalSet<Executor>::
DoReadSignalsSync() -> Result<int>
{
    ::signalfd_siginfo siginfo;
    return OsAbstraction::Read(this->native_handle_, &siginfo, sizeof(siginfo))
        .and_then([&siginfo](std::size_t bytes_read) -> Result<int> {
            if(bytes_read < sizeof(siginfo))
                return MakeUnexpected(ErrorCode::read_insufficient_data);
            return siginfo.ssi_signo;
        });
}

template <class Executor>
template <typename WaitCompletionCallback, typename OnImmediateCompletion>
inline void BasicSignalSet<Executor>::
DoReadSignalsAsync(WaitCompletionCallback&& completion_callback, OnImmediateCompletion&& on_immediate_completion)
{
    bool async_needed{false};
    Result<int> read_result{};

    if(this->speculative_read_){ /* false on first attempt */
        this->speculative_read_ = false;
        read_result = this->DoReadSignalsSync(); /* this will not block (we asserted this with IsAsyncPreconditionsMet()) */
        if(!read_result.has_value()) [[unlikely]] {
            if(ErrorCode_Ns::IsBusy(read_result.error())) [[likely]]
                async_needed = true;
        }
    }else{
        async_needed = true;
    }

    if(async_needed){
        this->is_wait_ongoing_ = true;
        this->wait_completion_handler_ = std::move(completion_callback);
        this->RegisterReactorEvent(); /* register for read event */
    }else{
        /* report success or read error (non-busy) */
        on_immediate_completion(std::move(completion_callback), std::move(read_result));
    }
}

template <class Executor>
inline const auto BasicSignalSet<Executor>::
MakeReactorEventHandler(){
    return [this](std::unique_lock<MutexType>& lock, Events ev, ReactorHandle handle) {
                assert(lock.owns_lock());
                assert(handle == this->reactor_handle_);
                this->OnReactorEvent(lock, ev);
            };
}
template <class Executor>
inline auto BasicSignalSet<Executor>::
RegisterToReactor() -> Result<void>
{
    assert(this->reactor_.has_value());
    return this->reactor_.value().Register(
            this->native_handle_, EventType::kRead, /* level-triggered mode + eager registration for read events */
            this->MakeReactorEventHandler())
        .map([this](const auto& registry) {
            this->reactor_handle_ = registry.tag;
            this->reactive_sigset_mtx_ = &(registry.mutex);
            ASRT_LOG_TRACE("Signalset (handle: {}) registration with reactor success, reactor handle: {}", 
                this->native_handle_, this->reactor_handle_);
        });
}

template <class Executor>
inline auto BasicSignalSet<Executor>::
AcquireNativeHandle(int flags) -> Result<void>
{
    return OsAbstraction::GetSignalFd(&this->signal_set_, flags)
        .map([this](NativeHandle sigfd){
            this->native_handle_ = sigfd;
            this->is_native_nonblocking_ = true;
        });
}

template <class Executor>
inline void BasicSignalSet<Executor>::
RegisterReactorEvent()
{
    using typename ReactorNS::OperationType;
    /* we are only interested in read events */
    this->reactor_.value().OperationStarted(this->reactor_handle_, OperationType::kRead);
}
