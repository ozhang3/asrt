#ifndef F62119B4_4F29_411E_A82F_2C7C703AE0AE
#define F62119B4_4F29_411E_A82F_2C7C703AE0AE

#include <cstdint>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <mutex>
//#include <shared_mutex>
#include <vector>
#include <limits>
#include <queue>
//#include <bitset>

#include "asrt/util.hpp"
#include "asrt/error_code.hpp"
#include "asrt/reactor/reactor_interface.hpp"
#include "asrt/executor/io_executor.hpp"
//#include "asrt/config.hpp"
#include "asrt/reactor/types.hpp"

#ifdef DSISABLE_LOCKING_EXECUTOR_REACTOR_UNSAFE
    // using ExecutorMutexType = Util::NullMutex;
    using ReactorMutexType = Util::NullMutex;
#else
    // using ExecutorMutexType = std::mutex;
    using ReactorMutexType = std::mutex;
#endif

namespace ReactorNS{

    namespace internal{
        static constexpr HandlerTag kReactorUnblockTag{std::numeric_limits<HandlerTag>::max()};
        static constexpr HandlerTag kTimerTag{std::numeric_limits<HandlerTag>::max() - 1};
        static constexpr HandlerTag kMaxHandlerCount{std::numeric_limits<HandlerTag>::max() - 2};

        struct HandlerTagInfo {
            using size_type = HandlerTag;
            explicit constexpr HandlerTagInfo(HandlerTag tag) noexcept
            {

            }

            constexpr bool IsValid() const noexcept
            {
                return true;
            }
        private:
            size_type seq_num_;
            size_type index_;
        };


    }

    /**
     * @brief  thread-safe reactor based on edge-triggered linux epoll
     * 
     */
    class EpollReactor final : public ReactorInterface<EpollReactor>
    {
        static_assert(ASRT_HAS_EPOLL, "EPOLL not available with this linux kernel");
        static_assert(ASRT_HAS_EVENTFD, "Eventfd not available with this linux kernel");
        
    public:
        using ReactorBase = ReactorInterface<EpollReactor>;
        using Executor = ExecutorNS::IO_Executor<EpollReactor>;
        using MutexType = asrt::config::ReactorMutexType;
        //using TimerQueue = asrt::config::DefaultTimerService;
        //using Executor = asrt::config::DefaultExecutor;
        using typename ReactorBase::ErrorCode;
        using typename ReactorBase::OperationQueue;
        using EventHandler = ReactorNS::Types::EventHandler;
        using OperationType = ReactorNS::Types::OperationType;
        using ReactorRegistry = ReactorNS::Types::ReactorRegistry;
        using UnblockReason = ReactorNS::UnblockReason;
        using Events = ReactorNS::Types::Events;
        using EventType = Events::EventType;
        using HandlerTag = ReactorNS::Types::HandlerTag;
        template <typename T> using Result = Util::Expected_NS::Expected<T, ErrorCode>;
        using TimerHandler = asrt::UniqueFunction<void(HandlerTag, std::unique_lock<MutexType>&)>;
        
        static constexpr HandlerTag kInvalidReactorHandle{ReactorNS::Types::kInvalidHandlerTag};

        enum class EventRegistrationType {
            kIoEvent,
            kOneShotSoftwareEvent,
            kPersistentSoftwareEvent
        };
        struct OperationEntry
        {   

            enum OperationEntryFlagPosition{
                flag_valid = 0,
                flag_async_operation_ongoing = 1,
                flag_handler_posted 
            };

            enum class OperationState : std::uint8_t{
                kHandlerRegistered,
                kOperationInitiated,
                kHandlerPosted,
                kExecutionInProgress,
                kHandlerDeregistered
            };

            void SetState(OperationState new_state)
            {

            }
            
            /**
             * @brief mutex to protect multitheaded access to data of this structure
             * 
             */
            MutexType mtx_;

            /**
             * @brief the native file descriptor of the underlying i/o object, ie: socket. timerfd, file fd etc.
             * 
             */
            asrt::NativeHandle io_source_{asrt::kInvalidNativeHandle}; //initialize to -1

            /**
             * @brief unique number for current handler at given position
             * @details increments for each new handler at given position 
             * 
             */
            std::uint32_t sequence_number_;

            /**
             * @brief i/o events that the object is interesteed in
             * 
             */
            Events monitored_events_{};

            /**
             * @brief io availability status as reported by epoll in the form of events 
             * @note  unused by software events
             */
            Events captured_events_{};

            /**
             * @brief handler to be called when i/o is ready 
             * 
             */
            EventHandler handler_{nullptr}; // Socket.OnReactorEvent(Events, tag)

            /**
             * @brief indicates whether the handler is pending execution; flag is reset before handler invocation
             * @note  unused by software events
             */
            bool async_operation_ongoing_;

            /**
             * @brief whether the handler is still registered
             * 
             */
            bool valid_{false};

            /**
             * @brief Flag indicating whether the handler is in the middle of execution
             * 
             */
            bool execution_in_progress_{false};

            /**
             * @brief whether the operation is enqueued and pending execution 
             * @note  flag is reset on handler completion
             * @note  unused by software events
             */
            bool handler_posted_{false};

            /**
             * @brief whether TiggerSoftwareEvents() has already been called for this event 
             * @note  relevant only for software events
             * 
             */
            bool is_software_event_oneshot_{false};

            /**
             * @brief flag to indicate whethe handler memory needs to be released asynchronously
             * 
             */
            bool release_handler_memory_{false};

