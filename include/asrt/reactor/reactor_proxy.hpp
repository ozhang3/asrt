#ifndef C616E9B0_1D01_451E_AAE1_D67EF4285777
#define C616E9B0_1D01_451E_AAE1_D67EF4285777

#include <functional>
#include "asrt/reactor/types.hpp"

namespace ReactorNS{


using ReactorNS::Types::HandlerTag;

template <typename Reactor>
    requires requires (Reactor& r) {r.Register(); r.Trigger();}
class NonOwningReactorProxy {

public:

    NonOwningReactorProxy() = default;


    NonOwningReactorProxy(Reactor& reactor)
    {
        reactor.Register();


    }


private:

    HandlerTag reactor_handle_;
};

template <typename Reactor>
class OwningReactorProxy {

public:

    OwningReactorProxy() = default;


    OwningReactorProxy(Reactor& reactor)
    {
        reactor.Register();


    }

private:

    void OnReactorEvent() noexcept
    {

    }

private:

    std::function<void()> handler_;
    HandlerTag reactor_handle_;
};

}

#endif /* C616E9B0_1D01_451E_AAE1_D67EF4285777 */
