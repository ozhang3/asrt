#ifndef B4265F04_04CF_40E7_94C3_CC5FA4199BD5
#define B4265F04_04CF_40E7_94C3_CC5FA4199BD5

#include <cstdint>
#include <memory>
#include <atomic>
#include <random>
#include <chrono>
#include <type_traits>
#include <concepts>
#include <span>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

#include "asrt/common_types.hpp"
#include "asrt/util.hpp"
#include "asrt/concepts.hpp"
#include "asrt/client_server/message_queue.hpp"
#include "asrt/netbuffer.hpp"
#include "asrt/error_code.hpp"
#include "asrt/socket/address_types.hpp"
#include "asrt/executor/strand.hpp"

namespace ClientServer
{   
    using namespace Util::Expected_NS;
    using namespace std::chrono_literals;
    using namespace AddressTypes;

    template <typename Callback, typename Message>
    concept OnClientMessageCallbackType = std::invocable<Callback&, Message>;

    template <typename Callback, typename Client, typename Message>
    concept OnServerMessageCallbackType = std::invocable<Callback&, Client, Message>;

    template <typename Message>
    concept HasHeader = requires(Message& m) {m.HeaderView(); m.BodyView();};

    // template <typename Processor, typename Message>
    // concept MessageProcessor = requires(Processor processor, Message&& message) {processor.OnMessage(message);};

    enum class Identity : std::uint8_t
    {
        kServer,
        kClient
    };

    template<Identity>
    struct is_client : public std::false_type {};
    template<Identity>
    struct is_server : public std::false_type {};

    template<> struct is_server<Identity::kServer> : public std::true_type {};
    template<> struct is_client<Identity::kClient> : public std::true_type {};

    template<Identity identity>
    inline constexpr bool is_server_v = is_server<identity>::value;
    template<Identity identity>
    inline constexpr bool is_client_v = is_client<identity>::value;


    namespace internal{
        /**
         * @brief Just an empty message handler
         * 
         */
        template <Identity OwnerIdentity>
        struct DefaultOnMessageCallback{
            constexpr DefaultOnMessageCallback() noexcept = default;

            template <
                typename Message,
                typename = typename std::enable_if_t<is_client_v<OwnerIdentity>>>
            void operator()(Message&&) const noexcept
                {ASRT_LOG_TRACE("Calling default client message handler");}

            
            template <
                typename Client,
                typename Message,
                typename = typename std::enable_if_t<is_server_v<OwnerIdentity>>>
            void operator()(Client, Message&&) const noexcept
                {ASRT_LOG_TRACE("Calling default server message handler");}
        };

        inline constexpr std::size_t ComputeKey(std::size_t seed) noexcept
        {
            std::size_t temp{seed ^ 0xDEADBEEF};
            temp = (temp & 0xF0F0F0F0) >> 4 | (temp & 0x0F0F0F0F) << 4;
            return temp ^ 0xFACE6666;
        }

        //constexpr auto a = ComputeKey(0xFFFF);
    }

    template<
        typename Executor,
        StreamBased Protocol,
        HasHeader Message,
        typename ConnectionOwner> //eg: server/client-side connection
    //requires requires (ConnectionOwner& o) {ConnectionOwner::Identity() -> Identity; o.OnMessage(); o.RetrieveInbox();}
    class Connection : public std::enable_shared_from_this<Connection<Executor, Protocol, Message, ConnectionOwner>>
    {
    public:

        using ExecutorType      = Executor;
        using ProtocolType      = Protocol;
        using MessageType       = Message;
        using ConstMessageView  = std::span<std::uint8_t const>;
        using MutableMessageView = std::span<std::uint8_t>;

        using ConnectionIdType  = std::uint32_t;
        using Socket            = typename Protocol::template DataTransferSocketType<Executor>;
        using Endpoint          = typename Protocol::Endpoint;
        using Outbox            = std::deque<Message>;
        using Duration          = std::chrono::microseconds;
        using ConnectionType    = Connection;
        using MessageSource     = std::shared_ptr<Connection>;
        using Strand            = ExecutorNS::Strand<Executor>;

        struct IncomingMessage{
            MessageSource source_{this->shared_from_this()};
            Message msg_;
        };

        using Inbox = ThreadSafeQueue<IncomingMessage, typename Executor::MutexType>;

    public:
        