            /**
             * @brief mark this entry for asynchronous closing of its io_source
             * 
             */
            bool close_io_source_{false};
        };
#if 0        
        constexpr std::size_t a = sizeof(OperationEntry);
        std::size_t b = sizeof(std::mutex);
        std::size_t c = sizeof(EventHandler);
#endif

        struct TimerOperation{
            asrt::NativeHandle timer_fd_{asrt::kInvalidNativeHandle};
            TimerHandler handler_;
            bool in_progress_{false};
            bool release_handler_memory_{false};
        };

        explicit EpollReactor(Executor& executor, std::uint16_t handler_count) noexcept
            : executor_{executor}, epoll_events_{handler_count}, operations_{handler_count}
        {
            ASRT_LOG_TRACE("Reactor construction with {} handlers", handler_count);
            
            assert(handler_count > 0 || handler_count < internal::kMaxHandlerCount);

            this->triggered_software_events_.reserve(handler_count);

            this->used_operations_end_ = this->operations_.begin();

            this->epoll_.Open()
            .and_then([this]() -> Result<void> {
                return OsAbstraction::Eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK) /* to interrupt blocking epoll_wait() */
                .and_then([this](asrt::NativeHandle eventfd){
                    this->unblock_fd_ = eventfd;
                    const auto epoll_event{this->MakeEpollStruct(
                        Events::EventType::kRead, internal::kReactorUnblockTag)}; /* level-triggered */
                    return this->epoll_.Add(this->unblock_fd_, epoll_event);
                })
                .map_error([this](ErrorCode ec){
                    ASRT_LOG_ERROR("[EpollReactor]: Failed to setup eventfd: {}", ec);
                    return ec;
                });
            })
            .map_error([this](ErrorCode ec){
                LogFatalAndAbort("[EpollReactor]: Failed to construct epoll reactor, {}", ec);
            });
        }

        EpollReactor() = delete;

        ~EpollReactor() noexcept
        { 
            /* remove all registered fd from epoll interest list */
            std::for_each(this->operations_.begin(), this->used_operations_end_,
                [this](auto& entry){
                    if(entry.valid_){
                        this->epoll_.Remove(entry.io_source_)
                        .map_error([fd = entry.io_source_](ErrorCode ec){
                            ASRT_LOG_ERROR(
                                "[EpollReactor]: Failed to un-register fd {} during reactor deconstruction: {}",
                                fd, ec);
                        });
                    }
                });

            /* remove event fd from epoll interest list and close it */
            if(asrt::IsFdValid(this->unblock_fd_)){
                this->epoll_.Remove(this->unblock_fd_)
                .map_error([](ErrorCode ec){
                    ASRT_LOG_ERROR(
                        "[EpollReactor]: Failed to un-register event fd during reactor deconstruction: {}", ec);
                });

                OsAbstraction::Close(this->unblock_fd_)
                .map([this](){
                    ASRT_LOG_TRACE("Closed event fd {}", this->unblock_fd_);
                })
                .map_error([](ErrorCode ec){
                    ASRT_LOG_WARN("[EpollReactor]: Failed to close event fd, {}!", ec);
                });
            }

            /* remove timer fd from epoll interest list */
            if(asrt::IsFdValid(this->timer_op_.timer_fd_)){
                this->epoll_.Remove(this->timer_op_.timer_fd_)
                .map_error([](ErrorCode ec){
                    ASRT_LOG_ERROR(
                        "[EpollReactor]: Failed to un-register timer fd during reactor deconstruction: {}", ec);
                });
            }

            /* close epoll fd */
            this->epoll_.Close();
        }

        constexpr Executor& GetAssociatedExecutor() noexcept 
        {
            return this->executor_;
        }

        /**
         * @brief Unblocks the reactor
         */
        void UnblockImpl() noexcept
        {
            /* unblock when the reactor is in fact blocked */
            static_cast<void>(
                OsAbstraction::WriteEventfd(this->unblock_fd_, static_cast<eventfd_t>(1)) /* poke the eventfd */
                .map_error([](ErrorCode error){
                    if(error != ErrorCode::try_again){
                        LogFatalAndAbort("[EpollReactor]: Cannot unblock reactor, {}", error);
                    }
                })
            );
        }

        /**
         * @brief Get the current readability/writability status for the monitored fd
        */
        Events GetObservationStatusUnsafe(HandlerTag tag) const noexcept 
        {
            return this->operations_[tag].captured_events_;
        }

        /**
         * @brief Get the current readability/writability status for the monitored fd
        */
        void ConsumeObservationStatusUnsafe(HandlerTag tag, Events event_to_consume) noexcept
        {
            this->operations_[tag].captured_events_ -= event_to_consume;
        }

        template<typename Handler>
            requires asrt::MatchesSignature<Handler, void()>
        auto RegisterOneShotSoftwareEvent(Handler &&handler) noexcept -> Result<ReactorRegistry>
        {
            ASRT_LOG_DEBUG("[EpollReactor]: Registering software event handler");
            using enum EventRegistrationType;
            return RegisterImpl<Handler, kOneShotSoftwareEvent>(
                asrt::kInvalidNativeHandle, 
                ReactorNS::Types::OneShotSoftwareEvent, 
                std::forward<Handler>(handler));
        }
        
