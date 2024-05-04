#ifndef DF1FDB0F_38F5_46B3_946F_B2FFB9900478
#define DF1FDB0F_38F5_46B3_946F_B2FFB9900478

#include <cstdlib>
#include <iostream>
#include <vector>
#include <algorithm>
#include <array>
#include <string>
#include <expected.hpp>
#include <optional.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "asrt/user_format.hpp"

#define ASRT_LOG_INFO(...)      SPDLOG_INFO(__VA_ARGS__)
#define ASRT_LOG_DEBUG(...)     SPDLOG_DEBUG(__VA_ARGS__)
#define ASRT_LOG_TRACE(...)     SPDLOG_TRACE(__VA_ARGS__)
#define ASRT_LOG_WARN(...)      SPDLOG_WARN(__VA_ARGS__)
#define ASRT_LOG_ERROR(...)     SPDLOG_ERROR(__VA_ARGS__)
#define ASRT_LOG_CRITICAL(...)  SPDLOG_CRITICAL(__VA_ARGS__)

#define LogFatalAndAbort(...) do {\
    ASRT_LOG_CRITICAL(__VA_ARGS__);\
    std::abort();\
} while(0)\


// #define LogFatalAndAbort(message) Util::internal::LogFatalAndAbortInternal(message, __FILE__, __func__, __LINE__)

namespace Util
{   

    namespace Expected_NS
    {
        namespace Util_Expected = tl;

        template <typename T, typename E>
        using Expected = Util_Expected::expected<T, E>;

        template <typename E>
        using Unexpected = Util_Expected::unexpected<E>;

        template <class E>
        constexpr auto MakeUnexpected(E &&e) -> Unexpected<typename std::decay<E>::type>
        {
            return Util_Expected::make_unexpected<E>(std::forward<E>(e));
        }
    }

    namespace Optional_NS
    {
        namespace Util_Optional = tl;

        template <typename T>
        using Optional = Util_Optional::optional<T>;
        
    }

    struct NullMutex{
        void lock() const {SPDLOG_TRACE("Null mutex lock");}
        void unlock() const {SPDLOG_TRACE("Null mutex unlock");}
        bool try_lock() const noexcept {SPDLOG_TRACE("Null mutex try lock"); return true;}
    };

    struct ConditionalMutex{
        ConditionalMutex(bool enable) 
            : enable_{enable} {}

        void lock() {if(enable_) mtx_.lock();}
        void unlock() {if(enable_) mtx_.unlock();}
        bool try_lock() noexcept {return (enable_ ? mtx_.try_lock() : true);}

    private:
        bool enable_;
        std::mutex mtx_;
    };

    /**
     * @brief A map that does compile-time lookups
     * 
     * @tparam Key 
     * @tparam Value 
     * @tparam Size 
     */
    template <typename Key, typename Value, std::size_t Size>
    struct ConstexprMap {
        std::array<std::pair<Key, Value>, Size> data;

        [[nodiscard]] constexpr Value at(const Key &key) const {
            const auto itr =
                std::find_if(begin(data), end(data),
                            [&key](const auto &v) { return v.first == key; });
            if (itr != end(data)) {
                return itr->second;
            } else {
                if constexpr (std::is_convertible_v<Value, const char*>)
                    return "Not found!";
                else if constexpr (std::is_integral_v<Value>)
                    return std::numeric_limits<Value>::max(); //todo
                std::abort();
                //throw std::range_error("Not Found");
            }
        }
    };
    
    // constexpr tests
    // std::pair<int, int> as{1, 1};
    // std::array<std::pair<int, int>, 1> arr{{{1, 1}}};
    // constexpr ConstexprMap<int, const char*, 1> map{{{{1, "hi"}}}};

    inline float RunningAverage(float cur_avg, std::size_t cur_count, float new_val)
    {
        return ((cur_avg * cur_count) + new_val) / (cur_count + 1);
    }

    template <typename T>
    inline void BackSwap(
        std::vector<T>& container, 
        typename std::vector<T>::iterator it)
    {
        *it = std::move(*std::prev(container.end()));
        container.pop_back(); 
    }

    template <typename T>
    inline void QuickRemoveOne(
        std::vector<T>& container, 
        typename std::vector<T>::iterator it)
    {
        assert(it != container.end());
        const auto last_elem{std::prev(container.end())};
        if(it != last_elem) {*it = std::move(*last_elem);}
        container.pop_back(); 
    }

    template <typename T>
    inline void QuickRemoveOne(
        std::vector<T>& container, 
        typename std::vector<T>::reference value)
    {
        const auto it{std::find(
            container.begin(), container.end(), value)};
        QuickRemoveOne(container, it); 
    }

    template <typename T, class UnaryPredicate>
    inline void QuickRemoveOneIf(
        std::vector<T>& container, 
        UnaryPredicate pred)
    {
        const auto it{std::find_if(
            container.begin(), container.end(), pred)};
        if(it == container.end()) return;
        QuickRemoveOne(container, it); 
    }

    template <typename T>
    inline void QuickRemoveAll(
        std::vector<T>& container, 
        typename std::vector<T>::reference value)
    {
        for(auto it{container.begin()}, end{container.end()}; 
            it != end; it++) {
            if(*it != value) continue;
            *it = std::move(*std::prev(end));
            container.pop_back(); 
        } 
    }

    template <typename T, class UnaryPredicate>
    inline auto QuickRemoveAllIf(
        std::vector<T>& container, 
        UnaryPredicate pred)
    {
        for(auto it{container.begin()}, end{container.end()}; 
            it != end; it++){
            if(!pred(*it)) continue;
            *it = std::move(*std::prev(end));
            container.pop_back(); 
        }
    }

    template <typename EnumType> 
        requires std::is_enum_v<EnumType>
    constexpr auto ToUnderlying(EnumType enum_type) noexcept
    {
        return static_cast<std::underlying_type_t<EnumType>>(enum_type);
    }

}

#endif /* DF1FDB0F_38F5_46B3_946F_B2FFB9900478 */
