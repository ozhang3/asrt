#ifndef E3B889E7_19D1_4B9E_A316_46EDA641E7A9
#define E3B889E7_19D1_4B9E_A316_46EDA641E7A9

#include <cstdint>
#include <new>
#include <atomic>

namespace asrt{

namespace details{

#ifdef __cpp_lib_hardware_interference_size
    static constexpr auto kCacheAlign{std::hardware_destructive_interference_size};
#else
    static constexpr std::size_t kCacheAlign{64u};
#endif

}

class RingBufferView{

public:
    RingBufferView() noexcept = default;


private:

    

    alignas(details::kCacheAlign) std::atomic<std::uint32_t> head_index_;
    alignas(details::kCacheAlign) std::atomic<std::uint32_t> tail_index_;
};


}

#endif /* E3B889E7_19D1_4B9E_A316_46EDA641E7A9 */
