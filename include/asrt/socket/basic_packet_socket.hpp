#ifndef D396B89B_946C_4B0C_A315_8EBFC474DA9B
#define D396B89B_946C_4B0C_A315_8EBFC474DA9B

#include <cstdint>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <memory>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/filter.h>
//#include <sys/un.h>
#include <iostream>
#include <stop_token>

#include "asrt/error_code.hpp"
#include "asrt/socket/protocol.hpp"
#include "asrt/sys/syscall.hpp"
#include "asrt/netbuffer.hpp"
#include "asrt/socket/types.hpp"
#include "asrt/util.hpp"
#include "expected.hpp"
#include "asrt/common_types.hpp"
#include "asrt/socket/basic_socket.hpp"
#include "asrt/socket/socket_option.hpp"

#ifndef TPACKET3_HDRLEN
# error "Packet socket will only work with version 3 packet mmap rings"
#endif

namespace Socket::PacketSocket{


namespace details{
    /* Packet socket configs */
    //         block #1                 block #2
    // +---------+---------+    +---------+---------+
    // | frame 1 | frame 2 |    | frame 3 | frame 4 |
    // +---------+---------+    +---------+---------+

    //         block #3                 block #4
    // +---------+---------+    +---------+---------+
    // | frame 5 | frame 6 |    | frame 7 | frame 8 |
    // +---------+---------+    +---------+---------+
    static constexpr unsigned int kEthHeaderLength{18u}; //18 bytes
    static constexpr unsigned int kMaxEthFrameLength{1500u}; //1500 bytes
    static constexpr unsigned int kStdEthMTU{kEthHeaderLength + kMaxEthFrameLength}; //1518 bytes
    static constexpr unsigned int kMaxEthMTU{65536u}; // loop back interface MTU
    static constexpr unsigned int kDefaultBlockSize{1 << 21}; //2Mb
    static constexpr unsigned int kDefaultFrameSize{1 << 11}; //2048
    static constexpr unsigned int kDefaultBlockNum{64u};
    static constexpr unsigned int kDefaultBlockTimeoutMs{60u}; /* the max time (in ms) to wait for a block to be ready */
    static constexpr unsigned int kDefaultMmapSize{kDefaultBlockNum * kDefaultBlockSize};
    static constexpr unsigned int kDefaultPacketMmapVersion{TPACKET_V3};

    enum {kMaxPacketType = PACKET_KERNEL + 1};
    static constexpr std::string_view kPacketTypePrintables[kMaxPacketType]
    {
        "HOST",
        "BROADCAST",
        "MULTICAST",
        "OTHERHOST",
        "OUTGOING",
        "LOOPBACK",
        "USER",
        "KERNEL"
    };
    
    enum PacketType : unsigned char {
        kHost = PACKET_HOST,
        kBroadcast = PACKET_BROADCAST,
        kMulticast = PACKET_MULTICAST,
        kOtherHost = PACKET_OTHERHOST,
        kOutgoing = PACKET_OUTGOING,
        kLoopback = PACKET_LOOPBACK,
        kUser = PACKET_USER,
        kKernel = PACKET_KERNEL
    };

    enum PacketMmapMode : std::uint8_t{
        kDisabled,
        kMmap_Rx,
        kMmap_Tx,
        kMmap_TxRx
    };
    enum {kMaxPacketMmapMode = PacketMmapMode::kMmap_TxRx + 1};
    static constexpr std::string_view kPacketMmapModePrintables[kMaxPacketMmapMode]
    {
        "Disabled",
        "MmapRx",
        "MmapTx",
        "MmapTxRx"
    };

    inline auto ToStringView(PacketMmapMode mmap_mode) -> std::string_view
    {
        return kPacketMmapModePrintables[mmap_mode];
    }

    inline auto ToStringView(PacketType packet_type) -> std::string_view
    {
        return kPacketTypePrintables[packet_type];
    }

    struct PacketMmapRing{
        ::iovec *rd{nullptr};
        std::uint8_t *map{nullptr};
        ::tpacket_req3 req{};
    };

