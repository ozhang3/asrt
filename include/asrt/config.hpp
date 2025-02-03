#ifndef DA339FF9_EEAD_4A75_A747_044428B06382
#define DA339FF9_EEAD_4A75_A747_044428B06382

#include <cstdint>
#include <limits>
#include <mutex>
//#include "asrt/reactor/epoll_reactor.hpp"
//#include "asrt/timer/timer_queue.hpp"
//#include "asrt/executor/io_executor.hpp"

//#define NDEBUG //uncomment for release

// Linux: epoll, eventfd, timerfd and io_uring.
#if defined(__linux__)
# include <linux/version.h>
# if !defined(ASRT_HAS_EPOLL)
#  if !defined(ASRT_DISABLE_EPOLL)
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,45)
#    define ASRT_HAS_EPOLL 1
#   endif // LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,45)
#  endif // !defined(ASRT_DISABLE_EPOLL)
# endif // !defined(ASRT_HAS_EPOLL)
# if !defined(ASRT_HAS_EVENTFD)
#  if !defined(ASRT_DISABLE_EVENTFD)
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
#    define ASRT_HAS_EVENTFD 1
#   endif // LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
#  endif // !defined(ASRT_DISABLE_EVENTFD)
# endif // !defined(ASRT_HAS_EVENTFD)
# if !defined(ASRT_HAS_TIMERFD)
#  if defined(ASRT_HAS_EPOLL)
#   if (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 8)
#    define ASRT_HAS_TIMERFD 1
#   endif // (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 8)
#  endif // defined(ASRT_HAS_EPOLL)
# endif // !defined(ASRT_HAS_TIMERFD)
# if defined(ASRT_HAS_IO_URING)
#  if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
#   error Linux kernel 5.10 or later is required to support io_uring
#  endif // LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
# endif // defined(ASRT_HAS_IO_URING)
#else // defined(__linux__)
# error Linux kernel required to use this library
#endif // defined(__linux__)

// Default to a header-only implementation. The user must specifically request
// separate compilation by defining either ASRT_COMPILED_LIB or
// ASRT_SHARED_LIB (as a DLL/shared library implies separate compilation).
#if !defined(ASRT_HEADER_ONLY)
# if !defined(ASRT_COMPILED_LIB)
#  if !defined(ASRT_SHARED_LIB)
#   define ASRT_HEADER_ONLY
#  endif // !defined(ASRT_SHARED_LIB)
# endif // !defined(ASRT_COMPILED_LIB)
#endif // !defined(ASRT_HEADER_ONLY)

// Generic helper definitions for shared library support
#if defined _WIN32 || defined __CYGWIN__
#   define ASRT_DLL_IMPORT __declspec(dllimport)
#   define ASRT_DLL_EXPORT __declspec(dllexport)
#else
    #define ASRT_DLL_IMPORT __attribute__((visibility ("default")))
    #define ASRT_DLL_EXPORT __attribute__((visibility ("default")))
#endif

#ifdef ASRT_HEADER_ONLY
#   define ASRT_INLINE inline
#   define ASRT_API
#else
#   define ASRT_INLINE
#   if defined(ASRT_SHARED_LIB)
#       if defined(ASRT_EXPORTS)
#           define ASRT_API ASRT_DLL_EXPORT
#       else
#           define ASRT_API ASRT_DLL_IMPORT
#       endif
#   else
#       define ASRT_API
#   endif
#endif

#ifdef APPLICATION_IS_SINGLE_THREADED
    #define DSISABLE_LOCKING_EXECUTOR_REACTOR_UNSAFE
    #define EXECUTOR_HAS_THREADS false
    #define STRAND_HAS_THREADS false
    #define REACTOR_NO_SYNCHRONIZATION true
#else
    #ifdef DSISABLE_LOCKING_EXECUTOR_REACTOR_UNSAFE
        #undef DSISABLE_LOCKING_EXECUTOR_REACTOR_UNSAFE
    #endif
    #define EXECUTOR_HAS_THREADS true
    #define STRAND_HAS_THREADS true
#endif

#ifdef ASRT_HAS_IO_URING
    #define USE_IO_URING_REACTOR
#else
    #ifdef ASRT_HAS_EPOLL
        #define USE_EPOLL_REACTOR
    #else
        #define ASRT_HAS_NO_REACTOR
    #endif
#endif

namespace Util {struct NullMutex;}

namespace ReactorNS {
    class NullReactor;

    class EpollReactor;

    class IoUringReactor;
}

namespace ExecutorNS {
    template <typename Reactor> 
    class IO_Executor;
}
namespace Timer {
    class NullTimer;

    template <typename Reactor>
    class TimerQueue;
}

namespace asrt{
namespace config{

#ifdef DSISABLE_LOCKING_EXECUTOR_REACTOR_UNSAFE
    using ExecutorMutexType = Util::NullMutex;
    using ReactorMutexType = Util::NullMutex;
#else
    using ExecutorMutexType = std::mutex;
    using ReactorMutexType = std::mutex;
#endif

#ifdef USE_IO_URING_REACTOR
    using DeafaultReactorService = ReactorNS::IoUringReactor;
#else 
    #ifdef USE_EPOLL_REACTOR
        //#include "asrt/reactor/epoll_reactor.hpp"
        using DeafaultReactorService = ReactorNS::EpollReactor;
    #else
        using DeafaultReactorService = ReactorNS::NullReactor; 
    #endif
#endif

#ifdef ASRT_HAS_TIMERFD
    //#include "asrt/timer/timer_queue.hpp"
    using DefaultTimerService = Timer::TimerQueue<DeafaultReactorService>;
#else
    using DefaultTimerService = Timer::NullTimer;
#endif

    using DefaultExecutor = ExecutorNS::IO_Executor<DeafaultReactorService>; 

    static constexpr std::uint8_t kMaxTimerQueueSize{std::numeric_limits<std::uint8_t>::max()};
    static constexpr std::uint8_t kMaxReactorHandlerCount{std::numeric_limits<std::uint8_t>::max()};

}
}

#endif /* DA339FF9_EEAD_4A75_A747_044428B06382 */
