#ifndef A0104726_280E_40E5_9563_284AD84E4814
#define A0104726_280E_40E5_9563_284AD84E4814

#include <cstdint>
#include <cstring>
#include <cassert>
#include <array>
#include <sys/socket.h>
#include <sys/un.h>
#include <string>
#include <iostream>

#include "asrt/socket/types.hpp"
#include "asrt/type_traits.hpp"
//#include "asrt/protocol.hpp"

namespace ProtocolNS
{
    enum class ProtoType : std::uint8_t;
};

namespace Endpoint_NS{
/* +1 to ensure socket address is null terminated on retrieval */
//static constexpr std::uint8_t KMaxUnixSockAddrBuffSize{sizeof(sockaddr_un) + 1};

using namespace Socket::Types;
using ProtocolNS::ProtoType;

enum class PortNumber : std::uint16_t {};

/**
 * @brief A basic endpoint is an representation of an entity which a server can bind to
 *          and which a client can connect to.
 * 
 * @tparam InternetProtocol 
 */
template<typename InternetProtocol>
class GenericEndpoint
{
public:
    using ProtocolType = InternetProtocol;

    constexpr GenericEndpoint() noexcept = default;
    constexpr GenericEndpoint(const InternetProtocol&, PortNumber) noexcept;
    constexpr GenericEndpoint(const void* sockaddr, std::size_t sockaddrsize) noexcept;
    GenericEndpoint(GenericEndpoint const&) = delete;
    GenericEndpoint(GenericEndpoint&&) = delete;
    GenericEndpoint &operator=(GenericEndpoint const &other) = delete;
    GenericEndpoint &operator=(GenericEndpoint &&other) = delete;
    ~GenericEndpoint() noexcept = default;

    constexpr auto size() const -> std::size_t {return this->size_;};
    constexpr auto Family() const -> std::uint8_t {return InternetProtocol{}.Family();};
    constexpr auto ProtoName() const -> std::uint8_t {return InternetProtocol{}.Name();};

private:

    union AddrStorage
    {
        SockAddrType base;
        SockAddrStorageType storage;
    } addr_{};

    std::size_t size_{};
    //Protocol proto_;
};

template<typename Protocol>
constexpr GenericEndpoint<Protocol>::
GenericEndpoint(const void* sockaddr, std::size_t sockaddrsize) noexcept
{
    assert((sockaddrsize > 0) 
        && (sockaddrsize <= sizeof(SockAddrStorageType)));

    std::memset(&this->addr_.storage, 0, sizeof(addr_.storage));
    std::memcpy(&this->addr_.storage, sockaddr, sockaddrsize);
    this->size_ = sockaddrsize;
}


//GenericEndpoint<ProtocolNS::UnixDgram> ep{"192.17.3", 3};

}

#endif /* A0104726_280E_40E5_9563_284AD84E4814 */