        template<typename Handler>
            requires asrt::MatchesSignature<Handler, void()>
        auto RegisterPersistentSoftwareEvent(Handler &&handler) noexcept -> Result<ReactorRegistry>
        {
            ASRT_LOG_DEBUG("[EpollReactor]: Registering software event handler");
            using enum EventRegistrationType;
            return RegisterImpl<Handler, kPersistentSoftwareEvent>(
                asrt::kInvalidNativeHandle, 
                ReactorNS::Types::PersistentSoftwareEvent,
                std::forward<Handler>(handler));
        }

        auto DeregisterSoftwareEvent(HandlerTag tag) noexcept -> Result<void>
        {
            auto& entry_to_deregister{this->operations_[tag]};
            EventHandler temp_handler;

            std::scoped_lock const lock{entry_to_deregister.mtx_};
            ASRT_LOG_DEBUG("[EpollReactor]: Deregistering software event {}", tag);

            if(!entry_to_deregister.valid_) [[unlikely]] { /* entry already deregistered */
                ASRT_LOG_ERROR("[EpollReactor]: Software event {} already deregistered.", tag);
                return MakeUnexpected(ErrorCode::reactor_entry_invalid);
            }

            if(not entry_to_deregister.execution_in_progress_){
                temp_handler = std::move(entry_to_deregister.handler_); /* release handler memory here */
                ASRT_LOG_TRACE("Software event {} handler memory released on deregistration", tag);
            }else{ /* operation in progress */
                ASRT_LOG_TRACE("Software event {} handler in progress. Cleaning up asynchronously", tag);
                /* if we arrive here, it means that the operation to be de-registered 
                    is already queued for execution (in executor thread),
                    therefore we set flag and notify executor to finish cleanup 
                    after the operation is executed  */
                entry_to_deregister.release_handler_memory_ = true;
            }
            entry_to_deregister.valid_ = false;
            return Result<void>{};
        }

        template<typename Handler, EventRegistrationType RegType = EventRegistrationType::kIoEvent>
        auto RegisterImpl(asrt::NativeHandle io_source, Events events, Handler &&handler) noexcept -> Result<ReactorRegistry>
        {
            ASRT_LOG_DEBUG("[EpollReactor]: Registering fd {}", io_source);
            (void)static_cast<EventHandler>(handler); //todo check type with template constraints
            
            OperationStorage::iterator it;
            std::scoped_lock const lock{this->registration_mtx_};

            { /* find position to insert the new entry */
                Result<OperationStorage::iterator> const find_entry_result{
                    this->FindFreeOperationSlot(io_source)};

                if(not find_entry_result.has_value()) [[unlikely]] {
                    ASRT_LOG_ERROR("[EpollReactor]: Add entry fail");
                    return MakeUnexpected(find_entry_result.error());
                }

                it = find_entry_result.value();
            }

            HandlerTag const tag{static_cast<HandlerTag>(
                std::distance(operations_.begin(), it))};

            Events monitored_events{events};

            using enum EventRegistrationType;
            if constexpr (RegType == kIoEvent) {
                if(events.HasIoEvent()) {
                    /* eager registration for read events */
                    monitored_events.Add(EPOLLIN | EPOLLPRI); //todo check epollpri meaning 

                    /* register i/o event with epoll */
                    Result<void> const add_event_result{
                        this->epoll_.Add(io_source, this->MakeEpollStruct(monitored_events, tag))};

                    if(not add_event_result.has_value()) [[unlikely]] {
                        ASRT_LOG_ERROR("[EpollReactor]: Add entry fail");
                        return MakeUnexpected(add_event_result.error());
                    }
                }
            } else if constexpr (RegType == kOneShotSoftwareEvent) {
                it->is_software_event_oneshot_ = true;
            } 

            it->io_source_ = io_source;
            it->handler_ = std::move(handler);
            it->valid_ = true;
            it->monitored_events_ = monitored_events;
            it->captured_events_ = {};
            it->handler_posted_ = false;

            ASRT_LOG_TRACE("[EpollReactor]: Registered event {:#x} for fd {} at index {}",
                monitored_events.ExtractEpollEvent(), io_source, tag);

            return ReactorRegistry{tag, it->mtx_};    
        }

        auto DerigsterImpl(HandlerTag tag, bool close_on_deregister) noexcept -> Result<void>
        {
            auto& entry_to_deregister{this->operations_[tag]};
            EventHandler temp_handler;

            std::scoped_lock const lock{entry_to_deregister.mtx_};
            const auto io_source{entry_to_deregister.io_source_};
            ASRT_LOG_DEBUG("[EpollReactor]: Deregistering io source {}", io_source);

            if(!entry_to_deregister.valid_) [[unlikely]] { /* entry already deregistered */
                ASRT_LOG_ERROR("[EpollReactor]: IO source already deregistered.");
                return MakeUnexpected(ErrorCode::reactor_entry_invalid);
            }

            return this->epoll_.Remove(io_source)
                .map([this, io_source, close_on_deregister, &temp_handler, &entry_to_deregister](){
                    if(not entry_to_deregister.execution_in_progress_){
                        temp_handler = std::move(entry_to_deregister.handler_); /* release handler memory here */
                        if(close_on_deregister) {
                            static_cast<void>(OsAbstraction::Close(io_source)
                                .map([io_source](){ ASRT_LOG_TRACE("Io_source {} handler memory released on deregistration", io_source); })
                                .map_error([io_source](ErrorCode ec){
                                    ASRT_LOG_ERROR("[EpollReactor]: Failed to close io source {}, {}", io_source, ec);
                                    return ec;
                                }));
                        }
                        ASRT_LOG_TRACE("Io_source {} handler memory released on deregistration", io_source);
                    }else{ /* operation in progress */
                        ASRT_LOG_TRACE("Io_source {} handler in progress. Cleaning up asynchronously", io_source);
                        /* if we arrive here, it means that the operation to be de-registered 
                            is already queued for execution (in executor thread),
                            therefore we set flag and notify executor to finish cleanup 
                            after the operation is executed  */
                        entry_to_deregister.release_handler_memory_ = true;
                        if(close_on_deregister){
                            entry_to_deregister.close_io_source_ = true;
                        }
                    }

                    /* mark this entry as no longer valid so that its handler will not be called */
                    entry_to_deregister.valid_ = false;
                })
                .map_error([](ErrorCode ec){
                    ASRT_LOG_ERROR(
                        "[EpollReactor]: Unable to deregister descriptor from epoll, {}", ec);
                    return ec;
                });
        }

