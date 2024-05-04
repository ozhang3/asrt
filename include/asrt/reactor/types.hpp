#ifndef C90E6770_B081_438A_A1DF_95DCC156B1DC
#define C90E6770_B081_438A_A1DF_95DCC156B1DC

#include <cstdint>
#include <sys/epoll.h>
#include <functional> //event handler
#include <limits>
#include <iostream>

#include "asrt/config.hpp"
#include "asrt/util.hpp"
#include "asrt/timer/timer_types.hpp"

namespace ReactorNS{

namespace Types{

using EpollEventType = decltype(::epoll_event::events);

    namespace details{

        enum class EventType : EpollEventType
        {
            kNone = 0,
            kEdge = ::EPOLL_EVENTS::EPOLLET,
            kRead = ::EPOLL_EVENTS::EPOLLIN,
            kWrite = ::EPOLL_EVENTS::EPOLLOUT,
            kReadPri = ::EPOLL_EVENTS::EPOLLIN | ::EPOLL_EVENTS::EPOLLPRI,
            kWritePri = ::EPOLL_EVENTS::EPOLLOUT | ::EPOLL_EVENTS::EPOLLPRI,
            kReadWrite = ::EPOLL_EVENTS::EPOLLIN | ::EPOLL_EVENTS::EPOLLOUT,
            kReadWritePri = ::EPOLL_EVENTS::EPOLLIN | ::EPOLL_EVENTS::EPOLLOUT | ::EPOLL_EVENTS::EPOLLPRI,
            kReadEdge = ::EPOLL_EVENTS::EPOLLIN | ::EPOLL_EVENTS::EPOLLET,
            kWriteEdge = ::EPOLL_EVENTS::EPOLLOUT | ::EPOLL_EVENTS::EPOLLET,
            kReadWriteEdge = ::EPOLL_EVENTS::EPOLLIN | ::EPOLL_EVENTS::EPOLLOUT | ::EPOLL_EVENTS::EPOLLET,
            kReadEdgePri = ::EPOLL_EVENTS::EPOLLIN | ::EPOLL_EVENTS::EPOLLET | ::EPOLL_EVENTS::EPOLLPRI,
            kWriteEdgePri = ::EPOLL_EVENTS::EPOLLOUT | ::EPOLL_EVENTS::EPOLLET | ::EPOLL_EVENTS::EPOLLPRI,
            kReadWriteEdgePri = ::EPOLL_EVENTS::EPOLLIN | ::EPOLL_EVENTS::EPOLLOUT | ::EPOLL_EVENTS::EPOLLET | ::EPOLL_EVENTS::EPOLLPRI,
            kReadWriteErrHup = ::EPOLL_EVENTS::EPOLLIN | ::EPOLL_EVENTS::EPOLLOUT | ::EPOLL_EVENTS::EPOLLERR | ::EPOLL_EVENTS::EPOLLHUP,
            kPriEdge = ::EPOLL_EVENTS::EPOLLPRI | ::EPOLL_EVENTS::EPOLLET,
            kReadHangup = ::EPOLL_EVENTS::EPOLLIN | ::EPOLL_EVENTS::EPOLLHUP,
            kWriteHangup = ::EPOLL_EVENTS::EPOLLOUT | ::EPOLL_EVENTS::EPOLLHUP,
            kRdHup = ::EPOLL_EVENTS::EPOLLRDHUP,
            kReadHupPri = ::EPOLL_EVENTS::EPOLLIN | ::EPOLL_EVENTS::EPOLLHUP | ::EPOLL_EVENTS::EPOLLPRI,
            kWriteHupPri = ::EPOLL_EVENTS::EPOLLOUT | ::EPOLL_EVENTS::EPOLLHUP | ::EPOLL_EVENTS::EPOLLPRI,
            kReadEdgeHupPri = ::EPOLL_EVENTS::EPOLLIN | ::EPOLL_EVENTS::EPOLLET | ::EPOLL_EVENTS::EPOLLHUP | ::EPOLL_EVENTS::EPOLLPRI,
            kWriteEdgeHupPri = ::EPOLL_EVENTS::EPOLLOUT | ::EPOLL_EVENTS::EPOLLET | ::EPOLL_EVENTS::EPOLLHUP | ::EPOLL_EVENTS::EPOLLPRI,
            kReadWriteEdgeHupPri = ::EPOLL_EVENTS::EPOLLIN | ::EPOLL_EVENTS::EPOLLOUT | ::EPOLL_EVENTS::EPOLLET | ::EPOLL_EVENTS::EPOLLHUP | ::EPOLL_EVENTS::EPOLLPRI,
            kReadEdgeErrHupPri = ::EPOLL_EVENTS::EPOLLIN | ::EPOLL_EVENTS::EPOLLERR | ::EPOLL_EVENTS::EPOLLET | ::EPOLL_EVENTS::EPOLLHUP | ::EPOLL_EVENTS::EPOLLPRI,
            kReadErr = ::EPOLL_EVENTS::EPOLLIN | ::EPOLL_EVENTS::EPOLLERR,
            kWriteErr = ::EPOLL_EVENTS::EPOLLOUT | ::EPOLL_EVENTS::EPOLLERR,
            kError = ::EPOLL_EVENTS::EPOLLERR,
            kHangup = ::EPOLL_EVENTS::EPOLLHUP
        };