        /**
         * @brief Construct a new Connection
         * 
         * @param executor used to execute message handlers
         * @param owner owner of this connection
         * @param conn_id an optional id assigned to this connection
         */
        Connection(Executor& executor, ConnectionOwner& owner, ConnectionIdType conn_id = 0) noexcept 
            : executor_{executor}, 
              strand_{executor}, 
              socket_{executor},
              owner_{owner},
              inbox_{owner.RetrieveInbox()},
              conn_id_{conn_id} 
        {
            ASRT_LOG_TRACE("{} connection constructed with id {}", ConnectionIdentity(), conn_id);

            if constexpr (IsClient()) {
                this->socket_.Open().map_error([](auto error){
                    LogFatalAndAbort("Unable to open client socket required for this connection, {}", error);
                });
            }
            
            this->PrepareAuthInfo();
        }

        Connection(Connection const&) = delete;
        Connection(Connection&&) = delete;
        Connection &operator=(Connection const &other) = delete;
        Connection &operator=(Connection &&other) = delete;

        ~Connection() noexcept
        {
            ASRT_LOG_TRACE("Connection {} destruction", GetId());
            this->socket_.Close();
        }

        static constexpr Identity OwnerIdentity() noexcept {return ConnectionOwner::Identity();}

        static std::shared_ptr<Connection> Create(
            Executor& executor, 
            ConnectionOwner& owner, 
            ConnectionIdType conn_id = 0) noexcept
        {
            return std::make_shared<Connection>(executor, owner, conn_id);
        }

        ConnectionIdType GetId() const noexcept {return conn_id_;}

        Executor& GetExecutor() const noexcept {return executor_;}
        
        void SendSync(ConstMessageView message_view) noexcept
        {
            ASRT_LOG_TRACE("Connection SendSync: {}", spdlog::to_hex(message_view));

            if(not this->is_connection_validated_) [[unlikely]] {
                ASRT_LOG_TRACE("Saved SendSync message while connection is being validated");
                //todo this is not thread safe what if there's an ongoing SendAsync operation?
                this->outbox_.emplace_back(message_view);
                return;
            }

            this->socket_.SendSync(message_view)
            .map_error([this, message_view](Socket::SockErrorCode ec){
                if constexpr (IsClient()){
                    if(ErrorCode_Ns::IsUnconnected(ec)) [[likely]] {
                        ASRT_LOG_INFO(
                            "Failed to send message: server unreachable. Retrying when connected");
                        this->outbox_.emplace_back(message_view);
                        return;
                    }
                }

                ASRT_LOG_ERROR(
                    "SendSync to client failed with {}, closing socket", ec);
                this->Close();
            });
        }

        void Send(const Message& message) noexcept
        {
            //todo needs to implement TrySend so that 
            //todo we can accept message view. This way the message doesn't have to be uneccessarily copied
            this->strand_.Dispatch(
                [self = this->shared_from_this(), message](){
                    self->DoSendMessage2(message);
                });
        }

        void Close() noexcept
        {
            this->strand_.Dispatch(
                [self = this->shared_from_this()](){
                    self->socket_.Close();
                });
        }

        bool IsConnected() const noexcept
        {
            return IsSocketOpen() &&\
                this->IsConnectionValidated();
        }

        /* Client side APIs */
        void ConnectToServer(const Endpoint& server, Duration retry_period = 5s) noexcept
        {
            ASRT_LOG_TRACE("Connecting to server");
            static_assert(IsClient(), "API for client use only!");

            this->socket_.ConnectAsync(server, 
                [self = this->shared_from_this(), server, retry_period](auto connect_result){
                    if(connect_result.has_value()){
                        spdlog::info("Connected to server {}", server);
                        self->InitiateHandshake();
                    }else{
                        ASRT_LOG_INFO("Unable to connect to server, retrying in {}s", 
                            std::chrono::duration_cast<std::chrono::seconds>(retry_period).count());
                        self->executor_.PostDeferred(retry_period,
                            [self, server, retry_period](){
                                self->ConnectToServer(server, retry_period);
                            });
                    }
                });
        }

        void InitiateHandshake() noexcept
        {
            if(IsSocketOpen()) [[likely]] {
                if constexpr (IsServer())
                    this->WriteSeed();
                else if constexpr (IsClient())
                    this->ReadSeed();
            }else [[unlikely]] {
                ASRT_LOG_ERROR("Server disconnected. Aborting handshake.");
                this->socket_.Close();
            }
        }