    template <int OptionName>
    struct PacketMembershipOption : 
        SockOption::SocketOption<PacketMembershipOption<OptionName>, SOL_PACKET, OptionName>
    {
        PacketMembershipOption(int membership_type, const char* if_name) noexcept
        {
            std::memset(&this->mreq_, 0, sizeof(::packet_mreq));
            this->mreq_.mr_ifindex = OsAbstraction::GetNetIfIndex(if_name).value_or(-1);
            this->mreq_.mr_type    = membership_type;
        }
        constexpr std::size_t Length() const {return sizeof(::packet_mreq);}
        auto data() {return &mreq_;}
        const auto data() const {return &mreq_;}
    private:
        ::packet_mreq mreq_{};
    };

    template
    struct PacketMembershipOption<PACKET_ADD_MEMBERSHIP>;

    template
    struct PacketMembershipOption<PACKET_DROP_MEMBERSHIP>;

    template <int OptionName>
    struct SocketFilter : 
        SockOption::SocketOption<SocketFilter<OptionName>, SOL_SOCKET, OptionName>
    {
        template <unsigned short N>
        SocketFilter(const ::sock_filter(&filter)[N]) noexcept
        {
            this->sfprog_.filter = (::sock_filter*)&filter;
            this->sfprog_.len = N;
        }
        constexpr std::size_t Length() const {return sizeof(::sock_fprog);}
        auto data() {return &sfprog_;}
        const auto data() const {return &sfprog_;}
    private:
        ::sock_fprog sfprog_;
    };

    template struct SocketFilter<SO_ATTACH_FILTER>;
    template struct SocketFilter<SO_DETACH_FILTER>;
    template struct SocketFilter<SO_LOCK_FILTER>;

    template <int FanoutMode>
    struct PacketFanout : SockOption::IntOption<SOL_PACKET, PACKET_FANOUT>
    {
        using Base = SockOption::IntOption<SOL_PACKET, PACKET_FANOUT>;
        PacketFanout(unsigned int fanout_group_id) noexcept 
            : Base{(int)fanout_group_id | FanoutMode << 16} {}
    };

    template struct PacketFanout<PACKET_FANOUT_HASH>;
    template struct PacketFanout<PACKET_FANOUT_CPU>;
    template struct PacketFanout<PACKET_FANOUT_ROLLOVER>;
    template struct PacketFanout<PACKET_FANOUT_LB>;

    /* packet socket options */
    using QdiscBypass = SockOption::BoolOption<SOL_PACKET, PACKET_QDISC_BYPASS>;
    using PacketLoss = SockOption::BoolOption<SOL_PACKET, PACKET_LOSS>;
    using PacketFanoutHash = PacketFanout<PACKET_FANOUT_HASH>;
    using PacketFanoutCPU = PacketFanout<PACKET_FANOUT_CPU>;
    using PacketFanoutRollover = PacketFanout<PACKET_FANOUT_ROLLOVER>;
    using PacketFanoutRoundRobin = PacketFanout<PACKET_FANOUT_LB>;
    using PacketVersion = SockOption::IntOption<SOL_PACKET, PACKET_VERSION>;
    using PacketAuxData = SockOption::BoolOption<SOL_PACKET, PACKET_AUXDATA>;
    using PacketAddMembership = PacketMembershipOption<PACKET_ADD_MEMBERSHIP>;
    using PacketDropMembership = PacketMembershipOption<PACKET_DROP_MEMBERSHIP>;
    using PacketAttachFilter = SocketFilter<SO_ATTACH_FILTER>;
    using PacketDropFilter = SocketFilter<SO_DETACH_FILTER>;

    using MmapRingBlock = ::tpacket_block_desc;
    using MmapRingPacket = ::tpacket3_hdr;
    using MmapRingPacketStats = ::tpacket_stats_v3;

    inline auto GetFirstPacketInBlock(MmapRingBlock* pBlockStart) -> MmapRingPacket* {
        return reinterpret_cast<MmapRingPacket*>(
                    reinterpret_cast<std::uint8_t*>(pBlockStart) +
                    pBlockStart->hdr.bh1.offset_to_first_pkt);
    }

    inline auto GetNextPacket(MmapRingPacket* packet) -> MmapRingPacket* {
        return reinterpret_cast<MmapRingPacket*>(
                    reinterpret_cast<std::uint8_t*>(packet) + 
                    packet->tp_next_offset);
    }

    inline void ReturnBlockToKernel(MmapRingBlock* block){
        __atomic_store_n(&block->hdr.bh1.block_status, TP_STATUS_KERNEL, __ATOMIC_RELEASE);
    }

