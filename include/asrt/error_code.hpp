#ifndef SODA_ERROR_CODE
#define SODA_ERROR_CODE

#include <iostream>
#include <cstdint>
#include <cerrno>
#include <cstring>
#include <array>
#include <string_view>

#include "asrt/util.hpp"

namespace ErrorCode_Ns{

    using namespace std::literals::string_view_literals;
    static constexpr std::uint8_t kMaxSystemErrnoValue{EHWPOISON};
    static constexpr int kCustomErrorStartOffset{60000u};

    enum class ErrorCode : int
    {
        no_error = 0,

        /****************** System errors ******************/
        /* Permission denied */
        access_denied = EACCES,

        /* Address family not supported by protocol */
        address_family_not_supported = EAFNOSUPPORT,

        /* Address already in use */
        address_in_use = EADDRINUSE,

        /* Transport endpoint is already connected */
        already_connected = EISCONN,

        /* Operation already in progress */
        already_started = EALREADY,

        /* Broken pipe */
        broken_pipe = EPIPE,

        /* A connection has been aborted */
        connection_aborted = ECONNABORTED,

        /* Connection refused */
        connection_refused = ECONNREFUSED,

        /* Connection reset by peer */
        connection_reset = ECONNRESET,

        /* Bad file descriptor */
        bad_descriptor = EBADF,

        /* Bad address */
        fault = EFAULT,

        /* No route to host */
        host_unreachable = EHOSTUNREACH,

        /* Operation now in progress */
        in_progress = EINPROGRESS,

        /* Interrupted system call */
        interrupted = EINTR,

        /* Invalid argument */
        invalid_argument = EINVAL,

        /* Message too long */
        message_size = EMSGSIZE,

        /* The name was too long */
        name_too_long = ENAMETOOLONG,

        /* Network is down */
        network_down = ENETDOWN,

        /* Network dropped connection on reset */
        network_reset = ENETRESET,

        /* Network is unreachable */
        network_unreachable = ENETUNREACH,

        /* Too many open files */
        no_descriptors = EMFILE,

        /* No buffer space available */
        no_buffer_space = ENOBUFS,

        /* Cannot allocate memory */
        no_memory = ENOMEM,

        /* Operation not permitted */
        no_permission = EPERM,

        /* Protocol not available */
        no_protocol_option = ENOPROTOOPT,

        /* No such device */
        no_such_device = ENODEV,

        /* Transport endpoint is not connected */
        not_connected = ENOTCONN,

        /* Socket operation on non-socket */
        not_socket = ENOTSOCK,

        /* Operation cancelled */
        operation_aborted = ECANCELED,

        /* Operation not supported */
        operation_not_supported = EOPNOTSUPP,

        /* Cannot send after transport endpoint shutdown */
        shut_down = ESHUTDOWN,

        /* Connection timed out */
        timed_out = ETIMEDOUT,

        /* Resource temporarily unavailable */
        try_again = EAGAIN,

        /* The socket is marked non-blocking and the requested operation would block */
        would_block = EWOULDBLOCK,

        /****************** custom errors ******************/
        /* trying to open socket when socket is already open */
        socket_already_open = kCustomErrorStartOffset,

        /* trying to bind socket when socket is already bound */
        socket_already_bound,

        /* Protocol mismatch between socket and endpoint */
        protocol_mismatch,

        /* trying to send/recv when no default peer is available */
        no_default_peer,

        operation_cancelled,

        socket_already_has_reactor,

        reactor_not_valid,

        reactor_not_available,

        invalid_reactor_handle,

        socket_in_blocking_mode,

        socket_not_open,

        socket_not_bound,

        socket_state_invalid,

        socket_not_connected,

        socket_already_connected,

        receive_operation_ongoing,

        send_operation_ongoing, 

        listen_operation_ongoing,

        accept_operation_ongoing,

        unable_to_obtain_if_index,

        async_operation_in_progress,

        capacity_exceeded,

        read_insufficient_data,
        
        end_of_file,

        truncation,

        api_error,

        invalid_signal_number,

        reactor_entry_invalid,

        poll_error,

        connection_authentication_failed,

        timer_not_exist,

        default_error,

        max_error
    };

    static constexpr std::size_t kMaxCustomError{(std::size_t)ErrorCode::max_error - kCustomErrorStartOffset + 1u};

    inline constexpr bool IsSystemError(ErrorCode ec)
    {
        return static_cast<unsigned int>(ec) <= kMaxSystemErrnoValue;
    }

    inline constexpr bool IsBusy(ErrorCode error)
    {
        return ((error == ErrorCode::try_again) || (error == ErrorCode::would_block));
    }

    inline constexpr bool IsUnconnected(ErrorCode error)
    {
        return ((error == ErrorCode::not_connected) //system error
            || (error == ErrorCode::socket_not_connected)); //app error
    }

    inline constexpr bool IsConnectionDown(ErrorCode error)
    {
        return (error == ErrorCode::connection_reset) || //system error
            (error == ErrorCode::end_of_file); // app error
    }

    inline constexpr bool IsConnectInProgress(ErrorCode error)
    {
        return ((error == ErrorCode::try_again) || 
            (error == ErrorCode::in_progress));
    }

    inline constexpr bool IsConnectInProgress(int error_number)
    {
        return ((error_number == EAGAIN) || /* Unix domain sockets */
                (error_number == EINPROGRESS)); /* sockets from other families */
    }

    inline constexpr auto FromErrno(int err_number) -> ErrorCode
    {
        assert(err_number <= kMaxSystemErrnoValue);
        return static_cast<ErrorCode>(err_number);
    }