        Socket& GetSocket() noexcept
        {
            return this->socket_;
        }

        bool operator==(Connection const &other) const noexcept
        {
            //todo as it stands all connections without id assigned will compare equal!
            return this->conn_id_ == other.conn_id_;
        }
        
        auto operator<=>(Connection const &other) const noexcept
        {
            return this->conn_id_ <=> other.conn_id_;
        }

    private:

        bool IsSocketOpen() const noexcept
        {
            return this->socket_.IsOpen();
        }

        auto PrepareAuthInfo() noexcept
        {
            if constexpr (IsServer()){
                std::size_t rd32{std::mt19937_64{std::random_device{}()}()};
                this->auth_seed_.FromHost(rd32);
                this->priv_key_ = internal::ComputeKey(rd32);
            }
        }

        auto DoSendMessage2(const Message& message) noexcept {
            ASRT_LOG_TRACE("Sending message...");
            const bool send_in_progress{!this->outbox_.empty()};
            this->outbox_.push_back(message);

            if(send_in_progress || !this->IsConnectionValidated()) [[unlikely]]
                return;

            this->DoSendNextMessage();
        }
        
        void DoSendNextMessage()  noexcept
        {
            ASRT_LOG_TRACE("Sending next message...");
            const auto& msg{this->outbox_.front()};
            this->socket_.SendAsync(msg.DataView(), 
                [this](auto send_result){
                    this->outbox_.pop_front();
                    if(send_result.has_value()) [[likely]] {
                        ASRT_LOG_TRACE("Sent sucess");
                        if(!this->outbox_.empty()){
                            this->strand_.Dispatch([this](){
                                this->DoSendNextMessage();
                            });
                        }
                    }else [[unlikely]] {
                        ASRT_LOG_ERROR("Failed to send message");
                        this->HandleCommunicationError(send_result.error());
                    }
                }); 
        }

        void SendBackloggedMessages() noexcept
        {
            ASRT_LOG_TRACE("Backlogged message(s) size {}", this->outbox_.size());
            std::for_each(this->outbox_.begin(), this->outbox_.end(),
                [this](auto& message){
                    ASRT_LOG_TRACE("Sending backlogged message {}", spdlog::to_hex(message));
                    this->socket_.SendSync(message.DataView())
                    .map_error([&message](Socket::SockErrorCode ec){
                        spdlog::warn("SendSync failed with {}, dropping backlogged message {}",
                            ec, spdlog::to_hex(message));
                    }); 
                });
            this->outbox_.clear();  
        }

        [[deprecated]]
        auto DoSendMessage(const Message& message){
            ASRT_LOG_TRACE("Sending message...");
            const bool send_ongoing{!this->outbox_.empty()};
            this->outbox_.EmplaceBack(message);
            if(!send_ongoing)
                this->SendMessageHeader();
        }

        [[deprecated]]
        void SendMessageHeader(){
            ASRT_LOG_TRACE("Sending message header.");
            const auto& msg{this->outbox_.front()};
            bool has_body{msg.HasBody()};
            this->socket_.SendAsync(msg.HeaderView(), 
                [self = this->shared_from_this(), has_body](auto send_result){
                    if(send_result.has_value()) [[likely]] {
                        if(has_body)
                            self->SendMessageBody();
                        else if(!self->outbox_.empty()){
                            self->SendMessageHeader();
                        }
                    }else [[unlikely]] {
                        ASRT_LOG_ERROR("Failed to send message header");
                        self->HandleCommunicationError(send_result.error());
                    }
                }); 
        };

        [[deprecated]]
        void SendMessageBody(){
            ASRT_LOG_TRACE("Sending message body.");
            const auto& msg{this->outbox_.front()};
            this->socket_.SendAsync(msg.BodyView(),
                [self = this->shared_from_this(), this](auto send_result){
                    if(send_result.has_value()) [[likely]] {
                        this->outbox_.pop_front(); /* send complete */
                        if(!this->outbox_.empty()){
                            this->SendMessageHeader(); /* send next message */
                        }
                    }else [[unlikely]] {
                        ASRT_LOG_ERROR("Failed to send message body");
                        this->HandleCommunicationError(send_result.error());
                    }
                });
        };
        