    inline auto GetPacketCountInBlock(MmapRingBlock const* block) -> unsigned int {
        return block->hdr.bh1.num_pkts;
    }

    inline auto GetBlockAtIndex(PacketMmapRing const* ring, unsigned int block_index) -> MmapRingBlock* {
        /* void* -> tpacket_block_desc* */
        return static_cast<MmapRingBlock *>(ring->rd[block_index].iov_base);
    }
    
    inline auto GetBlockStatus(MmapRingBlock const* block) -> unsigned int {
        return __atomic_load_n(&block->hdr.bh1.block_status, __ATOMIC_ACQUIRE);
    }

    inline auto GetBlockStatusAtIndex(PacketMmapRing* ring, unsigned int index) -> unsigned int {
        auto pBlockStart{GetBlockAtIndex(ring, index)};
        return pBlockStart->hdr.bh1.block_status;
    }

    inline auto IsBlockAvailable(MmapRingBlock* block) -> bool{
        return (GetBlockStatus(block) & TP_STATUS_USER) != 0; 
    }

    inline auto GetSockAddrLLFromPacket(MmapRingPacket * packet) -> ::sockaddr_ll*
    {
        return reinterpret_cast<::sockaddr_ll*>(
            reinterpret_cast<std::uint8_t *>(packet) + TPACKET_ALIGN(sizeof(*packet)));
    }

    inline auto GetPacketType(MmapRingPacket * packet) -> PacketType
    {
        return PacketType{GetSockAddrLLFromPacket(packet)->sll_pkttype};
    }

    inline auto GetPacketLLProtocol(MmapRingPacket * packet) -> unsigned short
    {
        return GetSockAddrLLFromPacket(packet)->sll_protocol;
    }

    inline auto GetPacketVlanTpid(MmapRingPacket * packet) -> std::uint32_t
    {
        return packet->hv1.tp_vlan_tpid;
    }

    inline auto GetPacketVlanTci(MmapRingPacket * packet) -> std::uint32_t
    {
        return packet->hv1.tp_vlan_tci;
    }

    inline bool IsPacketVlanValid(MmapRingPacket * packet)
    {
        return (GetPacketVlanTci(packet) != 0 || (packet->tp_status & TP_STATUS_VLAN_TPID_VALID));
    }

    #include <iterator> // For std::forward_iterator_tag
    #include <cstddef>  // For std::ptrdiff_t

    class MmapRingBlockExp;
    class MmapRingPacketExp
    {
    public:
        using PacketType = ::tpacket3_hdr;
        //using Ptr2Packet = PacketType*;

        friend class MmapRingBlockExp;

        MmapRingPacketExp(PacketType* packet) noexcept : packet_{packet} {}

        auto data() noexcept {return ((std::uint8_t*)packet_ + packet_->tp_mac);}
        const auto data() const noexcept {return ((std::uint8_t*)packet_ + packet_->tp_mac);}
        auto Length() noexcept {return packet_->tp_len;}

        operator bool() const noexcept {return packet_;}

        auto Type() noexcept {return details::GetPacketType(packet_);}
        auto EtherProto() noexcept {return ::ntohs(details::GetPacketLLProtocol(packet_));}
        bool IsVlanValid() noexcept {return details::IsPacketVlanValid(packet_);}
        auto Tpid() noexcept {return details::GetPacketVlanTpid(packet_);}
        auto VlanTag() noexcept {return details::GetPacketVlanTci(packet_) & 0x0fffu;}

        friend bool operator== (const MmapRingPacketExp& a, const MmapRingPacketExp& b) = default;
        friend bool operator!= (const MmapRingPacketExp& a, const MmapRingPacketExp& b) = default;

    private:
        auto next_() -> MmapRingPacketExp
        {
            return details::GetNextPacket(packet_);
        }

        PacketType* packet_{nullptr};
    };
    
    class MmapRingBlockExp
    {

    public:
    
        struct Iterator 
        {
            using iterator_category = std::input_iterator_tag;
            using difference_type   = std::ptrdiff_t;
            using value_type        = MmapRingPacketExp;
            using pointer           = value_type*;
            using reference         = value_type const&;

            Iterator(value_type start, unsigned int max) noexcept 
                : start_(start), max_incr_{max} {}

            reference operator*() const { assert(start_); return start_; }

            // Prefix increment
            Iterator& operator++()
            {
                assert(start_);
                
                start_ = --max_incr_ ? 
                    start_.next_() : nullptr;
                return *this; 
            }  

