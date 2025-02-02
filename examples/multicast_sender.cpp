#include <span>
#include <spdlog/spdlog.h>
#include <asrt/socket/basic_datagram_socket.hpp>
#include <asrt/ip/udp.hpp>
#include <asrt/timer/steady_timer.hpp>
#include <asrt/executor/executor_work_guard.hpp>
//#include <asrt/config.hpp>

using namespace asrt::ip;
using asrt::Result;
using Timer::SteadyTimer;
using Timer::SteadyPeriodicTimer;
using namespace std::chrono_literals;

// udp::Executor executor;

// SteadyTimer timer{executor, 2s};

// constexpr auto a = sizeof(timer);

void set_timer(SteadyTimer& timer)
{
    timer.WaitAsync(2s, [&timer](){ 
        spdlog::info("timer expired, rearming...");
        //timer.ExpiresAfter(1s);
        set_timer(timer);
        //executor.Stop();
    });
}

int main() {

    spdlog::set_level(spdlog::level::trace);

    udp::executor executor;

    SteadyTimer timer{executor};

    SteadyPeriodicTimer timerp{executor, 2s};

    set_timer(timer);

    timerp.WaitAsync([&timerp]() mutable { 
        std::chrono::milliseconds const init_period{200};
        static int counter{};
        spdlog::info("Periodic timer expired");
        timerp.SetPeriod(init_period * ++counter);
    });

    executor.Run();

    // udp::Socket socket{};

    // udp::Endpoint endpoint{"225.1.2.3", 50000u};

    // socket.Open();

    // std::uint8_t const buffer[]{0x12, 0x34, 0x56, 0x78};
    // std::span<std::uint8_t const> buffer_view{buffer};
    
    // for(;;) {
    //     Result<std::size_t> const send_result{
    //         socket.SendToSync(endpoint, buffer_view)};
    //     if(send_result){
    //         spdlog::info("Sent {} bytes", send_result.value());
    //     }else{
    //         spdlog::error("Send error {}", send_result.error());
    //     }
    //     sleep(2);
    // }

}