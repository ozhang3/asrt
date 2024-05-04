#ifndef BD6B3CA4_271B_4A41_BF21_5CB80007F31D
#define BD6B3CA4_271B_4A41_BF21_5CB80007F31D

#include <sys/epoll.h>
#include "asrt/common_types.hpp"
#include "asrt/util.hpp"
#include "asrt/error_code.hpp"
#include "asrt/sys/syscall.hpp"
#include "asrt/config.hpp"

namespace Epoll_NS
{ 
    using namespace Util::Expected_NS;
    /*
        A thin wrapper of an epoll instance that does NOT take ownership of the underlying resources
        It is the user's responsibility to release the resource, ie: calling close() when finished using it
    */
    class EpollWrapper final
    {
        static_assert(ASRT_HAS_EPOLL, "Epoll not available with selected linux kernel");

    public:
        using Desciptor = asrt::NativeHandle;
        using ErrorCode = ErrorCode_Ns::ErrorCode;
        template <typename T> using Result = Util::Expected_NS::Expected<T, ErrorCode>;

        //explicit EpollWrapper(Desciptor epfd) : epollfd_{epfd} {}
        EpollWrapper() = default;
        EpollWrapper(EpollWrapper const&) = delete;
        EpollWrapper(EpollWrapper&&) = delete;
        EpollWrapper &operator=(EpollWrapper const &other) = delete;
        EpollWrapper &operator=(EpollWrapper &&other) = delete;
        ~EpollWrapper() noexcept = default;    

        static auto Create(bool CloseOnExecFlag = true) noexcept -> Result<Desciptor>;

        auto Open(bool CloseOnExecFlag = true) noexcept -> Result<void>;

        auto AssignHandle(Desciptor epfd) noexcept -> Result<void>;

        auto Add(Desciptor fd, const ::epoll_event& epoll_ev) noexcept -> Result<void>;
        auto Modify(Desciptor fd, const ::epoll_event& epoll_ev) noexcept -> Result<void>;

        /* Remove a file descriptor from the interface.  */
        auto Remove(Desciptor fd) noexcept -> Result<void>;
        bool IsValid() const noexcept {return this->epollfd_ != asrt::kInvalidNativeHandle;}

        template<typename EventStorage>
        auto WaitForEvents(EventStorage& events, int timeout_ms) noexcept -> Result<unsigned int>;

        auto Close() noexcept -> Result<void>;
    private:
        
        auto ValidateEntry(Desciptor fd, const ::epoll_event &epoll_ev) noexcept -> Result<void>;
        Desciptor epollfd_{asrt::kInvalidNativeHandle};

    };

    inline auto EpollWrapper::
    Create(bool CloseOnExecFlag) noexcept -> Result<Desciptor>
    {
        int flag{CloseOnExecFlag ? EPOLL_CLOEXEC : 0};
        return OsAbstraction::EpollCreate(flag);
    }

    inline auto EpollWrapper::
    Open(bool CloseOnExecFlag) noexcept -> Result<void>
    {
        if(!this->IsValid()){
            return 
                OsAbstraction::EpollCreate(CloseOnExecFlag ? EPOLL_CLOEXEC : 0)
                .map([this](int epollfd){
                    this->epollfd_ = epollfd;
                });
        }else{
            return MakeUnexpected(ErrorCode::default_error); //todo
        }
    }

    inline auto EpollWrapper::
    AssignHandle(Desciptor epfd) noexcept -> Result<void>
    {
        if(!this->IsValid()){
            this->epollfd_ = epfd;
            return {};
        }else{ /* epoll fd already exists */
            return MakeUnexpected(ErrorCode::default_error);
        }
    }

    inline auto EpollWrapper::
    ValidateEntry(Desciptor fd, const ::epoll_event &epoll_ev) noexcept -> Result<void>
    {
        (void)fd; (void)epoll_ev;
        return this->IsValid() ? 
            Result<void>{} :
            MakeUnexpected(ErrorCode::reactor_not_valid); //todo: supply meaningful error
#if 0
        return 
            (fd != asrt::kInvalidNativeHandle)  ?
                Result<void>{} :
                MakeUnexpected(ErrorCode::reactor_not_valid); //todo: supply meaningful error
#endif
    }

    inline auto EpollWrapper::
    Add(Desciptor fd, const ::epoll_event& epoll_ev) noexcept -> Result<void>
    {
        return this->ValidateEntry(fd, epoll_ev)
            .and_then([this, fd, &epoll_ev]() -> Result<void> {
                ASRT_LOG_DEBUG("[Epoll Wrapper] add epoll event: {:#x} epoll data: {:#x} for fd {}", 
                    epoll_ev.events, epoll_ev.data.u32, fd);
                return OsAbstraction::EpollControl(this->epollfd_, EPOLL_CTL_ADD, fd, 
                    const_cast<::epoll_event*>(&epoll_ev)); 
            });
    }

    inline auto EpollWrapper::
    Modify(Desciptor fd, const ::epoll_event& epoll_ev) noexcept -> Result<void>
    {
        return this->ValidateEntry(fd, epoll_ev)
            .and_then([this, fd, &epoll_ev]() -> Result<void> {
                ASRT_LOG_DEBUG("[Epoll Wrapper] modify epoll event: {:#x} epoll data: {:#x} for fd {}", 
                    epoll_ev.events, epoll_ev.data.u32, fd);
                return OsAbstraction::EpollControl(this->epollfd_, EPOLL_CTL_MOD, fd, 
                    const_cast<::epoll_event*>(&epoll_ev)); 
            });
    }

    inline auto EpollWrapper::
    Remove(Desciptor fd) noexcept -> Result<void>
    {
        return OsAbstraction::EpollControl(this->epollfd_, EPOLL_CTL_DEL, fd, nullptr); 
    }
    
    template<typename EventStorage>
    inline auto EpollWrapper::
    WaitForEvents(EventStorage& events, int timeout_ms) noexcept -> Result<unsigned int>
    {
        return OsAbstraction::EpollWait(this->epollfd_, events.data(), static_cast<int>(events.size()), timeout_ms);
    }

    inline auto EpollWrapper::
    Close() noexcept -> Result<void>
    {
        if(this->IsValid())
            return OsAbstraction::Close(this->epollfd_)
                .map([this](){
                    ASRT_LOG_TRACE("[EpollWrapper]: Closed epoll fd {}", this->epollfd_);
                    this->epollfd_ = asrt::kInvalidNativeHandle;
                })
                .map_error([this](ErrorCode ec){ 
                    ASRT_LOG_WARN("Failed to close epoll fd {}", this->epollfd_);
                    return ec; 
                });

        else
            return MakeUnexpected(ErrorCode::default_error); //todo
    }

}

#endif /* BD6B3CA4_271B_4A41_BF21_5CB80007F31D */