        Result<void> TriggerSoftwareEvent(HandlerTag tag) noexcept
        {
            //todo check tag is software tag
            auto& entry_to_trigger{this->operations_[tag]};
            
            {
                std::scoped_lock const lock{entry_to_trigger.mtx_};
                if(not entry_to_trigger.valid_) [[unlikely]] {
                    ASRT_LOG_TRACE("Trying to trigger deregistered software event {:#x}", tag);
                    return MakeUnexpected(ErrorCode::api_error);
                }
                if(not entry_to_trigger.monitored_events_.HasSoftwareEvent()) [[unlikely]] {
                    ASRT_LOG_TRACE("Event tag {:#x} does not belong to a software event", tag);
                    return MakeUnexpected(ErrorCode::api_error);
                }
            }

            //! for now we do not check whether event has already been triggered
            //! or if handler is already posted for execution, because we want each 
            //! separate call of this function to trigger a separate handler invocation
            //! as opposed to merging them into one invocation

            {
                std::scoped_lock const lock{this->software_events_mtx_};      
                this->triggered_software_events_.push_back(tag);
                ASRT_LOG_TRACE("[EpollReactor]: Pushed software event {:#x} to triggered events queue", tag);
            }

            return Result<void>{};
        }  

        Result<UnblockReason> HandleEventsImpl(int timeout_ms, OperationQueue& op_queue) noexcept
        {
            //todo possible optimization opportunity:
            //todo currentyly software event handlers have to wait till io events are retrieved from epoll
            //todo before being transferred over to executor. We could avoid this by reworking the API
            //todo to take not op_queue param but a generic function that does the transfer internally. 
            //todo It is the executor's responsibility to provide this function. We would want to invoke this funciton multiple times inside HandleEvents(0)
            //todo so that each invocation of this function queues the transferred op(s) for execution 
            //todo (possibly waking up a thread if there are more than 1 worker thread). 

            const std::size_t num_software_events{this->HandleSoftwareEvents(op_queue)};

            if(num_software_events > 0) {
                ASRT_LOG_TRACE("UnblockReason::kSoftwareEvent!");
                return UnblockReason::kSoftwareEvent;
            }

            /* do not block if there are pending software event handlers to execute 
                as we want to shift control back to the executor asap */
            //const int epoll_timeout{num_software_events > 0 ? 0 : timeout_ms};

            Result<unsigned int> const epoll_wait_result{
                this->epoll_.WaitForEvents(this->epoll_events_, timeout_ms)};

            if(not epoll_wait_result.has_value()) [[unlikely]] {
                ASRT_LOG_ERROR("[EpollReactor]: Got epoll error: {}", 
                    epoll_wait_result.error());
                return MakeUnexpected(epoll_wait_result.error());
            }

            unsigned int const num_io_events{epoll_wait_result.value()}; 

            ASRT_LOG_TRACE("Returned from epoll_wait(), got {} event(s)", num_io_events);

            if(num_io_events == 0){
                ASRT_LOG_TRACE("UnblockReason::Timeout!");
                return UnblockReason::kTimeout;
            }

            for(unsigned int index = 0; index < num_io_events; index++){
                ::epoll_event const event{this->epoll_events_[index]}; /* get received events */
                //todo: add tag validation
                HandlerTag const tag{static_cast<HandlerTag>(event.data.u32)};
                if(tag == internal::kReactorUnblockTag){
                    this->HandleUnblock();
                    return UnblockReason::kUnblocked;
                }else if(tag == internal::kTimerTag) {
                    this->HandleTimerEvent(op_queue);
                }else{
                    this->HandleSingleEvent(event, tag, op_queue);
                }
            }

            ASRT_LOG_TRACE("UnblockReason::kEventsHandled!");
            return UnblockReason::kEventsHandled;
        }

        enum EventChangeType {kAddEvent, kRemoveEvent, kSetEvent};

        Result<void> AddEventImpl(HandlerTag tag, Events ev) noexcept
        {
            ASRT_LOG_TRACE("Adding event {} for io source {}", 
                ev.ExtractEpollEvent(), this->operations_[tag].io_source_);
            return this->DoModifyEvent<kAddEvent>(tag, ev);
        }

        Result<void> RemoveEventImpl(HandlerTag tag, Events ev) noexcept
        {
            ASRT_LOG_TRACE("Removing event {} for io source {}", 
                ev.ExtractEpollEvent(), this->operations_[tag].io_source_);
            return this->DoModifyEvent<kRemoveEvent>(tag, ev);
        }

