#ifndef EA948689_91CC_4152_A52D_A6E88E261695
#define EA948689_91CC_4152_A52D_A6E88E261695

#include <memory>

#include "asrt/util.hpp"
#include "asrt/error_code.hpp"

namespace ExecutorNS{

using namespace Util::Expected_NS;

template <class ExecutorImpl>
class ExecutorInterface
{
public:

private:
  constexpr ExecutorImpl& Implementation() { return static_cast<ExecutorImpl &>(*this); }
};

}




#endif /* EA948689_91CC_4152_A52D_A6E88E261695 */