            // Postfix increment
            Iterator operator++(int) 
            {
                Iterator tmp = *this; 
                ++(*this); 
                return tmp; 
            }

            friend bool operator== (const Iterator& a, const Iterator& b) { return a.start_ == b.start_; }
            friend bool operator!= (const Iterator& a, const Iterator& b) { return a.start_ != b.start_; }

        private:
            value_type start_;
            unsigned int max_incr_; //max increment count
        };

        MmapRingBlockExp(MmapRingBlock * block) noexcept : block_{block} {}

        auto Front() noexcept { return details::GetFirstPacketInBlock(block_); }

        auto size() const noexcept { return details::GetPacketCountInBlock(block_); }

        bool IsReady() const noexcept { return details::IsBlockAvailable(block_); }

        auto Consume() noexcept { details::ReturnBlockToKernel(block_); }

        Iterator begin() noexcept { return {this->Front(), this->size()}; }
        Iterator end() noexcept { return {nullptr, 0}; }

    private:
        MmapRingBlock * block_;
    };

}


using namespace Util::Expected_NS;
using Util::Optional_NS::Optional;


/**
 * @brief L2 socket (AF_PACKET) implementation
 * 
 * @tparam Protocol SOCKET_RAW / SOCKET_DGRAM
 * @tparam Executor 
 */
template <typename Protocol, typename Executor>
class BasicPacketSocket : 
    public BasicSocket<
        Protocol, 
        BasicPacketSocket<Protocol, Executor>,
        Executor>
{

    static_assert(ProtocolTraits::is_packet_level<Protocol>::value, "Invalid protocol used. Packet level protocol only!");

public:
    using SockErrorCode = ErrorCode_Ns::ErrorCode;
    using EndPointType = typename Protocol::Endpoint; /* should be a packet endpoint */
    using Events = ReactorNS::Events;
    using SendCompletionHandlerType = std::function<void(Expected<std::size_t, SockErrorCode>&& send_result)>;
    using ReceiveCompletionHandlerType = std::function<void(Expected<std::size_t, SockErrorCode>&& receive_result)>;
    //using SendBlockCompletionHandler = std::function<void(Expected<std::size_t, SockErrorCode>&& send_result)>;
    using ReceiveBlockCompletionHandlerType = std::function<void(details::MmapRingBlockExp)>;
    using Base = BasicSocket<Protocol, BasicPacketSocket, Executor>;
    using typename Base::AddressType;
    using typename Base::Reactor;
    using typename Base::EventType;
    using typename Base::MutexType;
    template <typename T> using Result = Expected<T, SockErrorCode>;

    BasicPacketSocket() noexcept = default;
    explicit BasicPacketSocket(Executor& executor) noexcept : Base{executor} {};
    explicit BasicPacketSocket(Optional<Executor&> executor) noexcept : Base{executor} {};
    explicit BasicPacketSocket(Reactor& reactor) : Base{reactor} {}
    BasicPacketSocket(BasicPacketSocket const&) = delete;
    BasicPacketSocket(BasicPacketSocket&& other) noexcept;
    BasicPacketSocket &operator=(BasicPacketSocket const &other) = delete;
    BasicPacketSocket &operator=(BasicPacketSocket &&other) = delete;
    ~BasicPacketSocket() noexcept
    {
        if(this->ring_.rd){
            std::free(this->ring_.rd);
        }
        Base::Destroy();
    }

    /**
     * @brief Opens a packet socket with option to enable packet_mmap features
     * @details Overrides base class Open() call
     * 
     * @param enable_packet_mmap whether to enable packet_mmap
     * @return Result<void> 
     */
    auto Open(details::PacketMmapMode enable_packet_mmap) noexcept -> Result<void>;

    /**
     * @brief Binds to a packet Endpoint
     * @details
     * 
     * @param ep the endpoint to bind to
     * @return Result<void> 
     */
    auto BindToEndpointImpl(const EndPointType& ep) noexcept -> Result<void>;

    void OnReactorEventImpl(Events ev, std::unique_lock<MutexType>& lock) noexcept;

    void OnCloseEvent() noexcept;

    auto SendSome(Buffer::ConstBufferView buffer_view) noexcept -> Result<std::size_t>;
    

    /**
     * @brief 
     * @details Datagram sockets in various domains (e.g., the UNIX and Internet domains) permit zero-length datagrams. 
     * When such a datagram is received, the return value is 0.
     */
    auto ReceiveSome(Buffer::MutableBufferView buffer_view) noexcept -> Result<std::size_t>;

    template<typename ReceiveCompletionHandler>
    auto ReceiveAsync(Buffer::MutableBufferView buffer_view, ReceiveCompletionHandler&& handler) noexcept -> Result<void>;

    //auto RecvFromSync(const Buffer::NetBufferType& buffer_view) -> bool; //todo

    template<typename ReceiveBlockCompletionHandler>
    auto ReceiveBlockAsync(ReceiveBlockCompletionHandler&& handler) noexcept -> Result<void>;

    template<typename ReceiveBlockCompletionHandler>
    auto PollReceiveBlockSync(ReceiveBlockCompletionHandler&& handler, std::stop_token stoken) noexcept -> Result<void>;


    friend std::ostream& operator<<(std::ostream& os, const BasicPacketSocket& socket) noexcept
    {
        os << "[socket protocol: " << Protocol{}
            << ", socket fd: " << socket.GetNativeHandle()
            << ", state: " << socket.GetBasicSocketState()
            << ", blocking: " << std::boolalpha << !socket.IsNonBlocking() 
            << ", mmap: " << details::ToStringView(socket.packet_mmap_mode_) << "]\n";
        return os;
    }

    Result<details::MmapRingPacketStats> GetSocketStats() noexcept
    {
        details::MmapRingPacketStats stats{};
        return OsAbstraction::GetSocketOptions(this->GetNativeHandle(), 
            SOL_PACKET, PACKET_STATISTICS, &stats, sizeof(stats))
            .map([&stats](){
                return stats;
            });
    }

private:

    auto SetupPacketMmap() noexcept -> Result<void>;

    void HandleSend(std::unique_lock<MutexType>& lock) noexcept;
    void HandleReceive(std::unique_lock<MutexType>& lock) noexcept;
    void HandleBlockReceive(std::unique_lock<MutexType>& lock) noexcept;

    auto CheckSendPossible() const noexcept -> Result<void>;

    auto CheckRecvPossible() const noexcept -> Result<void>;


    bool IsAsyncInProgress() const noexcept {return (this->send_ongoing_ || this->recv_ongoing_);}

    void IncrementBlockIndex() noexcept {this->current_block_index_ = (this->current_block_index_ + 1) % this->total_block_num_;};
    /**
     * @brief Get pointer to current block in mmapped ring. Assumes packet_mmap enabled!
     * 
     * @return pointer to block at current index
     */
    auto GetCurrentBlockUnsafe() const noexcept -> details::MmapRingBlock* {
        /* void* -> tpacket_block_desc* */
        return static_cast<details::MmapRingBlock *>(this->ring_.rd[this->current_block_index_].iov_base);
    }

    bool IncrementAndCheckNextBlockAvailable() noexcept
    {
        this->current_block_index_ = (this->current_block_index_ + 1) % this->total_block_num_;
        return details::IsBlockAvailable(
            static_cast<details::MmapRingBlock *>(
                this->ring_.rd[this->current_block_index_].iov_base));
    }

    bool IsPacketMmapEnabled() const noexcept {return this->packet_mmap_mode_ != details::PacketMmapMode::kDisabled;}

    bool recv_ongoing_{false};
    bool send_ongoing_{false};

    bool packet_mmap_block_read_pending_{false};
    //bool is_bound_to_endpoint_{false};

    Buffer::ConstBufferView send_buffview_{nullptr, 0};
    Buffer::MutableBufferView recv_buffview_{nullptr, 0};

    SendCompletionHandlerType send_completion_handler_{};
    ReceiveCompletionHandlerType recv_completion_handler_{};

    details::PacketMmapMode packet_mmap_mode_{details::PacketMmapMode::kDisabled};
    details::PacketMmapRing ring_;
    int current_block_index_{};
    int total_block_num_{details::kDefaultBlockNum}; //todo make this const
    ReceiveBlockCompletionHandlerType recv_block_completion_handler_{};
};

} //end namespace

#if defined(ASRT_HEADER_ONLY)
# include "asrt/socket/impl/packet_socket.ipp"
#endif // defined(ASRT_HEADER_ONLY)

#endif /* D396B89B_946C_4B0C_A315_8EBFC474DA9B */