        Result<void> SetEventImpl(HandlerTag tag, Events ev) noexcept
        {
            ASRT_LOG_TRACE("Setting event {} io source {}", 
                ev.ExtractEpollEvent(), this->operations_[tag].io_source_);
            return this->DoModifyEvent<kSetEvent>(tag, ev);        
        }

        bool IsValidImpl() const noexcept {return this->epoll_.IsValid();}

        bool IsInUseImpl(HandlerTag tag) noexcept
        {
            assert(tag <= this->operations_.size()); //todo move tag validation to if else?
            auto& entry{this->operations_[tag]};
            {/* enter critical section */
                std::scoped_lock operation_lock{entry.mtx_};

                return entry.valid_ || entry.execution_in_progress_;
            }
        }
        
        template<typename Handler>
        Result<void> RegisterHandler(HandlerTag tag, Handler&& handler) noexcept
        {
            assert(tag >=0 && tag <= this->operations_.size());

            auto& op{this->operations_[tag]}; /* no mutex required */

            {/* enter critical section */
                std::scoped_lock operation_lock{op.mtx_};
                op.handler_ = std::move(handler);
            } /* leave critical section */

            return Result<void>{};
        }

        /**
         * @brief update monitiored i/o event; notify executor of incoming job
         * 
         * @param tag 
         * @param op_type read and/or write
         */
        void OnStartOfOperation(HandlerTag tag, OperationType op_type) noexcept
        {
            auto& operation{this->operations_[tag]};

            ASRT_LOG_TRACE("Reactor OnStartOfOperation(), op type {}, monitored events {}({:#x})",
                ToString(op_type),
                operation.monitored_events_,
                operation.monitored_events_.ExtractEpollEvent());

            const bool update_epoll{
                op_type == OperationType::kWrite && 
                !operation.monitored_events_.HasWriteEvent()};

            operation.monitored_events_ += op_type; //todo this is confusing why are we adding event type to op type?

            ASRT_LOG_TRACE("Reactor OnStartOfOperation() updated monitored events {}({:#x})",
                operation.monitored_events_,
                operation.monitored_events_.ExtractEpollEvent());

            if(!operation.async_operation_ongoing_) [[likely]] { /* make sure to check if no async operation already pending */
                /* this is a new request */
                this->executor_.OnJobArrival(); //pending executor work + 1
                operation.async_operation_ongoing_ = true;
            }else [[unlikely]] { /* there's already pending events requested for this tag */
                /* the requested event will be notified in next handler invocation 
                    along with previously requested event so we don't call OnJobArrival() here */
                ASRT_LOG_TRACE("Io source {}", operation.io_source_, " async in progress");
            }
            if(update_epoll) [[unlikely]] {
                ASRT_LOG_TRACE("Updating epoll to monitor write event for io source {}, monitored events {:#x}",
                    operation.io_source_, operation.monitored_events_.ExtractEpollEvent());
                this->epoll_.Modify(operation.io_source_, 
                    MakeEpollStruct(operation.monitored_events_.ExtractEpollEvent() | EPOLLIN | EPOLLPRI, tag))
                .map_error([](ErrorCode ec){
                    ASRT_LOG_ERROR(
                        "Failed to register for write event on start of async operation, {}", ec);
                });
            }
        }

        // void OperationComplete(HandlerTag tag, OperationType op_type) noexcept
        // {
           
        // }

        void OnEventIgnored(HandlerTag tag, Events ev) noexcept
        {
            ASRT_LOG_TRACE("[{}] event ignored by io source {}, resubscribing.",
                Events{ev}.ToString(), this->operations_[tag].io_source_);

            /* ignored events are not considered consumed 
                therefore we resubscribe them */
            this->operations_[tag].monitored_events_ += ev;

            /* the event that was dispatched to io objects was unhandled
                therefore we initiate a complementary OnJobArrival() call 
                to indicate to the executor that the job remains outstanding */
            this->executor_.OnJobArrival(); 
        }

        /**
         * @brief A private timer handler registration method used by friend class TimerQueue
         *          to bypass certain precondition checks
         * @return Returns API_Error if user tries to re-register timer handler through this API
         */
        template <typename TimerHandler>
        Result<ReactorRegistry> RegisterTimerHandlerImpl(asrt::NativeHandle timerfd, TimerHandler&& handler) noexcept
        {
            std::scoped_lock const lock{this->timer_mtx_};
            return this->CheckTimerHandlerNotExist()
                .and_then([this, timerfd](){
                    auto epoll_event{
                        this->MakeEpollStruct(Events::EventType::kRead, internal::kTimerTag)}; /* level-triggered */
                    return this->epoll_.Add(timerfd, epoll_event);
                }) 
                .map([this, timerfd, &handler](){
                    this->timer_op_.timer_fd_ = timerfd;
                    this->timer_op_.handler_ = std::move(handler);
                    this->has_timer_handler_ = true;
                    return ReactorRegistry{internal::kTimerTag, this->timer_mtx_};
                })
                .map_error([this](ErrorCode ec){
                    ASRT_LOG_ERROR(
                        "[EpollReactor]: Failed to register epoll event for timerfd, error: {}", ec);
                    return ec;
                });
        }

