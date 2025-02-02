#include "execution/utilities/utils.hpp"
#include "execution/details/receiver.hpp"

namespace asrt{
namespace execution{
namespace details{

struct receiver_ebo { };

struct null_receivcer : receiver_ebo {
    using receiver_concept = execution::receiver_t;
    void set_error(auto) noexcept {};
    void set_stopped() noexcept {};
};

template <typename Derived, typename WrappedReceiver = details::null_receivcer>
struct receiver_wrapper : execution::receiver_t {

    [[no_unique_address]] WrappedReceiver wrapped_receiver_;

    constexpr receiver_wrapper() = default;

    template <class R>
    explicit constexpr receiver_wrapper(R&& wrapped) 
        noexcept(std::is_nothrow_constructible_v<WrappedReceiver, R>)
            : wrapped_receiver_{static_cast<R&&>(wrapped)} {}

    template <typename... Args, typename Self = Derived>
    void set_value(Args&&... args) && noexcept {
        return execution::set_value(
            wrapped(static_cast<Self&&>(*this)), 
            static_cast<Args&&>(args)...);
    }

    template <typename Error, typename Self = Derived>
    void set_error(Error&& err) && noexcept {
        return execution::set_error(
            wrapped(static_cast<Self&&>(*this)), 
            static_cast<Error&&>(err));
    }

    template <typename Self = Derived>
    void set_stopped() && noexcept {
        return execution::set_stopped(
            wrapped(static_cast<Self&&>(*this)));
    }

protected:

    template <typename Self>
    static auto wrapped(Self&& self) noexcept -> decltype(auto) {
        return execution::forward_like<Self>(self.wrapped_receiver_);
    }    
};

} // namespace details
} // namespace execution
} // namespace asrt