    static constexpr inline const std::string_view kErrorPrintout[] {
    /* socket_already_open */               {"Trying to open socket when socket is already open"sv},
    /* socket_already_bound */              {"Trying to bind socket when socket is already bound"sv},   
    /* protocol_mismatch */                 {"Protocol mismatch between socket and endpoint"sv},
    /* no_default_peer */                   {"Trying to bind socket when socket is already bound"sv},
    /* operation_cancelled */               {"User cancelled async operation"sv},
    /* socket_already_has_reactor */        {"Socket already bound to reactor and may not rebind"sv},
    /* reactor_not_valid */                 {"Reactor not valid"sv},
    /* reactor_not_available */             {"Reactor needed for asynchronous io"sv},
    /* invalid_reactor_handle */            {"Reactor handle invalid"sv},
    /* socket_in_blocking_mode */           {"Socket in blocking mode"sv},
    /* socket_not_open */                   {"Socket not open"sv},
    /* socket_not_bound */                  {"Socket not bound"sv},
    /* socket_state_invalid */              {"Socket state invalid"sv},
    /* socket_not_connected */              {"Trying to send/recv when no peer is available"sv},
    /* socket_already_connected */          {"Connected stream socket may not connect to different peer"sv},
    /* receive_operation_ongoing */         {"Asynchronous receive ongoing"sv},
    /* send_operation_ongoing */            {"Asynchronous send ongoing"sv},    
    /* listen_operation_ongoing */          {"Listen operation is ongoing"sv},
    /* accept_operation_ongoing */          {"Asynchronous accept operation ongoing"sv},
    /* unable_to_obtain_if_index */         {"Unable to obtain ethernet interface index"sv}, 
    /* async_operation_in_progress */       {"Asynchronous operations ongoing"sv},
    /* capacity_exceeded */                 {"storage capacity exceeded"sv},
    /* read_insufficient_data */            {"Received insufficient data"sv},
    /* end_of_file */                       {"End of file reached"sv},
    /* truncation */                        {"Buffer size insufficient"sv},
    /* api_error */                         {"wrong use of API!"sv},
    /* invalid_signal_number */             {"Invalid signal number"sv},
    /* socket_already_bound */              {"Trying to bind socket when socket is already bound"sv},
    /* reactor_entry_invalid */             {"Reactor entry is invalid"sv},
    /* poll_error */                        {"Poll / Epoll reports POLLERR / EPOLLERR event"},
    /* connection_authentication_failed*/   {"Client failed connection handshake validation"},
    /* timer_not_exist */                   {"Timer does not exist!"},
    /* default_error */                     {"Error message not yet implemented :)"sv}
    };

    // static constexpr inline Util::ConstexprMap<ErrorCode, std::string_view, kMaxCustomError> ErrorPrintout{{{
    //     {ErrorCode::socket_already_open, "Trying to open socket when socket is already open"sv},
    //     {ErrorCode::socket_already_bound, "Trying to bind socket when socket is already bound"sv},
    //     {ErrorCode::socket_not_connected, "Trying to send/recv when no peer is available"sv},
    //     {ErrorCode::protocol_mismatch, "Protocol mismatch between socket and endpoint"sv},
    //     {ErrorCode::no_default_peer, "Trying to bind socket when socket is already bound"sv},
    //     {ErrorCode::operation_cancelled, "User cancelled async operation"},
    //     {ErrorCode::socket_already_has_reactor, "Socket already bound to reactor and may not rebind"sv},
    //     {ErrorCode::socket_in_blocking_mode, "Socket in blocking mode"sv},
    //     {ErrorCode::reactor_not_available, "Reactor needed for asynchronous io"sv},
    //     {ErrorCode::async_operation_in_progress, "Asynchronous operations ongoing"sv},
    //     {ErrorCode::receive_operation_ongoing, "Asynchronous receive ongoing"sv},
    //     {ErrorCode::capacity_exceeded, "storage capacity exceeded"sv},
    //     {ErrorCode::truncation, "Buffer size insufficient"sv},
    //     {ErrorCode::api_error, "wrong use of API!"sv},
    //     {ErrorCode::end_of_file, "End of file reached"sv},
    //     {ErrorCode::socket_already_bound, "Trying to bind socket when socket is already bound"sv},
    //     {ErrorCode::socket_already_open, "Trying to bind socket when socket is already bound"sv},
    //     {ErrorCode::unable_to_obtain_if_index, "Ethernet interface unknown"sv},
    //     {ErrorCode::default_error, "Error message not yet implemented"sv}
    // }}};

    constexpr inline auto ToStringView(ErrorCode ec) -> std::string_view
    { 
        return IsSystemError(ec) ?
            ::strerror(static_cast<unsigned int>(ec)) :
            kErrorPrintout[static_cast<unsigned int>(ec) - kCustomErrorStartOffset];
    }

    //constexpr std::string_view errrr{ToStringView(ErrorCode::default_error)};

    inline auto MapLatestSysError() -> ErrorCode
    {
        return static_cast<ErrorCode>(errno);
    }

    inline auto MapAndLogSysError(const char *syscall) -> ErrorCode
    {
        ASRT_LOG_DEBUG("[Syscall]: {} failed with {}", syscall, ::strerror(errno));
        return MapLatestSysError();
    }

    inline std::ostream& operator<<(std::ostream& os, const ErrorCode ec)
    {
        os << ToStringView(ec);
        return os;
    }


    template <class ErrorDomainDerived>
    class ErrorDomain
    {
        using Printable = const char*;

        public:

        auto Name() -> Printable;

        auto Message() -> Printable;
    };
}

#endif //SODA_ERROR_CODE