        void DeregisterTimerHandlerImpl(HandlerTag tag) noexcept
        {
            assert(tag == internal::kTimerTag); //todo we might need multiple timer tags in the future

            TimerHandler placeholder{};
            std::scoped_lock const lock{this->timer_mtx_};
            
            if(!this->timer_op_.in_progress_){

                placeholder = std::move(this->timer_op_.handler_);

                OsAbstraction::Close(this->timer_op_.timer_fd_)
                .map_error([](ErrorCode ec){
                    ASRT_LOG_ERROR("[EpollReactor]: Failed to close timer fd, {}", ec);
                });

                ASRT_LOG_TRACE("Closed timer fd and released handler");

            }else{ /* handler is mid execution */
                this->timer_op_.release_handler_memory_ = true;
                ASRT_LOG_TRACE("Closing timer fd and releasing handler asynchronously");
            }
        }

    private:
        using OperationStorage = std::vector<OperationEntry>; //todo optimize for better data locality? consider using static storage
        using OperationIterator = typename OperationStorage::iterator;
        using EventStorage = std::vector<::epoll_event>;

        constexpr Result<void> CheckTimerHandlerNotExist() noexcept
        {
            return this->has_timer_handler_ ? 
                MakeUnexpected(ErrorCode::api_error) :
                Result<void>{};
        }

        auto MakeIoEventOpertaionHandler(HandlerTag handler_tag) noexcept {
            return [this, handler_tag]() -> void {
                EventHandler temp_handler{}; //nullptr
                auto& op{this->operations_[handler_tag]}; /* no mutex required */
                Events events_to_report;
                ASRT_LOG_TRACE("Io source {} operation entry", op.io_source_);

                {/* enter critical section */
                    std::unique_lock<MutexType> operation_lock{op.mtx_}; //todo

                    /* check handler is not already deregistered */
                    if(not op.valid_) [[unlikely]] {
                        ASRT_LOG_TRACE("Io source {} already deregistered, not calling handler", op.io_source_);
                        return;
                    }

                    /* we need to recalculate events to reports since 
                        captured and/or monitored events may have changed 
                        between operation enqueue and invocation */
                    events_to_report = op.captured_events_.Intersection(op.monitored_events_); 

                    if(!events_to_report.Empty()) [[likely]] { /* check we are not reporting empty events */

                        /* reported events are considered consumed and no longer monitored; 
                            user needs to re-register for i/o events prior to next operation */
                        op.monitored_events_.Consume(events_to_report); 

                        /* mark async phase finished so that next incoming operation 
                            will get a separate executor job */
                        op.async_operation_ongoing_ = false;
                        
                        op.execution_in_progress_ = true;

                        ASRT_LOG_TRACE("Calling io source {} operation handler", op.io_source_);
                        /* perform i/o & notify completion in registered handler */ /* unlocks mutex inside handler */
                        op.handler_(operation_lock, events_to_report, handler_tag); /* call io object OnReactorEvent() */
                        assert(operation_lock.owns_lock());

                        op.execution_in_progress_ = false;
                    }else [[unlikely]] {
                        /* events monitored removed by io object */
                        ASRT_LOG_DEBUG("User removed registered event for io source {}. Skipping handler.", 
                            op.io_source_);
                    }
                    
                    if(op.release_handler_memory_) [[unlikely]] { /* deregistration requested */
                        temp_handler = std::move(op.handler_);
                        op.release_handler_memory_ = false;
                        ASRT_LOG_TRACE("Released io source {} handler memory", op.io_source_);
                    }

                    if(op.close_io_source_) [[unlikely]] {
                        asrt::NativeHandle const io_source{op.io_source_};
                        static_cast<void>(OsAbstraction::Close(io_source)
                            .map([io_source](){ ASRT_LOG_TRACE("Io_source {} handler memory released asynchronously", io_source); })
                            .map_error([io_source](ErrorCode ec){
                                ASRT_LOG_ERROR("[EpollReactor]: Failed to asynchronously close io source {}, {}", io_source, ec);
                                return ec;
                            }));
                        op.close_io_source_ = false;
                    }

                    op.handler_posted_ = false;
                } /* leave critical section */

                /* Do not call OnJobCompletion() here
                    it's the executor's responsibility to call 
                    OnJobCompletion() after operation execution */
            };
        }

        auto MakeSoftwareEventOpertaionHandler(HandlerTag handler_tag) noexcept {
            return [this, handler_tag]() -> void {
                EventHandler temp_handler{}; //nullptr
                auto& op{this->operations_[handler_tag]}; /* no mutex required */
                Events events_to_report;
                ASRT_LOG_TRACE("Software event {:#x} operation entry", handler_tag);

                {/* enter critical section */
                    std::unique_lock<MutexType> operation_lock{op.mtx_}; //todo
                    op.execution_in_progress_ = true;

                    ASRT_LOG_TRACE("Calling software event {:#x} operation handler", handler_tag);
                    op.handler_(operation_lock, events_to_report, handler_tag);  /* unlocks mutex inside handler */
                   
                    assert(operation_lock.owns_lock());
                    op.execution_in_progress_ = false;

                    if(op.is_software_event_oneshot_) {
                        temp_handler = std::move(op.handler_);
                        op.release_handler_memory_ = false;
                        op.valid_ = false;
                        ASRT_LOG_TRACE("Released software event {:#x} handler memory", handler_tag);
                    } else if(op.release_handler_memory_) [[unlikely]] {
                        /* deregistration requested for persistent event */
                        temp_handler = std::move(op.handler_);
                        op.release_handler_memory_ = false;
                        ASRT_LOG_TRACE("Released software event {:#x} handler memory", handler_tag);
                    }

                    op.handler_posted_ = false;
                } /* leave critical section */

                /* Do not call OnJobCompletion() here
                    it's the executor's responsibility to call 
                    OnJobCompletion() after operation execution */
            };
        }

