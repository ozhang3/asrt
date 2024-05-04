#ifndef ASRT_SYSCALL_HPP_
#define ASRT_SYSCALL_HPP_

#include "asrt/config.hpp"
#include <cstdint>
#include <csignal>
#include <sys/signalfd.h> //todo
#include <string>
#include <sys/socket.h>
#if ASRT_HAS_EVENTFD
#   include <sys/eventfd.h>
#endif
#include <iostream>
#include <concepts>
#include <stop_token>
//#include <string_view>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#if ASRT_HAS_EPOLL
#   include <sys/epoll.h>
#endif
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_ether.h>
#include <sys/timerfd.h>
#include <ctime> //struct itimerspec

#include <linux/if_packet.h>

#include <sys/mman.h>

#include "asrt/socket/types.hpp"
#include "asrt/error_code.hpp"
#include "asrt/util.hpp"
#include "asrt/type_traits.hpp"

/* Evaluate EXPRESSION, and repeat as long as it returns -1 with `errno'
   set to EINTR.  */
#ifndef TEMP_FAILURE_RETRY
# define TEMP_FAILURE_RETRY(expression)\
  (({ long int result__;\
       do result__ = (long int) (expression);\
       while (result__ == -1L && errno == EINTR);\
       result__; }))
#endif

namespace OsAbstraction{

using namespace ErrorCode_Ns;
using ErrorCode = ErrorCode_Ns::ErrorCode;
using Socket::Types::NativeSocketHandleType;
using namespace Util::Expected_NS;

template <typename T> using Result = Expected<T, ErrorCode>;

namespace details{

