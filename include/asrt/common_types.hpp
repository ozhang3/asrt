#ifndef D939C086_0864_462D_B7DF_BB870928FC7A
#define D939C086_0864_462D_B7DF_BB870928FC7A

#ifdef __cpp_lib_move_only_function	
    #include <functional>
#else
    #include <std23/function_ref.h>
    #include <std23/move_only_function.h>
#endif

#include <expected.hpp>
#include "asrt/error_code.hpp"

namespace asrt
{
    using NativeHandle = int;
    static constexpr NativeHandle kInvalidNativeHandle{-1};

    constexpr inline bool IsFdValid(NativeHandle handle)
    {
        return handle != kInvalidNativeHandle;
    }

#ifdef __cpp_lib_move_only_function	
    template <class R, class... Args>
    using UniqueFunction = std::move_only_function<R, Args...>;

    template <class R, class... Args>
    using FunctionRef = std::function_ref<R, Args...>;
#else
    template <class R, class... Args>
    using UniqueFunction = std23::move_only_function<R, Args...>;

    template <class R, class... Args>
    using FunctionRef = std23::function_ref<R, Args...>;
#endif

    using ErrorCodeType = ErrorCode_Ns::ErrorCode;
    template <typename T> using Result = tl::expected<T, ErrorCodeType>;
    
}

#endif /* D939C086_0864_462D_B7DF_BB870928FC7A */
