#ifndef ASRT_EXECUTION_DETAILS_SENDER_HPP_
#define ASRT_EXECUTION_DETAILS_SENDER_HPP_

#include <tuple>
#include <type_traits>
#include "execution/utilities/config.hpp"
#include "execution/details/receiver.hpp"

namespace asrt{
namespace execution{
namespace details {

template <typename Tag, typename Data, typename... Children>
struct basic_sender;

struct sender_impl_defaults {
    template <typename Sender, typename Receiver>
    static auto connect(Sender&& sndr, Receiver&& rcvr) {
        return typename sender_impl<Tag>::template operation_state<Sender, Receiver>{
            static_cast<Sender&&>(sndr), static_cast<Receiver&&>(rcvr)};
    }

    // Default completion signatures
    template <typename Sender, typename Env>
    static auto get_completion_signatures(const Sender&, const Env&) {
        static_assert(false, "No get_completion_signatures specilization found for this sender");
    }
    
    static auto start(auto& op) { op.start(); }
};

//-----------------------------------------------------------------------------
// sender_impl: Customization point for sender operations
//-----------------------------------------------------------------------------
template <typename Tag>
struct sender_impl : details::sender_impl_defaults{
    typedef void not_specilized;
};

//-----------------------------------------------------------------------------
// basic_sender: Core sender implementation
//-----------------------------------------------------------------------------
template <typename Tag, typename Data = std::tuple<>, typename... Children>
struct basic_sender {
    // Type identifiers
    using sender_concept = sender_t;
    using tag = Tag;
    using data_type = Data;
    using children_type = std::tuple<Children...>;

    // Storage
    [[no_unique_address]] data_type data_;
    [[no_unique_address]] children_type children_;

    ASRT_EXEC_ATTR((always_inline)) explicit basic_sender(
        Tag, Data&& data = std::tuple<>{}, Children&&... children) noexcept
            : data_{static_cast<Data&&>(data)}, 
              children_(static_cast<Children&&>(children)...) {}

    // Environment propagation
    ASRT_EXEC_ATTR(always_inline) friend auto get_env(const basic_sender& sndr) {
        return sender_impl<Tag>::get_env(sndr);
    }

    // Connection point
    template <typename Self, typename Receiver>
    ASRT_EXEC_ATTR(always_inline) static auto connect(Self&& self, Receiver&& rcvr) {
        return sender_impl<Tag>::connect(
            static_cast<Self&&>(self), 
            static_cast<Receiver&&>(rcvr));
    }

    // Apply function to children (for composition)
    template <typename Fn>
    ASRT_EXEC_ATTR(always_inline) auto apply(Fn&& fn) const {
        return std::apply(static_cast<Fn&&>(fn), children);
    }

    // Completion signatures
    template <typename Env>
    ASRT_EXEC_ATTR(always_inline) auto get_completion_signatures(const Env& env) const {
        return sender_impl<Tag>::get_completion_signatures(*this, env);
    }
};

struct connect_t {

    template <typename S, typename R>
    struct has_static_member_connect {
        template <typename Snd, typename Rcv>
        static constexpr auto test(int) -> decltype(
            Snd::connect(std::declval<Snd>(), std::declval<Rcv>()),
            std::true_type{});
        
        template <typename, typename>
        static constexpr std::false_type test(...);
        
        static constexpr bool value = decltype(test<S, R>(0))::value;
    };

    template <typename S, typename R>
    struct has_member_connect {
        template <typename Snd, typename Rcv>
        static constexpr auto test(int) -> decltype(
            std::declval<Snd>().connect(std::declval<Rcv>()),
            std::true_type{});
        
        template <typename, typename>
        static constexpr std::false_type test(...);
        
        static constexpr bool value = decltype(test<S, R>(0))::value;
    };

    template <typename S, typename R>
    struct has_tag_invoke {
        template <typename Snd, typename Rcv>
        static constexpr auto test(int) -> decltype(
            tag_invoke(std::declval<connect_t>(), std::declval<Snd>(), std::declval<Rcv>()),
            std::true_type{});
        
        template <typename, typename>
        static constexpr std::false_type test(...);
        
        static constexpr bool value = decltype(test<S, R>(0))::value;
    };

    template <typename Sender, typename Receiver>
    auto operator()(Sender&& sender, Receiver&& receiver) const noexcept
    {
        if constexpr (has_static_member_connect<Sender, Receiver>::value) {
            return Sender::connect(static_cast<Sender&&>(sender), static_cast<Receiver&&>(receiver));
        } else if constexpr (has_member_connect<Sender, Receiver>::value) {
            return static_cast<Sender&&>(sender).connect(static_cast<Receiver&&>(receiver));
        } else {
            return tag_invoke(*this, static_cast<Sender&&>(sender), static_cast<Receiver&&>(receiver));
        }
    }
};
} // end ns details

inline constexpr details::connect_t connect;

struct sender_t { typedef sender_t sender_concept; };

#if ASRT_EXECUTION_HAS_CONCEPTS
    template <typename Sender>
    concept sender =
        std::is_base_of_v<execution::sender_t, typename Sender::sender_concept> ||\
        requires { typename Sender::is_sender; };

    template <typename Sender, typename Receiver>
    concept sender_to =
        receiver<Receiver> &&
        requires (Sender&& s, Receiver&& r) {
            execution::connect(static_cast<Sender&&>(s), static_cast<Receiver&&>(r));
        };
#endif

} // namespace execution
} // namespace asrt

#endif // ASRT_EXECUTION_DETAILS_SENDER_HPP_