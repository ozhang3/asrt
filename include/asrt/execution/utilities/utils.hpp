
#include <type_traits>
#include <utility>

namespace asrt{
namespace execution{

#ifdef __cpp_lib_forward_like
    using std::forward_like;
#else

    template <typename T, typename U>
    constexpr auto forward_like(U&& u) noexcept ->
        std::conditional_t<
            std::is_lvalue_reference_v<T&&>,
            std::remove_reference_t<U>&,
            std::remove_reference_t<U>&&> {
        return static_cast<
            std::conditional_t<
                std::is_lvalue_reference_v<T&&>,
                std::remove_reference_t<U>&,
                std::remove_reference_t<U>&&>>
            (u);
    }
#endif

// upcast to base, preserving value categories
template <typename Base, typename Self>
constexpr auto up_cast(Self&&self) noexcept -> decltype(auto) {
    using base_ref_t = std::conditional_t<
        std::is_lvalue_reference_v<Self>, 
        Base&,                        
        Base&&>;
    return static_cast<base_ref_t<Base, Self>>(
        static_cast<Self&&>(self));
}

} // namespace execution
} // namespace asrt