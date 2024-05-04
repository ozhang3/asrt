#ifndef DB643653_3AAA_418B_83D9_561D0B54A6D7
#define DB643653_3AAA_418B_83D9_561D0B54A6D7

#include <cstdint>
#include <deque>

#include "asrt/common_types.hpp"
#include "asrt/util.hpp"
#include "asrt/error_code.hpp"
#include "asrt/reactor/types.hpp"
#include "asrt/executor/types.hpp"
#include "asrt/timer/timer_types.hpp"

namespace ExecutorNS{


using ExecutorOperation = asrt::UniqueFunction<void()>;
using OperationQueue = std::deque<ExecutorOperation>;

using namespace Util::Expected_NS;
using ErrorCode = ErrorCode_Ns::ErrorCode;

template <typename T>
using Result = Expected<T, ErrorCode>;

using ReactorUnblockReason = Types::UnblockReason;


static constexpr std::uint16_t kReactorHandlerCount{16u};
static constexpr std::uint16_t kConcurrentTimerCountHint{16u};

struct ThreadInfo{
    std::deque<ExecutorOperation> private_op_queue_;
    std::uint32_t private_job_count_{0};
};

enum class JobContext{ kClient, kExecutor };

enum class OperationType{ kStart, kContinuation };

template <typename Content>
class ExecutionContext{
public:
    explicit ExecutionContext(Content& content) noexcept
    {
        content_ = &content;
        context_marker_ = true;
    }

    /* important! reset the marker and info on destruction */
    ~ExecutionContext() noexcept
    {
        content_ = nullptr;
        context_marker_ = false;
    }

    static bool IsInContext() {return context_marker_;}
    static Content* RetrieveContent() {return content_;}
private:
    static inline thread_local Content* content_{nullptr};
    static inline thread_local bool context_marker_{false};
};


struct ExecutorConfig{
    bool single_threaded_{false};
    std::size_t concurrency_hint_{1};
};

enum class ExecutorConcurrency{
    SingleThreaded,
    MultiThreaded
};

static constexpr inline ExecutorConfig kDefaultExConfig{ExecutorConfig{false, 1}};


struct ConditionalMutex{

    explicit ConditionalMutex(bool enable) {}

private:
    
};

}

#endif /* DB643653_3AAA_418B_83D9_561D0B54A6D7 */