    template <typename SyscallRetval>
    constexpr inline auto HasSystemCallFailed(const SyscallRetval retval){
        static_assert(std::is_same<typename std::remove_cv<SyscallRetval>::type, int>::value ||
                    std::is_same<typename std::remove_cv<SyscallRetval>::type, ssize_t>::value,
                "HasSystemCallFailed() only accepts types int or ssize_t as parameter.");

        return retval < 0;     
    }
}




/**
 * @brief Creates a system socket in given protocol
 * 
 * @tparam Protocol A valid socket protocol, eg: UnixStream etc.
 * @param proto 
 * @param flags 
 * @return Result<NativeSocketHandleType> 
 */
template<typename Protocol>
inline auto Socket(const Protocol& proto, int flags = SOCK_CLOEXEC) noexcept -> Result<NativeSocketHandleType>
{
    static_assert(ProtocolTraits::is_valid<Protocol>::value, "Invalid protocol type passed to socket()");

    int const sockfd = ::socket(proto.Family(), 
                           proto.GetType() | static_cast<int>(flags), 
                           proto.ProtoNumber());
    if(sockfd == -1) [[unlikely]]
    {
        return MakeUnexpected(MapAndLogSysError("::socket()"));
    }
    else
    {
        ASRT_LOG_TRACE("Open socket sucess, socket fd {}", sockfd);
        return sockfd;
    }
}

/**
 * @brief 
 * 
 * @param fd 
 * @return Result<void> 
 */
inline auto Close(int fd) noexcept -> Result<void>
{
    ASRT_LOG_TRACE("Closing file descriptor {}", fd);

    /* Man close():
        "Retrying the close() after a failure return is the wrong thing to
        do, since this may cause a reused file descriptor from another
        thread to be closed. Regarding the EINTR error ... Linux and many other
        implementations, where, as with other errors that may be reported
        by close(), the file descriptor is guaranteed to be closed." */

    /* Here we rely on the linux implementation where the fd is closed even when
        the close() call is interrupted. Therefore we do not retry on EINTR */

    return ::close(fd) == -1 ?
        MakeUnexpected(MapAndLogSysError("::close()")) :
        Result<void>{};
}

inline auto Unlink(const char* path) noexcept -> Result<void>
{
    ASRT_LOG_TRACE("{}()", __func__);

    if(::unlink(path) == -1) [[unlikely]]
    {
        return MakeUnexpected(MapAndLogSysError("::unlink()"));
    }

    return {};
}

inline auto Unlink(const std::string& path) noexcept -> Result<void>
{
    ASRT_LOG_TRACE("{}()", __func__);

    if(::unlink(path.c_str()) == -1) [[unlikely]]
    {
        return MakeUnexpected(MapAndLogSysError("::unlink()"));
    }

    return {};
}

/*!
   * \brief Disables sends and/or receives on the socket.
   * \details This function is used to disable send operations, receive operations, or both.
   * \param[in] sockfd  file desciptor of the socket on which the operation is performed
   * \param[in] how what operations to disable:
   *                SHUT_RD   = No more receptions;
   *                SHUT_WR   = No more transmissions;
   *                SHUT_RDWR = No more receptions or transmissions.
   * \return whether the operation was successful
*/
inline auto Shutdown(int sockfd, int how) noexcept -> Result<void>
{
    //std::cout << "[Syscall] Disabling socket operation " << how << " for " << sockfd << "\n";
    ASRT_LOG_TRACE("{}()", __func__);

    if(::shutdown(sockfd, how) == -1) [[unlikely]]
    {
        return MakeUnexpected(MapAndLogSysError("::shutdown()"));
    }

    return {};
}

/**
 * @brief Get index number for given network interface 
 * 
 * @param if_name name of interface in const char *
 * @param sockfd an open socket fd used to perform ioctl; if none provided, a socket
 * 
 * @note: 
 * @return Result<int> 
 */
[[nodiscard]] inline auto 
GetNetIfIndex(const char* if_name, int sockfd = 0) noexcept -> Result<int>
{
    ASRT_LOG_TRACE("{}()", __func__);

    int fd{sockfd};
    ::ifreq ifr;
    std::size_t const if_name_len{strlen(if_name)};

    if(if_name_len < IFNAMSIZ) [[likely]] {
        memcpy(ifr.ifr_name, if_name, if_name_len);
        ifr.ifr_name[if_name_len] = 0; //null terminate
    } else [[unlikely]] {
        return MakeUnexpected(ErrorCode::truncation);
    }

    if(!sockfd){
        if(fd = ::socket(AF_UNIX, SOCK_DGRAM, 0); !fd) [[unlikely]] {
            return MakeUnexpected(MapAndLogSysError("::socket()"));
        }
    }

    if (::ioctl(fd, SIOCGIFINDEX, &ifr) == -1) [[unlikely]] {
        return MakeUnexpected(MapAndLogSysError("::ioctl()"));
    }

    if(!sockfd){
        if(::close(fd) == -1) [[unlikely]]{
            LogFatalAndAbort("Failed to close socket after ioctl");
        }
    }
    
    return ifr.ifr_ifindex;
}

/**
 * @brief Get MAC address given interface name
 * 
 * @param if_name [in]
 * @param addr_buffer [out]
 * @param sockfd [in] optional open socket fd to be used in obtaining address
 * @return Result<void> 
 */
inline auto 
GetIfHwAddr(const char* if_name, unsigned char* addr_buffer, int sockfd = 0) noexcept -> Result<void>
{
    ASRT_LOG_TRACE("{}()", __func__);

    ::ifreq ifr;
    std::size_t const if_name_len{::strlen(if_name)};

    if(if_name_len < IFNAMSIZ) [[likely]] {
        memcpy(ifr.ifr_name, if_name, if_name_len);
        ifr.ifr_name[if_name_len] = 0; //null terminate
    } else [[unlikely]] {
        return MakeUnexpected(ErrorCode::truncation);
    }

    if(!(sockfd = ::socket(AF_UNIX, SOCK_DGRAM, 0))) [[unlikely]] {
        return MakeUnexpected(MapAndLogSysError("::socket()"));
    }

    if (::ioctl(sockfd, SIOCGIFHWADDR, &ifr) == -1) [[unlikely]] {
        return MakeUnexpected(MapAndLogSysError("::ioctl()"));
    }

    if(!sockfd){
        if(::close(sockfd) == -1) [[unlikely]]{
            LogFatalAndAbort("Failed to close socket after ioctl");
        }
    }
    
    std::memcpy(addr_buffer, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

    return Result<void>{};
}


[[nodiscard]] inline auto
GetIfStat(const char * if_name, const char * stat_name) noexcept -> Result<long long int>
{
	char stat_path[PATH_MAX];
	std::snprintf(stat_path, sizeof(stat_path), 
        "/sys/class/net/%s/statistics/%s", if_name, stat_name);

	int const fd{open(stat_path, O_RDONLY)};
	if (fd == -1) [[unlikely]]
		return MakeUnexpected(MapAndLogSysError("::open()"));

	::ssize_t const bytes_read{
        ::read(fd, stat_path, sizeof(stat_path) - 1)};

	if(::close(fd) == -1) [[unlikely]]
        return MakeUnexpected(MapAndLogSysError("::close()"));

	if (bytes_read == -1) [[unlikely]]
		return MakeUnexpected(MapAndLogSysError("::read()"));

	stat_path[bytes_read] = '\0';
	return std::strtoll(stat_path, NULL, 10);
}

#if 0
/**
 *  @brief binds the socket referred to by the
       file descriptor sockfd to the address specified by addr
    @param sockfd socket file descriptor obtained from socket()
    @param addr any socket address that is castable to const sockaddr *
*/
template<typename SocketAddressType>
inline auto Bind(int sockfd, const SocketAddressType& addr) noexcept -> Result<void>
{
    static_assert(SocketAddressTraits::is_valid<SocketAddressType>::value, "Invalid socket adddress passed to Bind()");
    std::cout << "[Syscall] Binding sockfd: " << sockfd;// << " to addr: " << addr << "\n";

    if(::bind(sockfd, reinterpret_cast<const struct ::sockaddr *>(&addr), sizeof(addr)) == -1) [[unlikely]]
    {
        return MakeUnexpected(MapAndLogSysError("::bind()"));
    }

    return {};
}
#endif

/**
 *  @brief binds the socket referred to by the
       file descriptor sockfd to the address specified by addrview
    @param sockfd socket file descriptor obtained from socket()
    @param addrview a view to a valid socket address
*/
template<typename SockAddrView>
inline auto Bind(int sockfd, SockAddrView addrview) noexcept -> Result<void>
{
    //static_assert(SocketAddressViewTraits::is_valid<SockAddrView>::value, "Invalid socket adddress view passed to Bind()");
    ASRT_LOG_TRACE("Binding sockfd {}", sockfd);

    if(::bind(sockfd, reinterpret_cast<const ::sockaddr *>(addrview.data()), addrview.size()) == -1) [[unlikely]]
    {
        return MakeUnexpected(MapAndLogSysError("::bind()"));
    }

    return {};
}

/**  
    @brief connects the socket referred to by the file descriptor sockfd to the address specified by addr
    @param sockfd socket file descriptor obtained from socket()
    @param addr any socket address that is castable to const sockaddr *
    @details

            Some protocol sockets (e.g., UNIX domain stream sockets) may
       successfully connect() only once.

            Some protocol sockets (e.g., datagram sockets in the UNIX and
       Internet domains) may use connect() multiple times to change
       their association.

            If connect() fails, consider the state of the socket as
        unspecified.  Portable applications should close the socket and
        create a new one for reconnecting.
*/
template<typename SockAddrView>
inline auto Connect(int sockfd, SockAddrView addrview) noexcept -> Result<void>
{
    //static_assert(SocketAddressViewTraits::is_valid<SockAddrView>::value, "Invalid socket adddress view passed to connect()");
    ASRT_LOG_TRACE("Connecting sockfd {}", sockfd);

    if(::connect(sockfd, reinterpret_cast<const ::sockaddr *>(addrview.data()), addrview.size()) == -1) [[unlikely]]
    {
        return MakeUnexpected(MapAndLogSysError("::connect()"));
    }

    return {};
}

/*  @brief marks socket as passive, ie: ready for accepting connections
    @param sockfd socket file descriptor obtained from socket()
    @param num_connections the number of connections that may be accept()ed
    @error 
        EADDRINUSE
              Another socket is already listening on the same port.

       EADDRINUSE
              (Internet domain sockets) The socket referred to by sockfd
              had not previously been bound to an address and, upon
              attempting to bind it to an ephemeral port, it was
              determined that all port numbers in the ephemeral port
              range are currently in use.  See the discussion of
              /proc/sys/net/ipv4/ip_local_port_range in ip(7).

       EBADF  The argument sockfd is not a valid file descriptor.

       ENOTSOCK
              The file descriptor sockfd does not refer to a socket.

       EOPNOTSUPP
              The socket is not of a type that supports the listen()
              operation.
*/
inline auto Listen(int sockfd, int num_connections) noexcept -> Result<void>
{
    ASRT_LOG_TRACE("Listening on sockfd {}", sockfd);
    return ::listen(sockfd, num_connections) == -1 ?
        MakeUnexpected(MapAndLogSysError("::listen()")) :
        Result<void>{};
}

/**
 * @brief Returns next socket on pending connection queue
 * 
 * @param sockfd a bound, listening socket ready to accept connections
 * @param addrview peer addr will be written into buffer pointed to by addrview
 * @param flags SOCK_NONBLOCK SOCK_CLOEXEC
 *
 * @note    Error handling
       Linux accept() (and accept4()) passes already-pending network
       errors on the new socket as an error code from accept().  This
       behavior differs from other BSD socket implementations.  For
       reliable operation the application should detect the network
       errors defined for the protocol after accept() and treat them
       like EAGAIN by retrying.  In the case of TCP/IP, these are
       ENETDOWN, EPROTO, ENOPROTOOPT, EHOSTDOWN, ENONET, EHOSTUNREACH,
       EOPNOTSUPP, and ENETUNREACH.
*/
template <typename MutableSocketAddressView>
inline auto Accept(int sockfd, MutableSocketAddressView addrview, int flags = SOCK_CLOEXEC) noexcept -> Result<NativeSocketHandleType>
{
    //static_assert(SocketAddressViewTraits::is_mutable<MutableSocketAddressView>::value, "Socket adddress passed to accept() must be mutable");
    ASRT_LOG_TRACE("Accepting on sockfd {}", sockfd);

    ::socklen_t addr_len{static_cast<::socklen_t>(addrview.size())};
    NativeSocketHandleType const accepted_fd{
        ::accept4(sockfd, 
            reinterpret_cast<::sockaddr *>(addrview.data()), 
            &addr_len,
            flags | SOCK_CLOEXEC)}; 

    //todo needs to set addr size to returned addr_len

    return accepted_fd == -1 ?
            MakeUnexpected(MapAndLogSysError("::accept4()")) :
            Result<NativeSocketHandleType>{accepted_fd};
}

/**
 * @brief Accept without querying for peer address info
*/
inline auto AcceptWithoutPeerInfo(int sockfd, int flags = SOCK_CLOEXEC) noexcept -> Result<NativeSocketHandleType>
{
    ASRT_LOG_TRACE("{}()", __func__);
    NativeSocketHandleType accepted_fd{::accept4(sockfd, nullptr, nullptr, flags | SOCK_CLOEXEC)};
    
    return accepted_fd == -1 ?
            MakeUnexpected(MapAndLogSysError("::accept4()")) :
            Result<NativeSocketHandleType>{accepted_fd};
}

/* 
    @param flags: flags that may be relevaant:
    MSG_DONTWAIT (since Linux 2.2)
              Enables nonblocking operation; if the operation would
              block, EAGAIN or EWOULDBLOCK is returned.  This provides
              similar behavior to setting the O_NONBLOCK flag (via the
              fcntl(2) F_SETFL operation), but differs in that
              MSG_DONTWAIT is a per-call option, whereas O_NONBLOCK is a
              setting on the open file description (see open(2)), which
              will affect all threads in the calling process and as well
              as other processes that hold file descriptors referring to
              the same open file description.
    MSG_MORE (since Linux 2.4.4)
              The caller has more data to send.  This flag is used with
              TCP sockets to obtain the same effect as the TCP_CORK
              socket option (see tcp(7)), with the difference that this
              flag can be set on a per-call basis.

              Since Linux 2.6, this flag is also supported for UDP
              sockets, and informs the kernel to package all of the data
              sent in calls with this flag set into a single datagram
              which is transmitted only when a call is performed that
              does not specify this flag.  (See also the UDP_CORK socket
              option described in udp(7).)
    MSG_NOSIGNAL (since Linux 2.2)
              Don't generate a SIGPIPE signal if the peer on a stream-
              oriented socket has closed the connection.  The EPIPE
              error is still returned.  This provides similar behavior
              to using sigaction(2) to ignore SIGPIPE, but, whereas
              MSG_NOSIGNAL is a per-call feature, ignoring SIGPIPE sets
              a process attribute that affects all threads in the
              process.

    @param buf: contiguous (single) array of memory     
 */
template<typename BufferView>
inline auto Send(int sockfd, BufferView buf, int flags = 0) noexcept -> Result<std::size_t>
{
    //static_assert(BufferViewTraits::is_valid<BufferView>::value, "Invalid buffer view passed to send()");
    ASRT_LOG_TRACE("Sending {} bytes of data on sockfd {}", buf.size(), sockfd);

    //std::cout << "[Syscall] sending: " << static_cast<const char *>(buf.data()) << "\n";
    ::ssize_t const sent_bytes{
        ::send(sockfd, static_cast<const uint8_t*>(buf.data()), static_cast<std::size_t>(buf.size()), flags)};
    if(sent_bytes == -1) [[unlikely]]
    {
        return MakeUnexpected(MapAndLogSysError("::send()"));
    }
    return Result<std::size_t>{sent_bytes};
}

template<typename BufferView>
inline auto SendAll(int sockfd, BufferView buf, int flags = 0) noexcept -> Result<void>
{
    //static_assert(BufferViewTraits::is_valid<BufferView>::value, "Invalid buffer view passed to send()");
    ASRT_LOG_TRACE("Sending sync {} bytes of data on sockfd {}", buf.size(), sockfd);

    for(::ssize_t sent_bytes{};;){

        ::ssize_t const send_result{
            ::send(sockfd, (const uint8_t*)buf.data() + sent_bytes, buf.size(), flags | MSG_NOSIGNAL)};

        if(send_result == -1) [[unlikely]] {
                return MakeUnexpected(MapAndLogSysError("::send()"));
        }

        sent_bytes += send_result;

        if(static_cast<std::size_t>(sent_bytes) == buf.size()) [[likely]]{
            return Result<void>{};
        }
    }
}

template<typename BufferView>
inline auto NonBlockingSend(int sockfd, BufferView buf) noexcept -> Result<std::size_t>
{
    //static_assert(BufferViewTraits::is_valid<BufferView>::value, "Invalid buffer view passed to send()");
    ASRT_LOG_TRACE("Sending non-block {} bytes of data on sockfd {}", buf.size(), sockfd);

    for(;;)
    {
        ::ssize_t const sent_bytes{
            ::send(sockfd, 
                static_cast<const std::uint8_t*>(buf.data()), 
                static_cast<std::size_t>(buf.size()), 
                MSG_DONTWAIT)
        };

        if(sent_bytes >= 0) [[likely]] {
            return Result<std::size_t>{sent_bytes};
        }else if(errno == EINTR) {
            continue;
        }else [[unlikely]] {
            return MakeUnexpected(MapAndLogSysError("::send()"));
        }
    }
}

template<typename MutableBufferView>
inline auto ReceiveWithFlags(int sockfd, MutableBufferView buf, int flags = 0) noexcept -> Result<std::size_t>
{
    static_assert(BufferViewTraits::is_mutable<MutableBufferView>::value, "Invalid buffer view passed to recv()");
    ASRT_LOG_TRACE("Receiveing non-block on sockfd {}", sockfd);

    for(;;){
        ::ssize_t const received_bytes{
            ::recv(sockfd, 
                static_cast<std::uint8_t*>(buf.data()), 
                static_cast<std::size_t>(buf.size()), 
                flags)
        };

        if(received_bytes >= 0) [[likely]] {
            ASRT_LOG_TRACE("Received {} bytes of data on sockfd {}", received_bytes, sockfd);
            return Result<std::size_t>{received_bytes};
        }else if(errno == EINTR) {
            continue;
        }else [[unlikely]] {
            return MakeUnexpected(MapAndLogSysError("::recv()"));
        }
    }
}

/* ::poll() until socket is ready to write */
inline auto PollWrite(int sockfd, int timeout) noexcept -> Result<int>
{
    ASRT_LOG_TRACE("{}()", __func__);

    ::pollfd fds
    {
        .fd = sockfd, 
        .events = POLLOUT,
        .revents = 0
    };

    int const res{::poll(&fds, 1, timeout)};

    if(res == -1) [[unlikely]]
    {
        return MakeUnexpected(MapAndLogSysError("::poll()"));
    }

    return res;
}

inline auto GetFileControl(int fd) noexcept -> Result<int>
{
    ASRT_LOG_TRACE("{}()", __func__);

    int const retval{::fcntl(fd, F_GETFL)};
    if(retval == -1) [[unlikely]]
    {
        return MakeUnexpected(MapAndLogSysError("::fcntl()"));
    }
    return retval;
}

inline auto SetFileControl(int fd, int flags) noexcept -> Result<int>
{
    ASRT_LOG_TRACE("{}()", __func__);

    int const retval{::fcntl(fd, F_SETFL, flags)};
    if(retval == -1) [[unlikely]]
    {
        return MakeUnexpected(MapAndLogSysError("::fcntl()"));
    }
    return retval;
}


inline auto 
GetSocketOptions(int sockfd, int option_level, int option_name, 
    void* option_val, ::socklen_t option_len) noexcept -> Result<void>
{
    ASRT_LOG_TRACE("{}()", __func__);

    ::socklen_t input_len{option_len};
    if(::getsockopt(sockfd,  option_level, option_name, option_val, &input_len) == -1) [[unlikely]] {
        return MakeUnexpected(MapAndLogSysError("::getsockopt()"));
    }else{
        if(input_len > option_len) [[unlikely]] {
            ASRT_LOG_ERROR("length parameter returned by ::getsockopt() exceeds the supplied buffer size");
            return MakeUnexpected(ErrorCode::truncation);
        }
    }

    return Result<void>{};
}

template<typename SocketOption>
inline auto 
GetSocketOptions(int sockfd, SocketOption& option) noexcept -> Result<void>
{
    ASRT_LOG_TRACE("{}()", __func__);

    ::socklen_t option_len{static_cast<::socklen_t>(option.Length())};
    if(::getsockopt(sockfd,  option.Level(), option.Name(), option.data(), &option_len) == -1) [[unlikely]]
    {
        return MakeUnexpected(MapAndLogSysError("::getsockopt()"));
    }
    else
    {
        if(option_len > option.Length()) [[unlikely]] {
            ASRT_LOG_ERROR("length parameter returned by ::getsockopt() exceeds the supplied buffer size");
            return MakeUnexpected(ErrorCode::truncation);
        }
    }

    return Result<void>{};
}

template<typename SocketOption>
inline auto 
SetSocketOptions(int sockfd, SocketOption option) noexcept -> Result<void>
{
    ASRT_LOG_TRACE("{}()", __func__);

   // static_assert(BufferViewTraits::is_valid<BufferView>::value, "Invalid buffer view passed to setsockopt()");
    //todo static assert socket option traits
    if(::setsockopt(sockfd, option.Level(), option.Name(), option.data(), option.Length()) == -1) [[unlikely]] { //todo: is this cast valid? size may exceed socklen_t?
        return MakeUnexpected(MapAndLogSysError("::setsockopt()"));
    }

    return Result<void>{};
}


inline auto 
SetSocketOptions(int sockfd, int option_level, int option_name, 
    const void* option_val, ::socklen_t option_len) noexcept -> Result<void>
{
    ASRT_LOG_TRACE("{}()", __func__);

    if(::setsockopt(sockfd, option_level, option_name, option_val, option_len) == -1) [[unlikely]] {
        return MakeUnexpected(MapAndLogSysError("::setsockopt()"));
    }

    return Result<void>{};
}

template<typename MutableBufferView>
inline auto 
Read(int fd, MutableBufferView buf) noexcept -> Result<std::size_t>
{
    static_assert(BufferViewTraits::is_mutable<MutableBufferView>::value, "Buffer view passed to receive() must be mutable");
    ASRT_LOG_TRACE("Reading on fd {}", fd);

    ::ssize_t const result{
        ::read(fd, static_cast<std::uint8_t*>(buf.data()), static_cast<std::size_t>(buf.size()))};
    if(result == -1) [[unlikely]]
    {
        return MakeUnexpected(MapAndLogSysError("::read()"));
    }

    ASRT_LOG_TRACE("Read {} bytes of data on fd {}", result, fd);
    return result;
}

inline auto 
Read(int fd, void* buf, std::size_t nbytes) noexcept -> Result<std::size_t>
{
    ASRT_LOG_TRACE("Reading on fd {}", fd);

    ::ssize_t const result{::read(fd, buf, nbytes)};
    if(result == -1) [[unlikely]]
    {
        return MakeUnexpected(MapAndLogSysError("::read()"));
    }

    ASRT_LOG_TRACE("Read {} bytes of data on fd {}", result, fd);
    return result;
}

/* 
    @brief
        Read data from socket sockfd into buf.
        Returns the number of bytes read on success.

    @param flags: flags that may be relevaant:
        MSG_DONTWAIT (since Linux 2.2)
              Enables nonblocking operation; if the operation would
              block, the call fails with the error EAGAIN or
              EWOULDBLOCK.  This provides similar behavior to setting
              the O_NONBLOCK flag (via the fcntl(2) F_SETFL operation),
              but differs in that MSG_DONTWAIT is a per-call option,
              whereas O_NONBLOCK is a setting on the open file
              description (see open(2)), which will affect all threads
              in the calling process and as well as other processes that
              hold file descriptors referring to the same open file
              description.
       MSG_PEEK
              This flag causes the receive operation to return data from
              the beginning of the receive queue without removing that
              data from the queue.  Thus, a subsequent receive call will
              return the same data.
       MSG_TRUNC (since Linux 2.2)
              For raw (AF_PACKET), Internet datagram (since Linux
              2.4.27/2.6.8), netlink (since Linux 2.6.22), and UNIX
              datagram as well as sequenced-packet (since Linux 3.4)
              sockets: return the real length of the packet or datagram,
              even when it was longer than the passed buffer.

              For use with Internet stream sockets, see tcp(7).
       MSG_WAITALL (since Linux 2.2)
              This flag requests that the operation block until the full
              request is satisfied.  However, the call may still return
              less data than requested if a signal is caught, an error
              or disconnect occurs, or the next data to be received is
              of a different type than that returned.  This flag has no
              effect for datagram sockets.

    @retval 
        The number of bytes received or
        (0 for end-of-file stream packet or 
            zero-length datagram packets or 
            if requested length is 0 on stream sockets)
        ErrorCode       
*/
template<typename MutableBufferView>
inline auto 
Receive(int sockfd, MutableBufferView buf, int flags = 0) noexcept -> Result<std::size_t>
{
    static_assert(BufferViewTraits::is_mutable<MutableBufferView>::value, "Buffer view passed to receive() must be mutable");
    ASRT_LOG_TRACE("Receiving on sockfd {}", sockfd);

    ::ssize_t const result{
        ::recv(sockfd, static_cast<std::uint8_t*>(buf.data()), static_cast<std::size_t>(buf.size()), flags)};
    if(result == -1) [[unlikely]]
    {
        return MakeUnexpected(MapAndLogSysError("::recv()"));
    }

    ASRT_LOG_TRACE("Received {} bytes of data on sockfd {}", result, sockfd);
    return result;
}

template<typename MutableSockAddrView>
inline auto 
GetPeerName(int sockfd, MutableSockAddrView& addrview) noexcept -> Result<void>
{
    //static_assert(SocketAddressViewTraits::is_mutable<MutableSockAddrView>::value, "Invalid socket adddress view passed to Bind()");
    ASRT_LOG_TRACE("{}()", __func__);

    ::socklen_t addrlen{static_cast<::socklen_t>(addrview.size())};
    if(::getpeername(sockfd, 
        reinterpret_cast<::sockaddr *>(addrview.data()), &addrlen) == -1) [[unlikely]] {
        return MakeUnexpected(MapAndLogSysError("::getpeername()"));
    }

    addrview = addrview.first(addrlen);

    if(addrlen > addrview.size()) [[unlikely]] {
        ASRT_LOG_ERROR("Address truncation when calling getpeername() for sockfd {}", sockfd);
    }

    return Result<void>{};
}

template<typename MutableSockAddrView>
inline auto 
GetSockName(int sockfd, MutableSockAddrView& addrview) noexcept -> Result<void>
{
    //static_assert(SocketAddressViewTraits::is_mutable<MutableSockAddrView>::value, "Invalid socket adddress view passed to Bind()");
    ASRT_LOG_TRACE("{}()", __func__);
    
    ::socklen_t addrlen{static_cast<::socklen_t>(addrview.size())};
    if(::getsockname(sockfd, 
        reinterpret_cast<::sockaddr *>(addrview.data()), &addrlen) == -1) [[unlikely]]{
        return MakeUnexpected(MapAndLogSysError("::getsockname()"));
    }

    addrview = addrview.first(addrlen);

    if(addrlen > addrview.size()) [[unlikely]] {
        ASRT_LOG_ERROR("Address truncation when calling getsockname() for sockfd {}", sockfd);
    }

    return Result<void>{};
}

template <typename MutableStringView>
inline auto 
GetHostName(MutableStringView hostname)
{
    if(::gethostname(hostname.data(), hostname.length()) == -1) [[unlikely]]{
        return MakeUnexpected(MapAndLogSysError("::gethostname()"));
    }
    return Result<void>{};
}


/// @brief poll until ready to read
/// @param sockfd 
/// @param timeout 
/// @param ec 
/// @return A value of zero indicates that the system call timed out before any file descriptors became ready. On error, -1 is returned, and errno is set to indicate the error.

/**
 * @brief poll until ready to read
 * 
 * @param sockfd the socket descriptor to poll for
 * @param eventfd used to interrupt blocking poll() call
 * @param timeout_ms maximum time to poll for (-1 to wait indefinitely)
 * @return Result<int> 
 */
inline auto 
PollRead(int sockfd, int eventfd, int timeout_ms) noexcept -> Result<void>
{
    ASRT_LOG_TRACE("{}()", __func__);

    ::pollfd fds[2] {
        {
            .fd = sockfd, 
            .events = POLLIN | POLLERR,
            .revents = 0
        },
        {
            .fd = eventfd, 
            .events = POLLIN | POLLERR,
            .revents = 0
        }
    };

    int res{::poll(fds, 2, timeout_ms)};

    if(res == -1) [[unlikely]]
        return MakeUnexpected(MapAndLogSysError("::poll()"));

    if(res == 0) [[unlikely]]
        return MakeUnexpected(ErrorCode::timed_out);

    if((fds[0].revents & POLLERR) || (fds[1].revents & POLLERR)) [[unlikely]]
        return MakeUnexpected(ErrorCode::poll_error);

    if(fds[1].revents & POLLIN) [[unlikely]]{
        ASRT_LOG_TRACE("::poll() interrupted by eventfd");
        ::eventfd_t val;
        if(::eventfd_read(eventfd, &val) == -1) [[unlikely]]
            return MakeUnexpected(MapAndLogSysError("::eventfd_read()"));
    }
    return Result<void>{};
}


#if defined ASRT_HAS_EPOLL
[[nodiscard]] inline auto 
EpollCreate(int flags = 0) noexcept -> Result<int>
{
    ASRT_LOG_TRACE("{}()", __func__);
    int const epfd{::epoll_create1(flags)};
    if(epfd == -1) [[unlikely]]
        return MakeUnexpected(MapAndLogSysError("::epoll_create1()"));
    else{
        return epfd;
        ASRT_LOG_TRACE("Epollfd: {}", epfd);
    }
}

inline auto 
EpollControl(int epfd, int op, int monitored_fd, ::epoll_event* event) noexcept -> Result<void>
{
    //std::cout << "[Syscall] " << "Epoll control\n";
    ASRT_LOG_TRACE("EpollControl() for fd {}", monitored_fd);

    int const result{::epoll_ctl(epfd, op, monitored_fd, event)};

    if(result == -1) [[unlikely]]
        return MakeUnexpected(MapAndLogSysError("::epoll_ctl()"));
    else
        return {};
}

/**
 * @brief Enter epoll_wait() event loop; retries when interrupted by signal
 * 
 * @param epfd 
 * @param events 
 * @param maxevents 
 * @param timeout 
 * @return Expected<unsigned int, ErrorCode> 
 */
[[nodiscard]] inline auto 
EpollWait(
    int epfd, 
    ::epoll_event *events, 
    int maxevents, 
    int timeout) noexcept -> Expected<unsigned int, ErrorCode>
{
    ASRT_LOG_TRACE("EpollWait(), maxevents {}, timeout {}", maxevents, timeout);
 
    const int result{
        TEMP_FAILURE_RETRY(::epoll_wait(epfd, events, maxevents, timeout))};

    if(result == -1) [[unlikely]]
        return MakeUnexpected(MapAndLogSysError("::epoll_wait()"));
    else
        /* cast is valid since result is guranteed to be unsigned at this point */
        return static_cast<unsigned int>(result);
}
#endif

#if defined ASRT_HAS_EVENTFD
[[nodiscard]] inline auto
Eventfd(int initval, int flags) noexcept -> Result<unsigned int>
{
    ASRT_LOG_TRACE("{}()", __func__);

    int const result{::eventfd(initval, flags)};

    if(result == -1) [[unlikely]]
        return MakeUnexpected(MapAndLogSysError("::eventfd()"));
    else{
        /* cast is valid since result is guranteed to be unsigned at this point */
        return static_cast<unsigned int>(result);
        ASRT_LOG_TRACE("Eventfd: {}", result);
    }
}

/* Read event counter and possibly wait for events.  */
[[nodiscard]] inline auto 
ReadEventfd(int fd) noexcept -> Result<eventfd_t>
{
    ASRT_LOG_TRACE("{}()", __func__);

    ::eventfd_t value;
    if(::eventfd_read(fd, &value) == -1) [[unlikely]]
        return MakeUnexpected(MapAndLogSysError("::eventfd_read()"));
    else
        return value;
}

/* Increment event counter.  */
inline auto 
WriteEventfd(int fd, ::eventfd_t value) noexcept -> Result<void>
{
    ASRT_LOG_TRACE("{}()", __func__);

    int const result{::eventfd_write(fd, value)};

    if(result == -1) [[unlikely]]
        return MakeUnexpected(MapAndLogSysError("::eventfd_write()"));
    else
        return {};
}
#endif

/**
 * @brief Create a Timer Fd object
 * 
 * @param clockid [in] valid ids: CLOCK_REALTIME, CLOCK_MONOTONIC, CLOCK_BOOTTIME (Since Linux 3.15)
 * @param flags [in] valid flgas (since 2.6.27): TFD_NONBLOCK, TFD_CLOEXEC 
 * @return Expected<unsigned int, ErrorCode> 
 */
[[nodiscard]] inline auto 
TimerFd_Create(int clockid, int flags) noexcept -> Result<unsigned int>
{
    ASRT_LOG_TRACE("{}()", __func__);

    int const result{::timerfd_create(clockid, flags)};

    if(result == -1) [[unlikely]]
        return MakeUnexpected(MapAndLogSysError("::timerfd_create()"));
    else
        /* cast is valid since result is guranteed to be unsigned at this point */
        return static_cast<unsigned int>(result);
}

/**
 * @brief arms (starts) or disarms (stops) the timer referred to by timerfd
 * 
 * @param timerfd file descriptor returned by TimerFd_Create()
 * @param flags TFD_TIMER_ABSTIME for absolute time
 * @param new_val [in] specifies initial expiration and interval for the timer
 * @param old_val [out] if not null, return the settings currently associated with the timer
 * @return Expected<unsigned int, ErrorCode> 
 */
inline auto 
TimerFd_SetTime(
    int timerfd, int flags, 
    const ::itimerspec * new_settings, 
    ::itimerspec * old_settings) noexcept -> Result<void>
{
    ASRT_LOG_TRACE("{}()", __func__);

    if(::timerfd_settime(timerfd, flags, new_settings, old_settings) == -1) [[unlikely]]
        return MakeUnexpected(MapAndLogSysError("::timerfd_settime()"));
    else
        return {};
}

/**
 * @brief 
 * 
 * @param timerfd 
 * @param cur_val 
 * @return Result<void> 
 */
inline auto 
TimerFd_GetTime(int timerfd, itimerspec * cur_settings) noexcept -> Result<void>
{
    ASRT_LOG_TRACE("{}()", __func__);

    if(::timerfd_gettime(timerfd, cur_settings) == -1) [[unlikely]]
        return MakeUnexpected(MapAndLogSysError("::timerfd_gettime()"));
    else
        return {};
}

inline auto
SigEmptySet(::sigset_t *set) noexcept -> Result<void>
{
   return ::sigemptyset(set) == -1 ?
        MakeUnexpected(MapAndLogSysError("::sigemptyset()")) :
        Result<void>{};
}

inline auto
SigFillSet(::sigset_t *set) noexcept -> Result<void>
{
   return ::sigfillset(set) == -1 ?
        MakeUnexpected(MapAndLogSysError("::sigfillset()")) :
        Result<void>{};
}

/**
 * @brief Add signals to set
 * 
 * @tparam SigNum 
 * @param set 
 * @param signals 
 * @return Result<void> 
 */
template <std::same_as<int>... SigNum>
inline auto
SigAddSet(::sigset_t *set, SigNum... signals) noexcept -> Result<void>
{
    for(auto sig : {signals...}){
        ASRT_LOG_TRACE("Adding signal {} to set", sig);
        if(::sigaddset(set, sig) == -1) [[unlikely]]
            return MakeUnexpected(MapAndLogSysError("::sigaddset()"));  
    }
    return Result<void>{};
}

template <std::same_as<int>... SigNum>
inline auto
SigDelSet(::sigset_t *set, SigNum... signals) noexcept -> Result<void>
{
    for(auto sig : {signals...}){
        ASRT_LOG_TRACE("Deleting signal {} from set", sig);
        if(::sigdelset(set, sig) == -1) [[unlikely]]
            return MakeUnexpected(MapAndLogSysError("::sigdelset()"));  
    }
    return Result<void>{};
}

inline auto
SigIsMember(const ::sigset_t *set, int signum) noexcept -> Result<bool>
{
   int const result{::sigismember(set, signum)}; /* Returns 1 if signum is in SET, 0 if not. */
   return result == -1 ?
        MakeUnexpected(MapAndLogSysError("::sigismember()")) :
        Result<bool>{static_cast<bool>(result)};
}

/**
 * @brief Change current thread mask to new_set. Optionally retrieving previous mask through oldset.
 * 
 * @param how 
 * @param set 
 * @param oldset 
 * @return Result<void> 
 */
inline auto
PthreadSigmask(int how, const ::sigset_t * new_set, ::sigset_t * old_set) noexcept -> Result<void>
{
    ASRT_LOG_TRACE("Setting pthread mask");
    return ::pthread_sigmask(how, new_set, old_set) == -1 ?
        MakeUnexpected(MapAndLogSysError("::pthread_sigmask()")) :
        Result<void>{};
}

/**
 * @brief Returns file descriptor pointing to signals to be notified
 * @param mask 
 * @param flags 
 * @return Result<int> 
 */
[[nodiscard]] inline auto 
GetSignalFd(const ::sigset_t *mask, int flags = 0) noexcept -> Result<int>
{
    ASRT_LOG_TRACE("get signalfd");
    int const result{::signalfd(-1, mask, flags)};
    if(result == -1) [[unlikely]]
        return MakeUnexpected(MapAndLogSysError("::signalfd(-1)"));
    else
        return result;
}

/**
 * @brief Request notification for delivery of signals in mask to be
            performed using descriptor sigfd.
 * @param sigfd 
 * @param mask 
 * @param flags 
 * @return Result<void> 
 */
inline auto 
SetSignalFd(int sigfd, const ::sigset_t *mask, int flags = 0) noexcept -> Result<void>
{
    ASRT_LOG_TRACE("set signalfd");
    if(::signalfd(sigfd, mask, flags) == -1) [[unlikely]]
        return MakeUnexpected(MapAndLogSysError("::signalfd()"));
    else
        return Result<void>{};
}

/**
 * @brief Maps memory in user space that is shared with the kernel
 * 
 * @param addr address of the mapped buffer; must be multiple of page size;
 * @param length length of the mapping
 * @param prot memory protection configuration
 * @param flags controls visibility of mapped area to other processes
 * @param fd refers to the file (if any) underlying the mapped memory 
 * @param offset file offset from which the mapping begins
 * @return address of mapped region (void*) on success, system error on failure 
 * 
 * @note man mmap(2): The portable way to create a mapping is to specify addr as 0
       (NULL), and omit MAP_FIXED from flags.  In this case, the system
       chooses the address for the mapping; the address is chosen so as
       not to conflict with any existing mapping, and will not be 0.  If
       the MAP_FIXED flag is specified, and addr is 0 (NULL), then the
       mapped address will be 0 (NULL).
    @note   flag: MAP_ANONYMOUS
              The mapping is not backed by any file; its contents are
              initialized to zero. fd should be -1 if MAP_ANONYMOUS
              (or MAP_ANON) is specified. The offset argument should be zero.
 */
inline auto 
MemoryMap(void* addr, std::size_t length, int prot, int flags, int fd, ::off_t offset) noexcept -> Result<void*>
{
    ASRT_LOG_TRACE("{}()", __func__);

    if(addr == nullptr)
        flags &= ~MAP_FIXED; /* remove fixed-address mapping flag when automatic choosing of mapping address is requested (see note above) */

    const auto mapped_addr{::mmap(addr, length, prot, flags, fd, offset)};

    if(mapped_addr == MAP_FAILED) [[unlikely]] {
        return MakeUnexpected(MapAndLogSysError("::mmap()"));
    }else{
        return mapped_addr;
    }
}

/**
 * @brief Deletes mapping for the specified address range
 * 
 * @param addr 
 * @param length 
 * @return auto 
 */
inline auto 
MemoryUnmap(void* addr, std::size_t length) noexcept -> Result<void>
{
    ASRT_LOG_TRACE("{}()", __func__);

    if(::munmap(addr, length) == -1) [[unlikely]]{
        return MakeUnexpected(MapAndLogSysError("::munmap()"));
    }else{
        return {};
    }
}

}//end S

namespace Libc{

using namespace ErrorCode_Ns;
using ErrorCode = ErrorCode_Ns::ErrorCode;
using namespace Util::Expected_NS;

template <typename T> using Result = Expected<T, ErrorCode>;

void GetAddressInfo();

/**
 * @brief Converts internet address in character string format to binary format
 * 
 * @param address_family AF_INET/AF_INET6
 * @param src_cstring The string representation of the ip address to be converted. Eg: "192.169.0.1"
 * @param dest_binary Destination buffer holding the binary address;
 *                    Must be at least sizeof(struct in6_addr) to accomadate for ip_v4/ip_v6 addresses          
 * @return Result<void> 
 */
inline auto InetPtoN(int address_family, const char* src_cstring, void* dest_binary) noexcept -> Result<void>
{
    if(::inet_pton(address_family, src_cstring, dest_binary) == -1) [[unlikely]]{
        return MakeUnexpected(MapAndLogSysError("::inet_pton()"));
    }else{
        return {};
    }
}

/**
 * @brief Converts struct in_addr to internet address in character string format
 * 
 * @tparam MutableIntenetStringAddrView 
 * @param address_family AF_INET/AF_INET6
 * @param src_binary Points to a buffer holding the binary representation of the to be converted address 
 * @param dest_addr Destination buffer to which the character string will be written. 
 *                  Must be at least INET_ADDRSTRLEN / INET6_ADDRSTRLEN depending on address type
 * @return Expected<const char*, ErrorCode> 
 */
template <typename MutableIntenetStringAddrView>
inline auto InetNtoP(int address_family, const void* src_binary, MutableIntenetStringAddrView dest_addr) noexcept -> Result<const char*>
{
    const char* result{::inet_ntop(address_family, src_binary, 
        (char *)dest_addr.data(), (::socklen_t)dest_addr.size())};
    if(result == nullptr){
        return MakeUnexpected(MapAndLogSysError("::inet_ntop()"));
    }else{
        return result;
    }
}

using HostOrderLongType = std::uint32_t;
using NetworkOrderLongType = std::uint32_t;
using HostOrderShortType = std::uint16_t;
using NetworkOrderShortType = std::uint16_t;

inline auto HostToNetworkLong(HostOrderLongType host_long) noexcept -> NetworkOrderLongType
{
    return ::htonl(host_long);
}

inline auto NetworkToHostLong(NetworkOrderLongType network_long) noexcept -> HostOrderLongType
{
    return ::ntohl(network_long);
}


inline auto HostToNetworkShort(HostOrderShortType host_short) noexcept -> NetworkOrderShortType
{
    return ::htons(host_short);
}

inline auto NetworkToHostShort(NetworkOrderShortType network_short) noexcept -> HostOrderShortType
{
    return ::ntohs(network_short);
}

}

#endif /* ASRT_SYSCALL_HPP_ */
