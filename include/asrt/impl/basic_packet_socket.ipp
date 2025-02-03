#include "asrt/socket/basic_packet_socket.hpp"

namespace Socket::PacketSocket{

template <typename Protocol, class Executor>
inline auto BasicPacketSocket<Protocol, Executor>::
SetupPacketMmap() noexcept -> Result<void>
{   
    ASRT_LOG_TRACE("Using mmap ring for packet socket");
    return OsAbstraction::SetSocketOptions(this->GetNativeHandle(),
            details::PacketVersion{details::kDefaultPacketMmapVersion}) //enforce v3 pacekt_mmap
        .map_error([this](SockErrorCode ec){
            ASRT_LOG_ERROR("Failed to use v3 pacekt_mmap for socket: {}", ec);
            return ec;
        })
        .and_then([this]() -> Result<void> {
            /* setup request for mapping rx ring buffer */
            std::memset(&(this->ring_.req), 0, sizeof(this->ring_.req));
            this->ring_.req.tp_block_size = details::kDefaultBlockSize; //num of bytes in each block
            this->ring_.req.tp_frame_size = details::kDefaultFrameSize; //size of each frame (larger than eth frame size to accomadate for aux data)
            this->ring_.req.tp_block_nr = details::kDefaultBlockNum; //num of blocks in ring
            this->ring_.req.tp_frame_nr = (details::kDefaultBlockSize * details::kDefaultBlockNum) / details::kDefaultFrameSize;
            this->ring_.req.tp_retire_blk_tov = details::kDefaultBlockTimeoutMs; /* the max time (in ms) to wait for a block to be ready */
            //this->ring_.req.tp_feature_req_word = TP_FT_REQ_FILL_RXHASH;
            return OsAbstraction::SetSocketOptions(this->GetNativeHandle(), SOL_PACKET, PACKET_RX_RING, 
                    &(this->ring_.req), sizeof(this->ring_.req))
                .map_error([this](SockErrorCode ec){
                    ASRT_LOG_ERROR("Failed to setup rx ring buffer for socket: {}", ec);
                    return ec;
                });
        })
        .and_then([this]() -> Result<void*> {
            return OsAbstraction::MemoryMap(nullptr, this->ring_.req.tp_block_size * this->ring_.req.tp_block_nr,
                    PROT_READ | PROT_WRITE, MAP_SHARED, Base::GetNativeHandle(), 0)
            .map_error([this](SockErrorCode ec){
                ASRT_LOG_ERROR("Failed to obtain mapped memory for socket: {}", ec);
                return ec;
            });
        })
        .map([this](void* ring_addr) {
            this->total_block_num_ = this->ring_.req.tp_block_nr;
            this->ring_.map = static_cast<std::uint8_t*>(ring_addr);
            /* allocate space for block views (io_vec*) into start of each block in ring; 
                therefore number of views needed equals the total number of blocks */
            this->ring_.rd = static_cast<iovec *>(
                std::malloc(this->ring_.req.tp_block_nr * sizeof(*(this->ring_.rd))));
            assert(this->ring_.rd); 

            /* each block view points to the start of each block; 
                the view shall cover the entire pointed-to block */
            for (unsigned int i = 0; i < this->ring_.req.tp_block_nr; ++i) {
                this->ring_.rd[i].iov_base = this->ring_.map + (i * this->ring_.req.tp_block_size);
                this->ring_.rd[i].iov_len = this->ring_.req.tp_block_size;
            }
        });
}


template <typename Protocol, class Executor>
inline auto BasicPacketSocket<Protocol, Executor>::
Open(details::PacketMmapMode mode) noexcept -> Result<void>
{
    return Base::Open()
        .and_then([this, mode](){
            ASRT_LOG_TRACE("Base socket open success, fd {}",
                Base::GetNativeHandle());
            return (mode == details::PacketMmapMode::kDisabled) ?
                Result<void>{} :
                this->SetupPacketMmap(); /* mmap() and setup mapped memory */
        })
        .map([this, mode](){
            this->packet_mmap_mode_ = mode;
            ASRT_LOG_TRACE("Packet socket mmap mode {}", ToStringView(this->packet_mmap_mode_));

            if(Base::HasReactor()){
                Base::ChangeReactorObservation(EventType::kEdge, false); /* use level-triggered semantics */
                ASRT_LOG_TRACE("Using level triggered reactor semantics");
            }
        });
}

template <typename Protocol, class Executor>
BasicPacketSocket<Protocol, Executor>::
BasicPacketSocket(BasicPacketSocket&& other) noexcept
{
    if(this->IsAsyncInProgress() || other.IsAsyncInProgress()){
        LogFatalAndAbort("[PacketSoceket]: Trying to move socket when asynchronous operations are in progress");
    }

    Base::MoveSocketFrom(std::move(other)); /* close socket and transfer reactor here */

    this->send_ongoing_ = false;
    this->recv_ongoing_ = false;

    /* No need to move additional members. 
    They are only valid during an ongoing asynchronous operation. */ 
}

template <typename Protocol, class Executor>
inline void BasicPacketSocket<Protocol, Executor>::
OnCloseEvent() noexcept
{
    ASRT_LOG_TRACE("[PacketSoceket]: received close event");
    this->recv_ongoing_ = false;
    this->send_ongoing_ = false;
    this->packet_mmap_mode_ = details::PacketMmapMode::kDisabled;
    this->total_block_num_ = 0;
    this->current_block_index_ = 0;
    if(this->ring_.rd){
        std::free(this->ring_.rd);
    }
    this->ring_ = {};
}

template <typename Protocol, class Executor>
inline auto BasicPacketSocket<Protocol, Executor>::
BindToEndpointImpl(const EndPointType& ep) noexcept -> Result<void>
{
    return OsAbstraction::GetNetIfIndex(ep.IfName(), this->GetNativeHandle())        
        .map_error([](SockErrorCode ec){
            ASRT_LOG_ERROR("[PacketSoceket]: Unable to get interface index! Error: {}", ec);
            return SockErrorCode::unable_to_obtain_if_index;
        })
        .and_then([this, &ep](int index) {
            auto addr{Types::MakePacketSockAddr(index, ep.EtherProto())};
            return OsAbstraction::Bind(this->GetNativeHandle(), Types::ConstPacketSockAddrView{&addr});
        })
        .map([&ep](){
            ASRT_LOG_TRACE("Sucessfully bound to endpoint {}", ep);
        });
}

template <typename Protocol, class Executor>
inline void BasicPacketSocket<Protocol, Executor>::
OnReactorEventImpl(Events ev, std::unique_lock<MutexType>& lock) noexcept
{
    assert(lock.owns_lock()); //asert lock held by reactor thread
    ASRT_LOG_TRACE("[PacketSocket]: OnReactorEvent()");

    if(ev.HasWriteEvent()){
        /* only perform io if we are in the middle of an asynchronous operation 
            since we may receive events that we never registered for */
        if(this->send_ongoing_)
            this->HandleSend(lock);
        else [[unlikely]]{
            Base::OnReactorEventIgnored(EventType::kWrite);
            ASRT_LOG_INFO("[PacketSoceket]: Ignored socket write event");
        }
    }

    if(ev.HasReadEvent()) [[likely]] {
        if(this->recv_ongoing_) [[likely]] {
            if(this->IsPacketMmapEnabled()) //todo constexpr if ? 
                this->HandleBlockReceive(lock);
            else
                this->HandleReceive(lock);
        }else [[unlikely]] {
            Base::OnReactorEventIgnored(EventType::kRead);
            ASRT_LOG_INFO("[PacketSoceket]: Ignored socket read event");
        }
            
    }
}

template <typename Protocol, class Executor>
inline void BasicPacketSocket<Protocol, Executor>::
HandleSend(std::unique_lock<MutexType>& lock) noexcept
{
    ASRT_LOG_DEBUG("[PacketSoceket]: Detected write event");

    auto send_result{OsAbstraction::Send(Base::GetNativeHandle(), this->send_buffview_, 0)}; //todo: check if flag param need to be filled out here
    auto handler_backup{std::move(this->send_completion_handler_)};
    this->send_ongoing_ = false;

    lock.unlock();
    handler_backup(std::move(send_result));     /* call completion callbck */
    lock.lock();
}

template <typename Protocol, class Executor>
inline void BasicPacketSocket<Protocol, Executor>::
HandleReceive(std::unique_lock<MutexType>& lock) noexcept
{
    ASRT_LOG_DEBUG("[PacketSoceket]: Detected read event");

    auto recv_result{OsAbstraction::Receive(Base::GetNativeHandle(), this->recv_buffview_, 0)}; //todo: check if flag param need to be filled out here
    auto handler_backup{std::move(this->recv_completion_handler_)};
    this->recv_ongoing_ = false;

    /* call completion callbck */
    lock.unlock();
    handler_backup(std::move(recv_result));
    lock.lock();
}

template <typename Protocol, class Executor>
inline void BasicPacketSocket<Protocol, Executor>::
HandleBlockReceive(std::unique_lock<MutexType>& lock) noexcept
{
    //assert(this->Base::IsLockHeld()); //assumes lock acquisition on entry
    assert(this->IsPacketMmapEnabled());

    details::MmapRingBlockExp block{this->GetCurrentBlockUnsafe()};
    
    ASRT_LOG_DEBUG("[PacketSoceket]: Detected block read event, packets in block {}", 
        block.size());

    if(block.IsReady()) [[likely]] { /* block ready to be read by user */
        auto handler_backup{std::move(this->recv_block_completion_handler_)};
        this->recv_ongoing_ = false;

        if(this->IncrementAndCheckNextBlockAvailable()){
            this->packet_mmap_block_read_pending_ = true;
        }

        /* call completion callbck */
        lock.unlock();
        handler_backup(block);
        block.Consume();
        lock.lock();

    }else [[unlikely]] {
        ASRT_LOG_INFO("[PacketSoceket]: False wakeup, re-submitting read request");

        /* the receive event was not actually handled 
            therefore we re-initiate an async read request */
        Base::AsyncReadOperationStarted(); 
    }
}

template <typename Protocol, class Executor>
inline auto BasicPacketSocket<Protocol, Executor>::
CheckRecvPossible() const noexcept -> Result<void> 
{
    return this->CheckSocketOpen()
        .and_then([this](){
            return this->recv_ongoing_ ?
                MakeUnexpected(SockErrorCode::receive_operation_ongoing) :
                Result<void>{};
        });
}

template <typename Protocol, class Executor>
inline auto BasicPacketSocket<Protocol, Executor>::
CheckSendPossible() const noexcept -> Result<void> 
{
    return this->CheckSocketOpen()
        .and_then([this](){
            return this->send_ongoing_ ?
                MakeUnexpected(SockErrorCode::receive_operation_ongoing) :
                Result<void>{};
        });
}

template <typename Protocol, class Executor>
inline auto BasicPacketSocket<Protocol, Executor>::
SendSome(Buffer::ConstBufferView buffer_view) noexcept -> Result<std::size_t>
{
    auto pre_send_check_result{this->CheckSendPossible()};
    
    if(pre_send_check_result.has_value())
    {
        return 
            OsAbstraction::Send(Base::GetNativeHandle(), buffer_view, 0)
            .or_else([this](SockErrorCode ec) -> Result<std::size_t> {
                if(Base::IsNonBlocking() ||
                    (ec != SockErrorCode::try_again && ec != SockErrorCode::would_block))
                    return 0;
                else
                /* received wouldblock on a blocking socket (how is this possible??)*/
                    return MakeUnexpected(SockErrorCode::default_error);
                    //return OsAbstraction::PollWrite(Base::GetNativeHandle(), -1);
        });
    }
    else
    {
        return MakeUnexpected(pre_send_check_result.error());
    }

}

template <typename Protocol, class Executor>
inline auto BasicPacketSocket<Protocol, Executor>::
ReceiveSome(Buffer::MutableBufferView buffer_view) noexcept -> Result<std::size_t>
{
    std::scoped_lock const lock{Base::GetMutex()};
    return this->CheckRecvPossible()
        .and_then([this, buffer_view]() -> Result<std::size_t> {
            return OsAbstraction::Receive(Base::GetNativeHandle(), buffer_view); /* //todo this call may block. unlock before call? */
        })
        .or_else([this](SockErrorCode ec) -> Result<std::size_t> {
            if(Base::IsNonBlocking() ||
                (ec != SockErrorCode::try_again && ec != SockErrorCode::would_block))
                return 0;
            else
            /* received wouldblock on a blocking socket (how is this possible??)*/
                return MakeUnexpected(ec);
                //return OsAbstraction::PollWrite(Base::GetNativeHandle(), -1);
        });
}

template <typename Protocol, class Executor>
template <typename ReceiveCompletionHandler>
inline auto BasicPacketSocket<Protocol, Executor>::
ReceiveAsync(Buffer::MutableBufferView buffer_view, ReceiveCompletionHandler&& handler) noexcept -> Result<void>
{
    std::scoped_lock const lock{Base::GetMutexUnsafe()};
    ASRT_LOG_DEBUG("[PacketSoceket]: {} function entry", __func__);
    assert(Base::IsAsyncPreconditionsMet());

    return this->CheckRecvPossible()
        .map([this, buffer_view, &handler](){
            this->recv_ongoing_ = true;
            this->recv_buffview_ = buffer_view;
            this->recv_completion_handler_ = std::move(handler);        
            this->Base::AsyncReadOperationStarted();
        });
}

template <typename Protocol, class Executor>
template <typename ReceiveBlockCompletionHandler>
inline auto BasicPacketSocket<Protocol, Executor>::
ReceiveBlockAsync(ReceiveBlockCompletionHandler&& handler) noexcept -> Result<void>
{
    ASRT_LOG_DEBUG("[PacketSoceket]: Submitting new receive block async request");
    std::scoped_lock const lock{Base::GetMutexUnsafe()};
    assert(Base::IsAsyncPreconditionsMet());

    if(this->packet_mmap_block_read_pending_){
        ASRT_LOG_TRACE("Read pending");
        this->packet_mmap_block_read_pending_ = false;
        Base::PostImmediateExecutorJob(
            [handler = std::move(handler), block = this->GetCurrentBlockUnsafe()](){
                handler(block);
            });
        return Result<void>{};    
    }

    return this->CheckRecvPossible()
        .map([this, &handler](){
            this->recv_ongoing_ = true;
            this->recv_block_completion_handler_ = std::move(handler);        
            Base::AsyncReadOperationStarted();
        });
}

template <typename Protocol, class Executor>
template <typename ReceiveBlockCompletionHandler>
inline auto BasicPacketSocket<Protocol, Executor>::
PollReceiveBlockSync(ReceiveBlockCompletionHandler&& handler, std::stop_token stoken) noexcept -> Result<void>
{
    ASRT_LOG_DEBUG("[PacketSoceket]: PollReceiveBlockSync entry");
    std::unique_lock lock{Base::GetMutex()};

    if(auto recv_possible{this->CheckRecvPossible()}; !recv_possible) [[unlikely]]
        return recv_possible;

    this->recv_ongoing_ = true;
    lock.unlock();

    auto eventfd_result{OsAbstraction::Eventfd(0, EFD_CLOEXEC)};
    if(!eventfd_result.has_value()) [[unlikely]]
        return MakeUnexpected(eventfd_result.error());

    auto eventfd{eventfd_result.value()};
    
    std::stop_callback cb{stoken,
        [eventfd](){
            if(::eventfd_write(eventfd, 1) == -1) [[unlikely]]
                ASRT_LOG_ERROR("Failed to write event fd");
        }};

    Result<void> result{};
    while(!stoken.stop_requested()) [[likely]] {
        details::MmapRingBlockExp block{this->GetCurrentBlockUnsafe()};

        if(!block.IsReady()) { 
            auto poll_res{OsAbstraction::PollRead(Base::GetNativeHandle(), eventfd, -1)};
            if(!poll_res.has_value()) [[unlikely]] {
                ASRT_LOG_ERROR("Poll error {}", poll_res.error());
                result = poll_res;
                break;
            }
            continue;
        }
 
        handler(block);
        block.Consume();
        this->IncrementBlockIndex();
    }

    (void)OsAbstraction::Close(eventfd);
    ASRT_LOG_DEBUG("Stopped PollReceiveBlockSync()");

    lock.lock();
    this->recv_ongoing_ = false;
    return result;
}

















}
