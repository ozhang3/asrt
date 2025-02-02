#include <utility>
#include "execution/utilities/config.hpp"
#include "execution/details/sender.hpp"

namespace asrt{
namespace execution{
namespace details{
    
template <typename JustTag>
struct just_impl : details::sender_impl_defaults {

    using completion_t = typename JustTag::completion_tag;

    template <class Sender, class Receiver>
    struct operation_state {
        typename Sender::data_type data;
        Receiver receiver;

        static constexpr void start() & {
            std::apply([this](auto&&... args) {
                completion_t{}(
                    std::move(receiver), 
                    std::forward<decltype(args)>(args)...);
            }, data);
        }
    };
};

struct just_t {
    using completion_tag = set_value_t;

    template <typename... Ts>
    auto operator()(Ts&&... ts) const {
        return basic_sender{just_t{}, 
            std::tuple{static_cast<Ts&&>(ts)...}};
    }
};

struct just_error_t {
    using completion_tag = set_error_t;

    template <typename Error>
    auto operator()(Error&& err) const {
        return basic_sender{just_error_t{}, 
            static_cast<Error&&>(err)};
    }
};

struct just_stopped_t {
    using completion_tag = set_stopped_t;

    auto operator()() const noexcept {
        return basic_sender{just_stopped_t{}};
    }
};

// template <> struct just_impl<just_t>;
// template <> struct just_impl<just_error_t>;
// template <> struct just_impl<just_stopped_t>;

template <> struct sender_impl<just_t> : just_impl<just_t> {};
template <> struct sender_impl<just_error_t> : just_impl<just_error_t> {};
template <> struct sender_impl<just_stopped_t> : just_impl<just_stopped_t> {};

} // namespace details

inline constexpr details::just_t just{};
inline constexpr details::just_error_t just_error{};
inline constexpr details::just_stopped_t just_stopped{};

} // namespace execution
} // namespace asrt