        inline auto ToString(const EventType event_type) -> std::string
        {
            std::string printable;
            switch (event_type)
            {
                case EventType::kRead:
                    printable = "Read";
                    break;
                case EventType::kWrite:
                    printable = "Write";
                    break;
                case EventType::kReadWrite:
                    printable = "Read,Write";
                    break;
                case EventType::kPriEdge:
                    printable = "Priority,Edge";
                    break;
                case EventType::kEdge:
                    printable = "Edge";
                    break;   
                case EventType::kReadEdge:
                    printable = "Read,Edge";
                    break;
                case EventType::kWriteEdge:
                    printable = "Write,Edge";
                    break;
                case EventType::kReadWriteEdge:
                    printable = "Read,Write,Edge";
                    break;
                case EventType::kReadEdgePri:
                    printable = "Read,Priority,Edge";
                    break;
                case EventType::kWriteEdgePri:
                    printable = "Write,Priority,Edge";
                    break;
                case EventType::kReadWriteEdgePri:
                    printable = "Read,Write,Priority,Edge";
                    break;
                case EventType::kReadEdgeHupPri:
                    printable = "Read,Edge,Hangup,Priority";
                    break;
                case EventType::kReadWriteErrHup:
                    printable = "Read,Write,Error,Hangup";
                    break;
                case EventType::kReadHangup:
                    printable = "Read,Hangup";
                    break;
                case EventType::kWriteHangup:
                    printable = "Write,Hangup";
                    break;
                case EventType::kRdHup:
                    printable = "RDHUP";
                    break; 
                case EventType::kHangup:
                    printable = "Hangup";
                    break; 
                case EventType::kError:
                    printable = "Error";
                    break;  
                case EventType::kReadErr:
                    printable = "ReadError";
                    break; 
                case EventType::kWriteErr:
                    printable = "WriteError";
                    break;      
                default:
                    printable = "Unrecognized";
                    break;
            }
            return printable;
        }
    } //end ns details

    class Events
    {
    private:
        enum class SoftwareEventType : std::uint8_t {
            null_sw_event = 0x0u, 
            oneshot_sw_event = 0x01u, 
            persitent_sw_event = 0x02u
        };
        SoftwareEventType software_event_{SoftwareEventType::null_sw_event};
        EpollEventType event_mask_{};
    public:

        using UnderlyingType = EpollEventType;
        using EventType = details::EventType;

        struct OneShotSoftwareEventTag_t {};
        struct PersistentSoftwareEventTag_t {};
        inline static constexpr OneShotSoftwareEventTag_t OneShotSoftwareEventTag{};
        inline static constexpr PersistentSoftwareEventTag_t PersistentSoftwareEventTag{};
        
        constexpr Events() noexcept = default;

        constexpr explicit Events(OneShotSoftwareEventTag_t) noexcept
            : software_event_{SoftwareEventType::oneshot_sw_event} {}

        constexpr explicit Events(PersistentSoftwareEventTag_t) noexcept
            : software_event_{SoftwareEventType::persitent_sw_event} {}

        Events(Events const&) noexcept = default;
        Events(Events&&) noexcept = default;
        Events &operator=(Events const &other) noexcept = default;
        Events &operator=(Events &&other) noexcept = default;
        constexpr ~Events() noexcept = default;
    
        constexpr auto operator<=>(Events const &other) const noexcept = default;

        constexpr bool operator<(Events const &) = delete;
        constexpr bool operator>(Events const &) = delete;

        /* constructor from event mask */
        constexpr explicit Events(std::uint32_t epoll_event) noexcept : event_mask_{epoll_event} {}

        /* conversion constructor from event type */
        constexpr Events(EventType type) noexcept : event_mask_{static_cast<EpollEventType>(type)} {}

        Events& Add(EpollEventType epoll_event)
        {
            this->event_mask_ |= epoll_event;
            return *this;
        }

