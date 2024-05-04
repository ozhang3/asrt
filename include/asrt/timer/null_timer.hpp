#ifndef DDAE876F_1614_485E_80AE_509E72F491CF
#define DDAE876F_1614_485E_80AE_509E72F491CF

#include "asrt/executor/executor_task.hpp"

namespace Timer{

struct NullTimer : public ExecutorNS::TimerService<NullTimer>
{
  constexpr NullTimer() noexcept = default;
};

}

#endif /* DDAE876F_1614_485E_80AE_509E72F491CF */
