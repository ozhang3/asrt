#include <type_traits>
#include "execution/utilities/config.hpp"

namespace asrt{
namespace execution{

struct operation_state_t {};

namespace details {

struct start_t{
    template <class Op>
    ASRT_EXEC_ATTR(always_inline) 
    void operator()(Op& op) const noexcept 
    {
        static_assert(std::is_same_v<decltype(op.start()), void>,
            "start() must return void");
        op.start();
    }
};

} // namespace details

inline constexpr details::start_t start{};

#if ASRT_EXECUTION_HAS_CONCEPTS
  template <class Op>
  concept operation_state =
    std::is_destructible_v<Op> &&\
    std::is_object_v<Op> &&\
    requires(Op& op) {
      execution::start(op);
    };
#endif

} // namespace execution
} // namespace asrt