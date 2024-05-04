#ifndef E1547DF5_DBFE_423B_A767_EF56254CB6E1
#define E1547DF5_DBFE_423B_A767_EF56254CB6E1

#include <memory>
#include <atomic>
#include <optional>

#include "asrt/config.hpp"
#include "asrt/executor/io_executor.hpp"
#include "asrt/reactor/epoll_reactor.hpp"
#include "asrt/reactor/io_uring_reactor.hpp"

namespace asrt {
namespace details{

class ExecutorRegistry{
public:

    using Executor = asrt::config::DefaultExecutor;

    ExecutorRegistry(const ExecutorRegistry &) = delete;
    ExecutorRegistry &operator=(const ExecutorRegistry &) = delete;

    /**
     * @brief Returns shared pointer to current default global executor managed by this singelton
     * 
     * @return std::shared_ptr<Executor> 
     */
    auto GetDefaultExecutor() -> std::shared_ptr<Executor>
    {
#ifndef __cpp_lib_atomic_shared_ptr
        std::scoped_lock const lock{this->default_executor_access_mtx_};
#endif          
        return this->default_executor_;
    }

    /**
     * @brief Get raw pointer to global default executor
     * @warning Not thread safe. User needs to prevent concurrent calls to SetDefaultExecutor() 
     * 
     * @return Executor* 
     */
    auto GetDefaultExecutorUnsafe() -> Executor*
    {
        return this->default_executor_.get();
    }

    /**
     * @brief Sets default Executor to new executor
     * 
     * @param new_executor
     */
    void SetDefaultExecutor(std::shared_ptr<Executor> new_executor)
    {
#ifndef __cpp_lib_atomic_shared_ptr
        std::scoped_lock const lock{this->default_executor_access_mtx_};
#endif
        this->default_executor_ = std::move(new_executor);
    }

    /**
     * @brief Returns a refence to this singleton object
     * 
     * @return ExecutorRegistry& 
     */
    static ExecutorRegistry& Instance() noexcept
    {
        static ExecutorRegistry singleton;
        return singleton;
    }

private:

    ExecutorRegistry() noexcept
    {
        this->default_executor_ = std::make_shared<Executor>();
    }

    ~ExecutorRegistry() noexcept = default;

#ifndef __cpp_lib_atomic_shared_ptr
    std::mutex default_executor_access_mtx_;
    std::shared_ptr<Executor> default_executor_;
#else
    std::atomic_shared_ptr<Executor> default_executor_;
#endif
};



}
}
#endif /* E1547DF5_DBFE_423B_A767_EF56254CB6E1 */
