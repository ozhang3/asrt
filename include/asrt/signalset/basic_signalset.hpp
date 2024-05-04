#ifndef A4E61F3A_3189_42F3_83B2_FE8817537D3F
#define A4E61F3A_3189_42F3_83B2_FE8817537D3F

#include <cstdint>
#include <csignal>
#include <sys/signalfd.h>
#include <memory>
#include <concepts>
#include <string_view>
#include <functional>

#include "asrt/common_types.hpp"
#include "asrt/signalset/signal_set_base.hpp"
#include "expected.hpp"
#include "asrt/error_code.hpp"
#include "asrt/util.hpp"
#include "asrt/reactor/reactor_interface.hpp"

using namespace Util::Expected_NS;

// /* ISO C99 signals.  */
// #define	SIGINT		2	/* Interactive attention signal.  */
// #define	SIGILL		4	/* Illegal instruction.  */
// #define	SIGABRT		6	/* Abnormal termination.  */
// #define	SIGFPE		8	/* Erroneous arithmetic operation.  */
// #define	SIGSEGV		11	/* Invalid access to storage.  */
// #define	SIGTERM		15	/* Termination request.  */

// /* Historical signals specified by POSIX. */
// #define	SIGHUP		1	/* Hangup.  */
// #define	SIGQUIT		3	/* Quit.  */
// #define	SIGTRAP		5	/* Trace/breakpoint trap.  */
// #define	SIGKILL		9	/* Killed.  */
// #define SIGBUS		10	/* Bus error.  */
// #define	SIGSYS		12	/* Bad system call.  */
// #define	SIGPIPE		13	/* Broken pipe.  */
// #define	SIGALRM		14	/* Alarm clock.  */

// /* New(er) POSIX signals (1003.1-2008, 1003.1-2013).  */
// #define	SIGURG		16	/* Urgent data is available at a socket.  */
// #define	SIGSTOP		17	/* Stop, unblockable.  */
// #define	SIGTSTP		18	/* Keyboard stop.  */
// #define	SIGCONT		19	/* Continue.  */
// #define	SIGCHLD		20	/* Child terminated or stopped.  */
// #define	SIGTTIN		21	/* Background read from control terminal.  */
// #define	SIGTTOU		22	/* Background write to control terminal.  */
// #define	SIGPOLL		23	/* Pollable event occurred (System V).  */
// #define	SIGXCPU		24	/* CPU time limit exceeded.  */
// #define	SIGXFSZ		25	/* File size limit exceeded.  */
// #define	SIGVTALRM	26	/* Virtual timer expired.  */
// #define	SIGPROF		27	/* Profiling timer expired.  */
// #define	SIGUSR1		30	/* User-defined signal 1.  */
// #define	SIGUSR2		31	/* User-defined signal 2.  */

// /* Nonstandard signals found in all modern POSIX systems
//    (including both BSD and Linux).  */
// #define	SIGWINCH	28	/* Window size change (4.3 BSD, Sun).  */

// /* Archaic names for compatibility.  */
// #define	SIGIO		SIGPOLL	/* I/O now possible (4.2 BSD).  */
// #define	SIGIOT		SIGABRT	/* IOT instruction, abort() on a PDP-11.  */
// #define	SIGCLD		SIGCHLD	/* Old System V name */

enum class SignalNumber{
    interrupt = SIGINT
};
static constexpr std::size_t kMaxSigNum{SIGWINCH};
static constexpr inline Util::ConstexprMap<int, std::string_view, kMaxSigNum> kSignalPrintoutMap{{{
    {SIGINT, "SIGINT"},
    {SIGKILL, "SIGKILL"},
    {SIGTERM, "SIGTERM"},
    {SIGQUIT, "SIGQUIT"},
    {SIGALRM, "SIGALRM"}
}}};

constexpr inline auto ToStringView(int signal_number) -> std::string_view
{
    return kSignalPrintoutMap.at(signal_number);
}

constexpr inline bool IsSynchronous(int signal_number)
{
    return (signal_number == SIGSEGV) || (signal_number == SIGFPE);
}