        [[deprecated]] /* impl not ready */
        void StartOfReceptionExperimental() noexcept{
            this->socket_.ReceiveSomeAsync(this->incoming_message_.DataView(),
            [self = this->shared_from_this(), this](auto recv_result){
                if(recv_result.has_value()) [[likely]] {
                    if(this->incoming_message_.CommitReception(recv_result.value())){
                        this->FinalizeReception();
                    }else{
                        this->StartOfReceptionExperimental();
                    }
                }else [[unlikely]] {
                    ASRT_LOG_ERROR("Failed to read message: {}, closing socket.",
                        recv_result.error());
                    this->HandleCommunicationError(recv_result.error());
                }
            });
        }

        void ReceiveMessageHeader() noexcept {

            ASRT_LOG_TRACE("Connection attempting to receive {} bytes of message header", 
                Message::HeaderLength());
            this->incoming_message_.Resize(Message::HeaderLength());
            this->socket_.ReceiveAsync(this->incoming_message_.HeaderView(), 
                [self = this->shared_from_this(), this](auto recv_result) -> void {
                    if(recv_result.has_value()) [[likely]] {
                        this->incoming_message_.CommitHeaderUpdate();
                        if(this->incoming_message_.HasBody() > 0){
                            this->ReceiveMessageBody();
                        }else{ /* received zero-body message */
                            ASRT_LOG_TRACE("Connection received header only message");
                            this->FinalizeReception();
                        }
                    }else [[unlikely]] {
                        ASRT_LOG_ERROR("Failed to read message header: {}, closing socket.",
                            recv_result.error());
                        this->HandleCommunicationError(recv_result.error());
                    }
                }); 
        }

        void ReceiveMessageBody() noexcept {
            
            //todo there is no need to break the receive up into two parts!!
            //todo we should receive as much as we can and check if we received everything based on the length info contained in the message
            //todo consider switching to receive some rather than receive

            ASRT_LOG_TRACE("Receiving message body, size {:#x}", incoming_message_.BodyLength());
            //const auto& header{this->outbox_.front().header};
            this->socket_.ReceiveAsync(this->incoming_message_.BodyView(), 
                [self = this->shared_from_this(), this](auto recv_result) -> void {
                    if(recv_result.has_value()) [[likely]] {
                        this->FinalizeReception();
                    }else [[unlikely]] {
                        ASRT_LOG_ERROR("[Connection]: Failed to read message body: {}, closing socket.",
                            recv_result.error());
                        this->HandleCommunicationError(recv_result.error());
                    }
                }); 
        }

        void FinalizeReception() noexcept
        {
            //todo can we get rid of the branching here
            if(inbox_){
                this->inbox_->EmplaceBack(
                    {this->shared_from_this(), std::move(this->incoming_message_)});
                ASRT_LOG_TRACE("Enqueued message {}", spdlog::to_hex(this->incoming_message_));
            }else{
                ASRT_LOG_TRACE("Delivering message {}", spdlog::to_hex(this->incoming_message_));
                if constexpr (IsServer())
                    this->owner_.OnMessage(
                        this->shared_from_this(), this->incoming_message_);
                else
                    this->owner_.OnMessage(this->incoming_message_);
            }
            ASRT_LOG_TRACE("Connection {} message to {}",
                inbox_ ? "enqueued" : "delivered",
                IsClient() ? "client" : "server");
            this->incoming_message_.Clear();

            this->ReceiveMessageHeader(); //prepare next read
        }

        void HandleCommunicationError(ErrorCode_Ns::ErrorCode ec) noexcept
        {
            if constexpr (IsServer()){
                ASRT_LOG_TRACE("Notifying server of connection {} error {}", GetId(), ec);
                this->owner_.OnConnectionError(this->shared_from_this(), ec);
            } else if constexpr (IsClient()){
                ASRT_LOG_TRACE("Notifying client of connection error {}", ec);
                this->owner_.OnConnectionError(ec);
            }
          
            this->socket_.Close();   //todo          
        }

        void WriteSeed() noexcept
        {
            static_assert(IsServer(), "API for server use only!");
            ASRT_LOG_TRACE("Server sending auth seed {:#0x}", this->auth_seed_.ToHost());

            this->socket_.SendSync(
                Buffer::make_buffer(this->auth_seed_.data(), this->auth_seed_.size()))
            .map([this](){
                this->ReadKey();
            })
            .map_error([this](Socket::SockErrorCode ec){
                ASRT_LOG_ERROR("Failed to send auth seed: {}, closing socket", ec);
                this->HandleCommunicationError(ec);
            });
        }

