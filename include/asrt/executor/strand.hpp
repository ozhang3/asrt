#ifndef D4D22DAB_A4AD_4790_9E1D_1ABA8EA1F081
#define D4D22DAB_A4AD_4790_9E1D_1ABA8EA1F081

#include <deque>
#include <functional>
#include <mutex>
#include <memory>

#include "asrt/config.hpp"
#include "asrt/util.hpp"
#include "asrt/executor/types.hpp"
#include "asrt/executor/details.hpp"

namespace ExecutorNS{
using namespace Util::Expected_NS;
using OperationQueue = std::deque<ExecutorOperation>;
using StrandJob = ExecutorOperation;

template <typename Executor>
class Strand{
public:

    explicit Strand(Executor& executor) noexcept : executor_{executor} {}
    Strand(Strand const&) = delete;
    Strand(Strand&& other) noexcept
    {
        this->this_strand_ = std::move(other.this_strand_);
    }
    Strand &operator=(Strand const &other) = delete;
    Strand &operator=(Strand &&other)
    {   
        this->this_strand_ = std::move(other.this_strand_);
        return *this;
    }
    ~Strand() noexcept = default;

    template <typename Executable>
    void Post(Executable&& op);

    template <typename Executable>
    void Dispatch(Executable&& op);   

private:

    bool IsContinuation() noexcept
    {
        return ExecutionContext<Strand>::IsInContext();
    }

    void ExecutePendingJobs();

    struct StrandInfo{
        std::mutex mtx_;
        OperationQueue jobs_;
        bool is_running_{false};
    }this_strand_;
    
    Executor& executor_;
};

}

#if defined(ASRT_HEADER_ONLY)
# include "asrt/executor/impl/strand.ipp"
#endif // defined(ASRT_HEADER_ONLY)

#endif /* D4D22DAB_A4AD_4790_9E1D_1ABA8EA1F081 */