        Events &SetReadEvent(bool enable) noexcept
        {
            this->SetEvent(EventType::kRead, enable);
            return *this;
        }

        Events &SetWriteEvent(bool enable) noexcept
        {
            this->SetEvent(EventType::kWrite, enable);
            return *this;
        }

        constexpr void Reset() noexcept
        {
            this->event_mask_ = 0;
        }

        constexpr void Consume(Events ev) noexcept
        {
            this->event_mask_ &= ~ev.event_mask_;
        }

        constexpr bool Contains(Events ev) const noexcept
        {
            return (this->event_mask_ & ev.event_mask_) != 0; 
        }

        Events Union(Events ev) const noexcept
        {
            return Events{this->event_mask_ | ev.event_mask_};
        }

        Events Intersection(Events ev) const noexcept
        {
            return Events{this->event_mask_ & ev.event_mask_};
        }

        Events& operator+=(const Events& rhs) noexcept
        {
            this->event_mask_ |= rhs.event_mask_;
            return *this;
        }

        Events& operator-=(const Events& rhs) noexcept
        {
            this->event_mask_ &= ~rhs.event_mask_;
            return *this;
        }

        friend Events operator+(Events lhs, const Events& rhs) noexcept
        {
            lhs += rhs;
            return lhs;
        }

        friend Events operator-(Events lhs, const Events& rhs) noexcept
        {
            lhs -= rhs;
            return lhs;
        }
        
        constexpr bool Empty() const noexcept {return this->event_mask_ == 0;}

        constexpr bool HasReadEvent() const noexcept { return this->HasEvent(EventType::kRead); }

        constexpr bool HasSoftwareEvent() const noexcept 
        { return this->software_event_ != SoftwareEventType::null_sw_event; }

        constexpr bool HasPersistentSoftwareEvent() const noexcept 
        { return this->software_event_ == SoftwareEventType::persitent_sw_event; }

        constexpr bool HasOneShotSoftwareEvent() const noexcept 
        { return this->software_event_ == SoftwareEventType::oneshot_sw_event; }

        constexpr bool HasWriteEvent() const noexcept { return this->HasEvent(EventType::kWrite); }

        constexpr bool HasIoEvent() const noexcept { return this->HasReadEvent() || this->HasWriteEvent(); }

        constexpr auto GetIoEvents() const noexcept -> Events
        {
            return Events{this->event_mask_ & (EPOLLIN | EPOLLOUT)};
        }

        constexpr bool HasEvent(EventType ev) const noexcept
        {
            return (this->event_mask_ & static_cast<decltype(this->event_mask_)>(ev));
        }

        constexpr auto ExtractEpollEvent() const noexcept -> decltype(this->event_mask_)
        {
            return this->event_mask_;
        }

        constexpr bool HasAnyEvents() const noexcept {return this->event_mask_ != 0;}


        auto ToString() const noexcept
        {
            return details::ToString(EventType{event_mask_});
        }

        friend std::ostream& operator<<(std::ostream& os, const Events& events) noexcept
        {
            os << "[" << details::ToString(static_cast<EventType>(events.event_mask_))  << "]";
            return os;
        }

    private:

        constexpr void SetEvent(EventType ev, bool enable)
        {
            if (enable)
                this->event_mask_ |= static_cast<decltype(this->event_mask_)>(ev);
            else
                this->event_mask_ &= ~static_cast<decltype(this->event_mask_)>(ev);
        }
    }; //class Events

    using HandlerTag = std::uint32_t;
    using TimerTag = Timer::Types::TimerTag;
    using EventHandlerLockType = std::unique_lock<asrt::config::ReactorMutexType>;
    using EventHandler = std::function<void(EventHandlerLockType&, Events, HandlerTag)>;
    using TimerEventHandler = Timer::Types::EventHandler;

    inline constexpr HandlerTag kInvalidHandlerTag{std::numeric_limits<HandlerTag>::max()};
    inline constexpr Events OneShotSoftwareEvent{Events::OneShotSoftwareEventTag};
    inline constexpr Events PersistentSoftwareEvent{Events::PersistentSoftwareEventTag};

    struct ReactorRegistry{
        HandlerTag tag;
        asrt::config::ReactorMutexType& mutex;
    };


    enum class CloseIOSourceFlag : std::uint8_t
    {
        kDoNotCloseSource,
        kCloseSource
    };

    enum class ReactorState : std::uint8_t{
        kRunning,
        kUnbocked
    };

    using OperationType = details::EventType;

} //end ns Types
} //end ns ReactorNS


#endif /* C90E6770_B081_438A_A1DF_95DCC156B1DC */
