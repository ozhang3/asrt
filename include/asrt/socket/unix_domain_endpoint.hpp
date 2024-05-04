#ifndef A988F159_44DB_4A18_8C75_86BB4CEA1D06
#define A988F159_44DB_4A18_8C75_86BB4CEA1D06

#include <cstdint>
#include <cstring>
#include <cassert>
#include <array>
#include <sys/socket.h>
#include <sys/un.h>
#include <string>
#include <iostream>

#include "asrt/netbuffer.hpp"
#include "asrt/socket/types.hpp"
#include "asrt/type_traits.hpp"

namespace Unix{

using namespace Socket::Types;

/**
 * @brief Unix Domain Endpoint implementation
 * 
 * @tparam Protocol_ valid protocols: ProtocolNS::UnixStream, ProtocolNS::UnixDgram
 */
template<typename Protocol_>
class UnixDomainEndpoint
{
public:

    static_assert(ProtocolTraits::is_unix_domain<Protocol_>::value, "Invalid protocol");

    using ProtocolType = Protocol_;

    constexpr UnixDomainEndpoint() noexcept = default;
    constexpr UnixDomainEndpoint(const char* sockpath) noexcept;
    explicit UnixDomainEndpoint(const std::string& sockpath) noexcept;
    UnixDomainEndpoint(UnixDomainEndpoint const&) = default;
    UnixDomainEndpoint(UnixDomainEndpoint&&) = default;
    UnixDomainEndpoint &operator=(UnixDomainEndpoint const &other) = default;
    UnixDomainEndpoint &operator=(UnixDomainEndpoint &&other) = default;
    constexpr ~UnixDomainEndpoint() noexcept = default;

    static constexpr std::size_t capacity() noexcept {return sizeof(UnixSockAddrType);}

    SockAddrType* data() noexcept { return &this->addr_.base; }
    const SockAddrType* data() const noexcept { return &this->addr_.base; }

    void resize(std::size_t new_size) noexcept 
    {
        if(new_size > sizeof(UnixSockAddrType)) [[unlikely]] {
            LogFatalAndAbort("invalid resize argument {}", new_size);
        }else if (new_size == 0) {
            this->path_len_ = 0;
        }else{
            this->path_len_ = new_size - (offsetof(UnixSockAddrType, sun_path));

            /* check for null-termination */
            if(path_len_ > 0 && this->addr_.local.sun_path[path_len_ - 1] == 0){
                this->path_len_--;
            }
        }
    }

    constexpr std::size_t size() const noexcept
    {
        return this->path_len_ + (offsetof(UnixSockAddrType, sun_path));
    }
    
    constexpr const char* Path() const noexcept {return this->addr_.local.sun_path;}

    constexpr Buffer::ConstBufferView DataView() const noexcept {
        return {&(this->addr_.base), this->size()};
    } 

    constexpr Buffer::MutableBufferView DataView() noexcept {
        return {&(this->addr_.base), this->size()};
    }

    constexpr Protocol_ Protocol() const noexcept {return Protocol_{};}
    void SetPath(const char* sockpath) noexcept;
    void SetPath(const std::string& sockpath) noexcept;

    static constexpr int Family() noexcept {return Protocol_{}.Family();}
    static constexpr typename Protocol_::ProtoName ProtoName() noexcept {return Protocol_{}.Name();}

    constexpr auto ToString() const noexcept {return this->Path();}
    
    bool operator==(const UnixDomainEndpoint& other) const noexcept {return Path() == other.Path();}

private:
    constexpr void setPath_(const char* sockpath) noexcept;

    union UnixSockAddrStorage
    {
        SockAddrType base;
        UnixSockAddrType local;
    } addr_{};

    std::uint8_t path_len_{}; /* max unix domain socket path: 108 bytes */
};

template<typename Protocol_>
inline void UnixDomainEndpoint<Protocol_>::
SetPath(const char* sockpath) noexcept
{
    this->setPath_(sockpath);
}


template<typename Protocol_>
inline void UnixDomainEndpoint<Protocol_>::
SetPath(const std::string& sockpath) noexcept
{
    this->setPath_(sockpath.c_str());
}

template<typename Protocol_>
constexpr inline void UnixDomainEndpoint<Protocol_>::
setPath_(const char* sockpath) noexcept
{
    const std::size_t pathlen{std::char_traits<char>::length(sockpath) + 1};
    assert((pathlen > 0) && (pathlen <= sizeof(UnixSockAddrType::sun_path) - 1));
    this->path_len_ = pathlen;
    for(std::size_t i{}; i < pathlen; i++){
        this->addr_.local.sun_path[i] = sockpath[i];
    }
    this->addr_.local.sun_family = AF_UNIX;

}

template<typename Protocol_>
constexpr inline UnixDomainEndpoint<Protocol_>::
UnixDomainEndpoint(const char* sockpath) noexcept
    : addr_{}
{
    //ASRT_LOG_TRACE("Initializing endpoint from cstring, path: {}", sockpath);
    
    this->setPath_(sockpath);
}

template<typename Protocol_>
inline UnixDomainEndpoint<Protocol_>::
UnixDomainEndpoint(const std::string& sockpath) noexcept
    : addr_{}
{
    this->setPath_(sockpath.c_str());
}

} //end ns Unix

template<typename Protocol>
struct fmt::formatter<Unix::UnixDomainEndpoint<Protocol>> : fmt::formatter<const char*>
{
    auto format(Unix::UnixDomainEndpoint<Protocol> const& ep, format_context &ctx) const -> decltype(ctx.out())
    {
        return format_to(ctx.out(), "[{}]", ep.Path());
    }
};

#endif /* A988F159_44DB_4A18_8C75_86BB4CEA1D06 */
