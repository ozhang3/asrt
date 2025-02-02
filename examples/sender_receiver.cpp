#include <iostream>
#include <asrt/execution/execution.hpp>

using namespace asrt::execution;

struct my_receiver {
    void set_value(auto x) { std::cout << x << std::endl; }
};

int main()
{
    auto snd1 = just(42);

    auto snd2 = then(snd1, [](int x) { return x * 2; });

    auto op = connect(snd2, my_receiver{});

    op.start();
}