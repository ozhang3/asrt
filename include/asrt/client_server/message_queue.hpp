#ifndef D6450F73_8046_4269_9ADD_01C69359F675
#define D6450F73_8046_4269_9ADD_01C69359F675

#include <cstdint>
#include <deque>
#include <mutex>

namespace ClientServer
{
    template <typename T, typename MutexType = std::mutex>
    class ThreadSafeQueue
    {
    public:
        ThreadSafeQueue() noexcept = default;
        ThreadSafeQueue(ThreadSafeQueue const&) = delete;
        ThreadSafeQueue(ThreadSafeQueue&&) = delete;
        ThreadSafeQueue &operator=(ThreadSafeQueue const &other) = delete;
        ThreadSafeQueue &operator=(ThreadSafeQueue &&other) = delete;
        ~ThreadSafeQueue() noexcept = default;

        bool is_empty() noexcept
        {
            std::scoped_lock const lock{this->mtx_};
            return this->queue_.empty();
        }
        
        const T& Front() noexcept
        {
            std::scoped_lock const lock{this->mtx_};
            assert(!this->queue_.empty());
            return this->queue_.front();
        }

        T PopFront() noexcept
        {
            std::scoped_lock const lock{this->mtx_};
            T item{std::move(this->queue_.front())};
            this->queue_.pop_back();
            return item;
        }

        T Pop() noexcept
        {
            std::unique_lock<MutexType> lock{this->mtx_};
            this->cv_.wait(lock, [this](){
                return !this->queue_.empty();
            });
            T item{std::move(this->queue_.front())};
            this->queue_.pop_back();
            return item;
        }

        void PushBack(const T& item) noexcept
        {
            std::scoped_lock const lock{this->mtx_};
            this->queue_.push_back(item);
        }

        void EmplaceBack(const T& item) noexcept
        {
            std::scoped_lock const lock{this->mtx_};
            this->queue_.emplace_back(std::move(item));
        }

        void Clear() noexcept
        {
            std::scoped_lock const lock{this->mtx_};
            this->queue_.clear();
        }

    private:
        std::deque<T> queue_;
        MutexType mtx_;
        std::condition_variable_any cv_;
    };

}

#endif /* D6450F73_8046_4269_9ADD_01C69359F675 */
