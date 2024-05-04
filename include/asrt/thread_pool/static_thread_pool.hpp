#ifndef A8F6359D_0BEA_4204_96A0_96915BEDA568
#define A8F6359D_0BEA_4204_96A0_96915BEDA568

#include <vector>
#include <thread>
#include <atomic>

#include "asrt/details/registry.hpp"

namespace asrt{

namespace details{

inline constexpr std::size_t k_max_thread_pool_thread_count{16u}; //todo

inline long rational_thread_count(std::size_t num_threads) noexcept
{
    return std::min(num_threads, k_max_thread_pool_thread_count);
}

inline long default_thread_count() noexcept
{
    std::size_t num_threads{std::thread::hardware_concurrency() * 2}; //todo need to account for numa
    num_threads = num_threads == 0 ? 
        2 : rational_thread_count(num_threads);
    return static_cast<long>(num_threads);
}

inline auto default_executor_raw() noexcept
{
    return asrt::details::ExecutorRegistry::Instance().GetDefaultExecutorUnsafe();
}

}

class static_thread_pool{

public:

    static_thread_pool() noexcept
        : executor_{details::default_executor_raw()},
          num_threads_{details::default_thread_count()}
    {
        this->executor_->OnJobArrival();
        this->start_threads_();
    }

    static_thread_pool(std::size_t num_threads) noexcept
        : executor_{details::default_executor_raw()},
          num_threads_{details::rational_thread_count(num_threads)}
    {
        this->executor_->OnJobArrival();
        this->start_threads_();
    }

    auto& get_executor() noexcept
    {
        return *this->executor_;
    }

    void attach_this_thread() noexcept
    {
        ++this->num_threads_;
        executor_->Run();
    }

    void stop() noexcept
    {
        this->executor_->Stop();
    }

    void join() noexcept
    {
        std::ranges::for_each(this->threads_,
            [](auto& t){
                if(t.joinable()){
                    t.join();
                }
            });
    }

    ~static_thread_pool() noexcept
    {
        this->executor_->OnJobCompletion();
        this->stop();
    }

private:
    using Executor = asrt::details::ExecutorRegistry::Executor;
    using ThreadType = std::jthread;
    using ThreadGroup = std::vector<ThreadType>;

    void start_threads_() noexcept
    {
        const long num_thr{num_threads_};
        for(long i = 0; i < num_thr; i++)
            this->threads_.emplace_back(
                [ex = executor_]{ (void)ex->Run(); });
    }

    Executor* executor_;
    ThreadGroup threads_;
    std::atomic<long> num_threads_;
};





}
#endif /* A8F6359D_0BEA_4204_96A0_96915BEDA568 */