        void HandleTimerEvent(OperationQueue& op_queue) noexcept 
        {   
            ASRT_LOG_TRACE("Handling timer event");
            op_queue.push_back([this](){               
                std::unique_lock<MutexType> lock{this->timer_mtx_};
                this->timer_op_.in_progress_ = true;

                ASRT_LOG_TRACE("Calling timer operation handler");

                this->timer_op_.handler_(internal::kTimerTag, lock);

                TimerHandler placeholder{};
                if(this->timer_op_.release_handler_memory_){
                    placeholder = std::move(this->timer_op_.handler_);
                }
                this->timer_op_.in_progress_ = false;
            });

            ASRT_LOG_TRACE("Enqueued timer event handler");
        }

        void HandleSingleEvent(const ::epoll_event event, HandlerTag handler_tag, OperationQueue& op_queue) noexcept
        {   
            auto& current_operation{operations_[handler_tag]};
            Events const captured_events{event.events};

            ASRT_LOG_TRACE("Handling io event {}({:#x}) for io source {}", 
                captured_events, event.events, current_operation.io_source_);

            { /* enter critical section */
                std::scoped_lock operation_lock{current_operation.mtx_};

                /* always update io_source readability/writeablity status regardless of whether we're reporting them */
                current_operation.captured_events_ = captured_events; /* overwrite instead of add to reflect the latest status */

                /* report events that are both monitoried and captured */
                const Events events_to_report{captured_events
                    .Intersection(current_operation.monitored_events_)};

                /* Validate handler since it could have already been invalidated by Deregister() prior to HandleEvents() */
                if(not current_operation.valid_) [[unlikely]] { 
                    /* io-source already de-registered handler 
                        before it had a chance to process the incoming event */
                    /* handler memory already released in Deregister() */
                    ASRT_LOG_DEBUG("[EpollReactor]: Handler already de-registered. Abort event handling.");
                    return;
                }
                
                /* check no handler for this event is outstanding */
                if(current_operation.handler_posted_) [[unlikely]] { 
                    /* handler already queued for execution we just update events and return */
                    ASRT_LOG_TRACE("Updated io source {} captured events for queued handler", current_operation.io_source_);
                    return;
                }

                /* notify io object of incoming event only if it registered interst for the event */   
                if(events_to_report.Empty()) [[unlikely]] { 
                    /* just drop the uninteresting event and bail */
                    ASRT_LOG_DEBUG("[EpollReactor]: No events to report, monitored {}, captured {}",
                        current_operation.monitored_events_, current_operation.captured_events_);
                    ASRT_LOG_TRACE("monitored events: {:#x}, captured events: {:#x}",
                        current_operation.monitored_events_.ExtractEpollEvent(),
                        current_operation.captured_events_.ExtractEpollEvent());

                    // if(current_operation.async_operation_ongoing_) [[unlikely]] { /* async op started, but handler not yet enqueued */
                    //     /* if an io object started an async operation but later removed interest in the event
                    //         prior to the reactor posting the handler for execution, we need to compensate for 
                    //         the OnJobArrival() call by matching it with a OnJobCompletion() call 
                    //         to mark the job as finished (cancelled) */
                    //     this->executor_.OnJobCompletion();
                    //     current_operation.async_operation_ongoing_ = false;
                    //     ASRT_LOG_TRACE("[EpollReactor]: Io source {} operation cancelled", 
                    //         current_operation.io_source_);
                    // }
                    return;
                }

                /* now we know we have events to report &&
                    no existing handler is outstanding &&
                    handler for this event is still valid (registered) */
                current_operation.handler_posted_ = true;
                ASRT_LOG_TRACE("Pushed operation to executor queue");
            } /* leave critical section */

            /* queue handler for invocation by executor */
            op_queue.push_back(this->MakeIoEventOpertaionHandler(handler_tag));
        }
        
        std::size_t HandleSoftwareEvents(OperationQueue& op_queue) noexcept
        {
            std::size_t handled_events{};
            std::ranges::for_each(this->triggered_software_events_,
                [this, &handled_events, &op_queue](HandlerTag tag){
                    auto& op{operations_[tag]};
                    { /* enter critical section */
                        std::scoped_lock operation_lock{op.mtx_};
                        if(not op.valid_) [[unlikely]] {
                            ASRT_LOG_TRACE("Got deregistered software event when handling events");
                            return;
                        }
                        op.handler_posted_ = true;
                        if(not op.is_software_event_oneshot_){
                            this->executor_.OnJobArrival(); 
                        }
                    }
                    op_queue.push_back(this->MakeSoftwareEventOpertaionHandler(tag));
                    handled_events++;
                });
            return handled_events;
        } 

        void HandleUnblock() noexcept
        {
            ASRT_LOG_TRACE("Handling unblock");
            static_cast<void>(
                OsAbstraction::ReadEventfd(this->unblock_fd_)
                .map_error([](ErrorCode error){
                    if(error != ErrorCode::interrupted) [[unlikely]] { //todo
                        LogFatalAndAbort("[EpollReactor]: Handle unblock failed, {}", error);
                    }
                })
            );
        }

