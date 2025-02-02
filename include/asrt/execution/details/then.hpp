#include <utility>
#include "execution/utilities/config.hpp"
#include "execution/details/sender.hpp"
#include "execution/details/receiver_wrapper.hpp"

namespace asrt{
namespace execution{
namespace details{

template <typename R, typename F>
struct then_receiver : receiver_wrapper<then_receiver<WrappedReceiver>, WrappedReceiver> {

    using base = receiver_wrapper<then_receiver<WrappedReceiver>, WrappedReceiver>;

    explicit then_receiver(R&& receiver, F&& f)
        : base{static_cast<R&&>(receiver)}, 
          continuation_{static_cast<F&&>(f)} {}

    template <typename... Args>
    void set_value(Args&&... args) && noexcept {
#if ASRT_EXECUTION_HAS_EXCEPTIONS
        try {
#endif
            execution::set_value(
                base::wrapped(std::move(*this)), 
                std::invoke(static_cast<F&&>(continuation_), 
                            static_cast<Args&&>(args)...));
#if ASRT_EXECUTION_HAS_EXCEPTIONS
        } catch (...) {
            execution::set_error(
                base::wrapped(std::move(*this)), 
                std::current_exception());
        }
#endif
    }

private:
    F continuation_;
        
};

template <typename Sender, typename F>
struct then_sender {

    using sender_concept = execution::sender_t;

    Sender wrapped_sender_;
    F continuation_;

    template <typename Receiver>
    auto connect(Receiver&& rcvr) {
        return execution::connect(
            static_cast<Sender&&>(wrapped_sender_), 
            then_receiver<Receiver, F>{
                static_cast<Receiver&&>(rcvr), 
                static_cast<F&&>(continuation_)});
    }
};

struct then_t {
    template <typename Sender, typename F>
    auto operator()(Sender&& sender, F&& f) const {
        return then_sender<Sender, F>{
            static_cast<Sender&&>(sender), 
            static_cast<F&&>(f)};
    }
};

} // namespace details

inline constexpr details::then_t then{};

} // namespace execution
} // namespace asrt