template <class Executor>
class BasicSignalSet : public SignalSetBase
{
public:

    /* Class scope using directives */
    using ExecutorType = Executor;
    using Reactor = typename Executor::ReactorType;
    using MutexType = Reactor::MutexType;
    using NativeHandle = asrt::NativeHandle;
    using Events = ReactorNS::Events;
    using EventType = ReactorNS::EventType;
    using ErrorCode = ErrorCode_Ns::ErrorCode;
    template <typename T>
    using Result = Expected<T, ErrorCode>;
    using ReactorHandle = typename Reactor::HandlerTag;
    using SignalNumberType = int;
    using WaitCompletionHandler = std::function<void(Result<int>)>;

    /* @brief A default constructed socket can NOT perform socket operations without first being Open()ed */
    BasicSignalSet() noexcept = default;
    explicit BasicSignalSet(Executor& executor) noexcept;

    template <std::same_as<int> ...SigNum>
    explicit BasicSignalSet(Executor& executor, SigNum... signals) noexcept;

    BasicSignalSet(BasicSignalSet const&) = delete;
    BasicSignalSet(BasicSignalSet&&) = delete;
    BasicSignalSet &operator=(BasicSignalSet const &other) = delete;
    BasicSignalSet &operator=(BasicSignalSet &&other) = delete;

    ~BasicSignalSet() noexcept;

    template <std::same_as<int> ...SigNum>
    auto Add(SigNum... signals) -> Result<void>;

    auto Wait() -> Result<int>;

    template <typename WaitCompletionCallback>
    void WaitAsync(WaitCompletionCallback&& signal_handler);

    void Cancel();

    /**
     * @brief Blocks all signals in current signalset for current thread
     * 
     * @return Result<void> 
     */
    auto SetCurrentThreadMask() -> Result<void>;

    template <std::same_as<int> ...SigNum>
    auto SetCurrentThreadMask(SigNum... signals) -> Result<void>;

    void OnReactorEvent(std::unique_lock<MutexType>& lock, Events events);

private:

    template <std::same_as<int> ...SigNum>
    auto DoAddSignals(SigNum... signals) -> Result<void>;

    auto DoReadSignalsSync() -> Result<int>;

    template <typename WaitCompletionCallback, typename OnImmediateCompletion>
    void DoReadSignalsAsync(WaitCompletionCallback&& completion_callback, OnImmediateCompletion&& on_immediate_completion);

    const auto MakeReactorEventHandler();

    auto RegisterToReactor() -> Result<void>;

    auto GetMutex() -> MutexType&
    {
        return this->reactive_sigset_mtx_ ?
                    *(this->reactive_sigset_mtx_) :
                    this->fallback_mtx_;
    }

    auto GetMutexUnsafe() -> MutexType& 
    {
        assert(this->reactive_sigset_mtx_);
        return *(this->reactive_sigset_mtx_);
    }

    auto AcquireNativeHandle(int flags) -> Result<void>;

    bool IsAsyncPreconditionsMet()
    {
        return this->reactor_.has_value() && this->is_native_nonblocking_;
    }

    void RegisterReactorEvent();

    template <typename T> using Optional = Util::Optional_NS::Optional<T>;

    Optional<Executor&> executor_;
    Optional<Reactor&> reactor_;
    MutexType fallback_mtx_;
    MutexType* reactive_sigset_mtx_{nullptr};
    NativeHandle native_handle_{asrt::kInvalidNativeHandle};
    ReactorHandle reactor_handle_{ReactorNS::Types::kInvalidHandlerTag};
    ::sigset_t signal_set_{};
    bool is_native_nonblocking_{false};
    bool is_wait_ongoing_{false};
    bool speculative_read_{false};
    WaitCompletionHandler wait_completion_handler_;
};


#if defined(ASRT_HEADER_ONLY)
# include "asrt/signalset/impl/basic_signalset.ipp"
#endif // defined(ASRT_HEADER_ONLY)

#endif /* A4E61F3A_3189_42F3_83B2_FE8817537D3F */