        void ReadSeed() noexcept
        {
            static_assert(IsClient(), "API for client use only!");
            ASRT_LOG_TRACE("Client reading auth seed");
            this->socket_.ReceiveAsync(
                Buffer::make_buffer(this->auth_seed_.data(), this->auth_seed_.size()),
                [self = this->shared_from_this(), this](auto recv_result){
                    if(recv_result.has_value()) [[likely]] {
                        this->WriteKey();
                    }else{
                        ASRT_LOG_ERROR("Failed to read auth seed: {}",
                            recv_result.error());
                        this->HandleCommunicationError(recv_result.error());
                    }
                });
        }

        void WriteKey() noexcept
        {
            static_assert(IsClient(), "API for client use only!");
            this->auth_key_.FromHost(internal::ComputeKey(this->auth_seed_.ToHost()));
            ASRT_LOG_TRACE("Client sending auth key {:#0x}", this->auth_key_.ToHost());

            this->socket_.SendSync(
                Buffer::make_buffer(this->auth_key_.data(), this->auth_key_.size()))
            .map([this](){
                    this->SetConnectionValidated();
                    this->SendBackloggedMessages(); //todo
                    this->ReceiveMessageHeader();
            })
            .map_error([this](Socket::SockErrorCode ec){
                ASRT_LOG_ERROR("Failed to send auth key, {}", ec);
                this->HandleCommunicationError(ec);
            });
        }

        void ReadKey() noexcept
        {
            static_assert(IsServer(), "API for server use only!");
            ASRT_LOG_TRACE("Server reading auth key");
            this->socket_.ReceiveAsync(
                Buffer::make_buffer(this->auth_key_.data(), this->auth_key_.size()),
                [this, self = this->shared_from_this()](auto recv_result) mutable {
                    if(recv_result.has_value()) [[likely]] {
                        if(this->auth_key_.ToHost() == this->priv_key_){
                            ASRT_LOG_TRACE("Auth key validation success");
                            this->SetConnectionValidated();
                            this->SendBackloggedMessages();
                            this->owner_.OnClientValidated(self);
                            this->ReceiveMessageHeader();
                        }else{
                            using enum ErrorCode_Ns::ErrorCode;
                            ASRT_LOG_TRACE("Auth key validation error (received {:#0x}, expecting {:#0x}), closing socket.",
                                this->auth_key_.ToHost(), this->priv_key_);
                            this->owner_.OnConnectionError(self, connection_authentication_failed);
                            this->socket_.Close();
                        }
                    }else [[unlikely]] {
                        ASRT_LOG_ERROR("Failed to read auth key: {}, closing socket.",
                            recv_result.error());
                        this->HandleCommunicationError(recv_result.error());
                    }
                });
        }

        void SetConnectionValidated() noexcept
        {
            this->is_connection_validated_.store(true, std::memory_order::release);
            this->is_connection_validated_cached_ = true;
        }

        bool IsConnectionValidated() const noexcept
        {
            if(this->is_connection_validated_cached_ == false) [[unlikely]] {
                return this->is_connection_validated_.load(std::memory_order::acquire);
            }
            return this->is_connection_validated_cached_;
        }

        static constexpr bool IsClient() noexcept {return OwnerIdentity() == Identity::kClient;}
        static constexpr bool IsServer() noexcept {return OwnerIdentity() == Identity::kServer;}
        static constexpr const char * ConnectionIdentity() noexcept {
            if constexpr (IsClient()) {
                return "C2S";
            }else{
                return "S2C";
            }
        }
        //template <typename T> using Optional = Util::Optional_NS::Optional<T>;
        //using ConnectionId = std::size_t; 

        Executor& executor_;
        Strand strand_;
        Socket socket_{};
        ConnectionOwner& owner_;
        Inbox* inbox_;
        Outbox outbox_;
        Message incoming_message_{};

        NetworkOrder<std::size_t> auth_seed_;
        NetworkOrder<std::size_t> auth_key_;
        std::size_t priv_key_{};
        bool is_connection_validated_cached_{false};
        std::atomic_bool is_connection_validated_{false};
        ConnectionIdType conn_id_;
    };

} //end ns

#endif /* B4265F04_04CF_40E7_94C3_CC5FA4199BD5 */