        Result<OperationIterator> FindFreeOperationSlot(asrt::NativeHandle io_source) noexcept
        {
            bool found_slot{false};
            OperationIterator position;
            OperationIterator last_used_entry;
            
            //todo: do while loop maybe?
            /* only need to check up to used_operations_end_ (guaranteed to not surpass operations_.end()) */
            for(auto it{this->operations_.begin()}; (it <= this->used_operations_end_) && (it != this->operations_.end()); it++)
            {
                {/* enter critical section */
                    std::scoped_lock operation_lock{it->mtx_};

                    if(!it->valid_){ /* already deregistered */
                        if(!it->handler_posted_){ /* safe to enqueue here */
                            if(!found_slot){
                                found_slot = true;
                                position = it; /* return this as new entry */
                                last_used_entry = it; /* mark it as used entry */
                            }else{ 
                                /* we already have a free entry and we are only returning the first available entry */ 
                            }
                        }else{ /* deregistered but handler still in use */
                            last_used_entry = it; /* entry still in use */
                        }
                    }else{ /* entry still registered */
                        if(it->io_source_ == io_source) [[unlikely]] {
                            ASRT_LOG_TRACE("fd {} already registered!", io_source);
                            /* we want an early exit since registration has already failed at this point */
                            return MakeUnexpected(ErrorCode::api_error); //todo: alrady registered
                        }
                        last_used_entry = it; /* mark as used */
                    }
                }/* leave critical section */
            }

            /* update last used entry if necessary */
            if(this->used_operations_end_ != this->operations_.end()){ 
                this->used_operations_end_ = std::next(last_used_entry);
            }

            if(!found_slot) [[unlikely]] {
                ASRT_LOG_ERROR("[EpollReactor]: Ran out of handler storage!");
                return MakeUnexpected(ErrorCode::capacity_exceeded);
            }

            return position;
        }

        template <EventChangeType ChangeHow>
        Result<void> DoModifyEvent(HandlerTag tag, Events ev) noexcept 
        {
            static constexpr const char* kChangeTypePrintable[3]{"add", "remove", "set"};
            assert(tag <= internal::kMaxHandlerCount);
            auto& entry{this->operations_[tag]};
            auto monitored_events{entry.monitored_events_};
            Events changed_event;
            bool update_epoll;

            if(!entry.valid_) [[unlikely]] {
                ASRT_LOG_ERROR(
                    "[EpollReactor]: Trying to {} event for unregistered io source!",
                    kChangeTypePrintable[ChangeHow]);
                return MakeUnexpected(ErrorCode::reactor_entry_invalid);
            }

            if constexpr (EventChangeType::kAddEvent == ChangeHow) {
                changed_event = monitored_events + ev;
                update_epoll = 
                    (ev.HasEvent(EventType::kEdge) && !monitored_events.HasEvent(EventType::kEdge)) ||
                    (ev.HasEvent(EventType::kWrite) && !monitored_events.HasEvent(EventType::kWrite));
            } else if constexpr (EventChangeType::kRemoveEvent == ChangeHow) {
                changed_event = monitored_events - ev;
                update_epoll = 
                    (ev.HasEvent(EventType::kEdge) && monitored_events.HasEvent(EventType::kEdge)) ||
                    (ev.HasEvent(EventType::kWrite) && monitored_events.HasEvent(EventType::kWrite));
            } else {
                changed_event = ev;
                update_epoll = (ev != monitored_events);
            }

            if(!update_epoll){
                ASRT_LOG_TRACE("Not updating epoll. Monitored events before {:#x}, after {:#x}",
                    monitored_events.ExtractEpollEvent(), changed_event.ExtractEpollEvent());
                entry.monitored_events_ = changed_event; 
                return Result<void>{};
            }

            return this->epoll_.Modify(entry.io_source_, 
                MakeEpollStruct((changed_event.ExtractEpollEvent() | EPOLLIN | EPOLLPRI), tag))
                .map([&entry, changed_event](){
                    entry.monitored_events_ = changed_event;
                    ASRT_LOG_TRACE("{} event success for io_source {}", 
                        kChangeTypePrintable[ChangeHow], entry.io_source_);
                });
        }

        
        constexpr auto MakeEpollStruct(std::uint32_t epoll_events, HandlerTag tag) noexcept -> ::epoll_event
        {
            ::epoll_event ev{};
            ev.events = epoll_events;
            ev.data.u32 = tag; //todo: u32 or u64?
            return ev;
        }

        constexpr auto MakeEpollStruct(Events events, HandlerTag tag) noexcept -> ::epoll_event
        {
            return MakeEpollStruct(events.ExtractEpollEvent(), tag);
        }


        Epoll_NS::EpollWrapper epoll_{};
        asrt::NativeHandle unblock_fd_{asrt::kInvalidNativeHandle};
        Executor& executor_; 
        OperationStorage::iterator used_operations_end_;
        EventStorage epoll_events_;
        OperationStorage operations_;
        MutexType software_events_mtx_;
        std::vector<HandlerTag> triggered_software_events_;
        MutexType timer_mtx_;
        TimerOperation timer_op_; /* proteced by timer_mtx_ */
        bool has_timer_handler_{false};

        //bool do_release_handler_memory_{false};

        //bool do_close_io_source_{false};

        //bool reactor_needs_unblock_{false};
        //std::atomic<Types::ReactorState> reactor_state_;
        MutexType registration_mtx_; /* protects access to internal data of the reactor object */
    };
}

#endif /* F62119B4_4F29_411E_A82F_2C7C703AE0AE */
