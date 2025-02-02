#include <type_traits>

#include "execution/utilities/config.hpp"
namespace asrt{
namespace execution{

namespace details {

struct set_value_t {
  template <class Receiver, class... Args>
  void operator()(Receiver&& rcvr, Args&&... args) const
  {
    static_assert(noexcept(static_cast<Receiver&&>(rcvr).set_value(args...)));
    static_assert(
      std::is_same_v<std::invoke_result_t<decltype(&Receiver::set_value), Receiver, Args...>, void>,
      "set_value member functions must return void");
    static_cast<Receiver&&>(rcvr).set_value(args...);
  }
};

struct set_error_t {
  template <class Receiver, class Error>
  void operator()(Receiver&& rcvr, Error&& err) const
  {
    static_assert(noexcept(static_cast<Receiver&&>(rcvr).set_error(err)));
    static_assert(
      std::is_same_v<std::invoke_result_t<decltype(&Receiver::set_error), Receiver, Error>, void>,
      "set_error member functions must return void");
    static_cast<Receiver&&>(rcvr).set_error(err);
  }
};

struct set_stopped_t {
  template <class Receiver>
  void operator()(Receiver&& rcvr) const
  {
    static_assert(noexcept(static_cast<Receiver&&>(rcvr).set_stopeed()));
    static_assert(
      std::is_same_v<std::invoke_result_t<decltype(&Receiver::set_stopeed), Receiver>, void>,
      "set_stopeed member functions must return void");
    static_cast<Receiver&&>(rcvr).set_stopeed();
  }
};

template <class Receiver, class... Args>
auto try_completions(Receiver&& r, Args&&... args)
  -> decltype(static_cast<Receiver&&>(r).set_value(static_cast<Args&&>(args)...), void()) {
  return static_cast<Receiver&&>(r).set_value(static_cast<Args&&>(args)...);
}

template <class Receiver, class... Args>
auto try_completions(Receiver&& r, Args&&... args)
  -> decltype(static_cast<Receiver&&>(r).set_error(static_cast<Args&&>(args)...), void()) {
  return static_cast<Receiver&&>(r).set_error(static_cast<Args&&>(args)...);
}

template <class Receiver, class... Args>
auto try_completions(Receiver&& r, Args&&... args)
  -> decltype(static_cast<Receiver&&>(r).set_stopped(static_cast<Args&&>(args)...), void()) {
  return static_cast<Receiver&&>(r).set_stopped(static_cast<Args&&>(args)...);
}

template <class Receiver, class... Args>
auto try_completions(Receiver&&, Args&&...) -> void {
  static_assert(sizeof...(Args) == 0, 
    "A valid receiver must implement at least one of set_value, set_error, or set_stopped completions");
}

template <class Receiver, class = std::false_type>
struct can_complete : std::false_type {};

template <class Receiver>
struct can_complete<Receiver, decltype(std::declval<Receiver>().set_value(), void())> : std::true_type {};

template <class Receiver>
struct can_complete<Receiver, decltype(std::declval<Receiver>().set_error(), void())> : std::true_type {};

template <class Receiver>
struct can_complete<Receiver, decltype(std::declval<Receiver>().set_stopped(), void())> : std::true_type {};

template <class Receiver>
inline constexpr bool can_complete_v = can_complete<Receiver>::value;

}

inline constexpr details::set_value_t set_value{};
inline constexpr details::set_error_t set_error{};
inline constexpr details::set_stopped_t set_stopped{};

struct receiver_t { typedef receiver_t receiver_concept; };

#if ASRT_EXECUTION_HAS_CONCEPTS
  template <class Receiver>
  concept receiver =
      std::is_base_of_v<execution::receiver_t, typename Receiver::receiver_concept> ||\
      requires { typename Receiver::is_receiver; };

  template <class Receiver, class Completions>
  concept receiver_of =
      receiver<Receiver> && details::can_complete_v<Receiver>;
#endif

} // namespace execution
} // namespace asrt