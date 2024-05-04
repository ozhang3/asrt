#ifndef E331DC3E_4884_4BE3_87B9_41B627F15C14
#define E331DC3E_4884_4BE3_87B9_41B627F15C14

#include <cstdint>
#include <functional>

#include "asrt/common_types.hpp"
#include "asrt/util.hpp"
#include "asrt/error_code.hpp"
#include "asrt/sys/syscall.hpp"
#include "asrt/sys/epoll_wrapper.hpp"
#include "asrt/executor/executor_task.hpp"
#include "asrt/reactor/types.hpp"
#include "asrt/timer/timer_types.hpp"

namespace ReactorNS
{
    using ReactorNS::Types::Events;
    using ReactorNS::Types::details::EventType;
    using ReactorNS::Types::HandlerTag;
    using ReactorNS::Types::TimerTag;
    using ReactorNS::Types::OperationType;
    using namespace Util::Expected_NS;
    using ExecutorNS::ReactorService;
    using UnblockReason = ExecutorNS::ReactorUnblockReason; //todo
    using ReactorHandleType = ReactorNS::Types::HandlerTag;

    /*!
    * \brief    A reactor interface implemented with CRTP
    */
    template <class ReactorImpl>
    class ReactorInterface : public ReactorService<ReactorImpl>
    {
    public:
        using ErrorCode = ErrorCode_Ns::ErrorCode;
        using OperationQueue = ExecutorNS::OperationQueue;
        using EventHandlerType = ReactorNS::Types::EventHandler; /* void(Events, Handle) */
        using ReactorRegistry = ReactorNS::Types::ReactorRegistry;
        using Expiry = Timer::Types::Expiry;
        using Duration = Timer::Types::Duration;
        template <typename T> using Result = Expected<T, ErrorCode>;

        static constexpr int kInfiniteTimeout{-1};

        // constexpr auto GetExecutor() -> Executor&
        // {
        //     return Implementation().GetAssociatedExecutor();
        // }

        /**
         * @brief Execute exactly one event demultiplexing operation.
         * 
         * @param timeout_ms 
         * @param operations 
         * @return Expected<UnblockReason, ErrorCode> 
         */
        auto HandleEvents(int timeout_ms, OperationQueue& op_queue) noexcept -> Expected<UnblockReason, ErrorCode>
        {
            return Implementation().HandleEventsImpl(timeout_ms, op_queue);
        }   

        /**
         * @brief Interrupts reactor blocked waiting for events
         * 
         */
        void Unblock() noexcept
        {
            Implementation().UnblockImpl();
        }

        /**
         * @brief Register event handler for an event on some file descriptor
         * 
         * @tparam EventHandler must conform to void(Events, HandlerTag)
         * @param fd file descriptor of io source
         * @param ev event(s) to register notification for
         * @param handler the callback function to register
         * @return On success a struct containing the tag and events 
         *      that are currently being monitored by the underlying reactor
         */
        template<typename EventHandler>
        auto Register(asrt::NativeHandle fd, Events ev, EventHandler &&handler) noexcept -> Expected<ReactorRegistry, ErrorCode>;

        /*!
        * \brief    Unregisters a previously registered handler 
        *
        * \details  Handlers not yet executed will not be executed after this call, currently running handlers will finish
        *           executing. Tries to release handler memory and close io_source immediately if handler is not being executed, else
        *           completes the cleanup after handler execution.    
        * \param    tag handler id returned by Register() 
        */
        auto Deregister(HandlerTag tag, bool close_on_deregister) -> Expected<void, ErrorCode>;

        template<typename EventHandler>
        auto UpdateRegisteredHandler(HandlerTag tag, EventHandler &&handler) noexcept -> Expected<void, ErrorCode>;

        auto AddMonitoredEvent(HandlerTag tag, Events ev) noexcept -> Expected<void, ErrorCode>;

        auto SetMonitoredEvent(HandlerTag tag, Events ev) noexcept -> Expected<void, ErrorCode>;

        auto RemoveMonitoredEvent(HandlerTag tag, Events ev) noexcept -> Expected<void, ErrorCode>;

        template <typename TimerHandler>
        auto RegisterTimerHandler(asrt::NativeHandle timerfd, TimerHandler&& handler) -> Expected<ReactorRegistry, ErrorCode>
        {
            return Implementation().RegisterTimerHandlerImpl(timerfd, std::move(handler));
        }

        void DeregisterTimerHandler(HandlerTag tag)
        {
            return Implementation().DeregisterTimerHandlerImpl(tag);
        }

        auto IsInUse(HandlerTag tag) -> bool;

        /**
         * @brief Used by reactor users to notify reactor of initiation of async operation
         * 
         */
        auto OperationStarted(HandlerTag tag, OperationType op_type) -> void
        {
            Implementation().OnStartOfOperation(tag, op_type);
        }

        /**
         * @brief Used by reactor users to notify reactor of ignored reactor event
         * 
         */
        auto EventIgnored(HandlerTag tag, Events ev) -> void
        {
            Implementation().OnEventIgnored(tag, ev);
        }

        //auto OperationFinished() -> void;

        constexpr auto IsValid() -> bool;

    private:
        constexpr ReactorImpl &Implementation() { return static_cast<ReactorImpl &>(*this); }
    };
    
    template <typename ReactorImpl>
    template <typename EventHandler>
    inline auto ReactorInterface<ReactorImpl>::
    Register(asrt::NativeHandle fd, Events ev, EventHandler &&handler) noexcept -> Expected<ReactorRegistry, ErrorCode>
    {
        return Implementation().RegisterImpl(fd, ev, std::forward<EventHandler>(handler));
    }

    template <typename ReactorImpl>
    inline auto ReactorInterface<ReactorImpl>::
    Deregister(HandlerTag tag, bool close_on_deregister) -> Expected<void, ErrorCode>
    {
        return Implementation().DerigsterImpl(tag, close_on_deregister);
    }


    template <typename ReactorImpl>
    template <typename EventHandler>
    inline auto ReactorInterface<ReactorImpl>::
    UpdateRegisteredHandler(HandlerTag tag, EventHandler &&handler) noexcept -> Expected<void, ErrorCode>
    {
        return Implementation().RegisterHandler(tag, std::move(handler));
    }

    template <typename ReactorImpl>
    inline auto ReactorInterface<ReactorImpl>::
    AddMonitoredEvent(HandlerTag tag, Events ev) noexcept -> Expected<void, ErrorCode>
    {
        return Implementation().AddEventImpl(tag, ev);
    }

    template <typename ReactorImpl>
    inline auto ReactorInterface<ReactorImpl>::
    RemoveMonitoredEvent(HandlerTag tag, Events ev) noexcept -> Expected<void, ErrorCode>
    {
        return Implementation().RemoveEventImpl(tag, ev);
    }

    template <typename ReactorImpl>
    inline auto ReactorInterface<ReactorImpl>::
    SetMonitoredEvent(HandlerTag tag, Events ev) noexcept -> Expected<void, ErrorCode>
    {
        return Implementation().SetEventImpl(tag, ev);
    }

    template <typename ReactorImpl>
    inline constexpr auto ReactorInterface<ReactorImpl>::
    IsValid() -> bool
    {
        return Implementation().IsValidImpl();
    }

    template <typename ReactorImpl>
    inline auto ReactorInterface<ReactorImpl>::
    IsInUse(HandlerTag tag) -> bool
    {
        return Implementation().IsInUseImpl(tag);
    }

}


#endif /* E331DC3E_4884_4BE3_87B9_41B627F15C14 */
