#ifndef A94AE6FE_4EB4_4BCC_884F_3F5C2A71FD9B
#define A94AE6FE_4EB4_4BCC_884F_3F5C2A71FD9B

#include "asrt/executor/executor_task.hpp"

namespace ReactorNS{

struct NullReactor : public ExecutorNS::ReactorService<NullReactor>
{
  constexpr NullReactor() noexcept = default;
};

}
#endif /* A94AE6FE_4EB4_4BCC_884F_3F5C2A71FD9B */
