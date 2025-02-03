#ifndef C5C67EBD_F9B3_4E65_9B49_0703F2FD41DE
#define C5C67EBD_F9B3_4E65_9B49_0703F2FD41DE

#include <cstdint>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <iostream>
#include <linux/if_packet.h>
#include <arpa/inet.h>
#include <memory>

#include "asrt/socket/address_types.hpp" //for constexpr hton
#include "asrt/type_traits.hpp"

namespace Socket
{
    namespace Types
    {
        using NativeSocketHandleType = int;
        using SockAddrType = ::sockaddr;
        using SockAddrStorageType = ::sockaddr_storage;
        using UnixSockAddrType = ::sockaddr_un;
        using PacketSockAddrType = ::sockaddr_ll;
        
        struct ConstGenericSockAddrView
        {
            const SockAddrStorageType *data_;
            ::socklen_t const len_;
        };

        struct MutableGenericSockAddrView
        {
            SockAddrStorageType *data_;
            ::socklen_t len_;
        };

        struct ConstSockAddrView
        {
            const SockAddrType *data_;
            ::socklen_t len_;
        };

        struct MutableSockAddrView
        {
            SockAddrType *data_;
            ::socklen_t len_;
        };

        struct ConstUnixSockAddrView
        {
            const UnixSockAddrType *data_;
            static constexpr ::socklen_t len_{sizeof(UnixSockAddrType)};
        };

        struct MutableUnixSockAddrView
        {
            UnixSockAddrType *data_;
            static constexpr ::socklen_t len_{sizeof(UnixSockAddrType)};
        };

        struct ConstPacketSockAddrView
        {
            const PacketSockAddrType *data_;
            static constexpr ::socklen_t len_{sizeof(PacketSockAddrType)};
        };

        struct MutablePacketSockAddrView
        {
            PacketSockAddrType *data_;
            static constexpr ::socklen_t len_{sizeof(PacketSockAddrType)};
        };

        namespace details
        {
            inline auto ToString(std::uint8_t fam) -> std::string
            {
                std::string printable;
                switch (fam)
                {
                case AF_UNSPEC:
                    printable = "Unspecified";
                    break;
                case AF_UNIX:
                    printable = "Unix";
                    break;
                case AF_INET:
                    printable = "Internet";
                    break;
                [[unlikely]] default:
                    printable = "Invalid";
                    break;
                }
                return printable;
            }
        }

        inline std::ostream& operator<<(std::ostream& os, const SockAddrType &addr)
        {
            os << "[Family: " << details::ToString(addr.sa_family)
               << ", Address: " << addr.sa_data << "]";
            return os;
        }

        inline std::ostream& operator<<(std::ostream& os, const UnixSockAddrType &uaddr)
        {
            os << "[Family: " << details::ToString(uaddr.sun_family)
               << ", Path: " << uaddr.sun_path << "]";
            return os;
        }

        inline std::ostream& operator<<(std::ostream& os, const ConstSockAddrView &addrView)
        {
            os << addrView.data_;
            return os;
        }

        inline std::ostream& operator<<(std::ostream& os, const MutableSockAddrView &addrView)
        {
            os << addrView.data_;
            return os;
        }

        inline std::ostream& operator<<(std::ostream& os, const ConstUnixSockAddrView &addrView)
        {
            os << addrView.data_;
            return os;
        }
        
        inline std::ostream& operator<<(std::ostream& os, const MutableUnixSockAddrView &addrView)
        {
            os << addrView.data_;
            return os;
        }

        inline auto 
        MakeUnixSockAddr(const char *path) -> UnixSockAddrType
        {
            UnixSockAddrType uaddr;
            std::memset(&uaddr, 0, sizeof(uaddr));
            std::strncpy(uaddr.sun_path, path, ::strlen(path));
            uaddr.sun_family = AF_UNIX;
            return uaddr;
        }

        inline constexpr auto 
        MakeUnspecUnixSockAddress(void) -> UnixSockAddrType
        {
            UnixSockAddrType uaddr;
            uaddr.sun_family = AF_UNSPEC;
            return uaddr;
        }

        inline constexpr UnixSockAddrType kUnspecUnixSockAddress{MakeUnspecUnixSockAddress()};

        constexpr inline auto 
        MakePacketSockAddr(int if_index, unsigned short eth_type = ETH_P_ALL) -> PacketSockAddrType
        {
            PacketSockAddrType pktaddr{};
            pktaddr.sll_ifindex = if_index;
            pktaddr.sll_family = AF_PACKET;
            pktaddr.sll_protocol = AddressTypes::ToNetwork(eth_type);
            return pktaddr;
        }

        /*!
         * \brief Result of the Receive() operation.
         */
        enum class ConnectResult : std::uint8_t
        {
            kConnectCompleted = 0,
            kAsyncNeeded = 1,
        };

        enum class ShutdownType : std::uint8_t
        {
            kDisableRx = SHUT_RD, /* Receive() disabled */
            kDisableTx = SHUT_WR, /* Send() disabled */
            kDisableTxRx = SHUT_RDWR /* Send() & Receive() disabled */
        };

        enum class ReactorObservation : std::uint8_t
        {
            kNone = 0,
            kRead = 1,
            kWrite = 2,
            kReadWrite = 3

        };

        enum class MessageFlags : int{
            Peek = MSG_PEEK,
            OutOfBand = MSG_OOB,
            DoNotRoute = MSG_DONTROUTE,
            EndOfRecord = MSG_EOR
        };


        enum class OperationType : std::uint8_t
        {
            kSend,
            kReceive,
            kConnect,
            kOpTypeMax
        };

        enum OperationContext : std::uint8_t
        {
            kInitiation, 
            kContinuation
        };

        enum OperationMode : int
        {
            kOpModeNone = 0u,
            kSpeculative = 0x01u, 
            kExhaustive = 0x02u
        };

        enum class OperationStatus : std::uint8_t
        {
            kComplete, 
            kAsyncNeeded
        };

        static constexpr const char* 
        kOpTypeStrs[(std::uint8_t)OperationType::kOpTypeMax]
        {
            "Send",
            "Receive",
            "Connect"
        };

        constexpr inline auto 
        ToStringView(OperationType op_type) noexcept -> std::string_view
        {
            return kOpTypeStrs[(std::uint8_t)op_type];
        }
    }
}

#endif /* C5C67EBD_F9B3_4E65_9B49_0703F2FD41DE */
