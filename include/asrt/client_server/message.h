#ifndef FD334935_482A_4A09_98EF_2C197F06DB6B
#define FD334935_482A_4A09_98EF_2C197F06DB6B

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <string_view>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/core.h>

#include "asrt/netbuffer.hpp"
#include "asrt/socket/address_types.hpp"

namespace ClientServer
{

using namespace AddressTypes;
using namespace std::literals;

#if 0
struct [[using gnu: packed]] test{
    std::uint16_t a;
    std::uint8_t b;
};
constexpr auto t{sizeof(struct test)};
#endif

enum class MessageType : std::uint8_t
{
    kReserved = 0,
    kDummy1 = 1,
    kDummy2 = 2,
    kMax
};

constexpr auto ToUnderlying(MessageType type)
{
    return static_cast<std::underlying_type_t<MessageType>>(type);
}

static constexpr const char* kMessageTypeStrs[ToUnderlying(MessageType::kMax)]
{
    "Reserved",
    "Dummy 1",
    "Dummy 2"
};

inline auto ToStringView(MessageType type) -> std::string_view
{
    return kMessageTypeStrs[ToUnderlying(type)];
}

inline auto ToString(MessageType type) -> std::string
{
    std::string printable;
    switch (type)
    {
    case MessageType::kDummy1:
        printable = "Dummy1";
        break;
    case MessageType::kDummy2:
        printable = "Dummy2";
        break;
    default:
        printable = "InvalidType";
        break;
    }
    return printable;
}

std::ostream& operator<<(std::ostream& os, const MessageType& type)
{
    os << ToString(type);
    return os;
}

struct MessageHeader2
{
    MessageType type_;
    NetworkOrder<std::uint32_t> body_length_; //always stored in network order

    constexpr MessageType Type() const
    {
        return this->type_;
    }

    /* return header length in host byte order */
    constexpr std::size_t Length() const
    {
        return static_cast<std::size_t>(this->body_length_);
    }

    auto DataView() -> Buffer::MutableBufferView
    {
        return Buffer::make_buffer(&this->type_, sizeof(MessageType));
    }
};


union MessageHeader
{
    static constexpr std::size_t header_length_{5u};
    struct{
        NetworkOrder<std::uint32_t> type_;
        NetworkOrder<std::uint32_t> body_length_;
    }fields_;
    std::uint8_t data_[header_length_]{};

    auto DataView() -> Buffer::MutableBufferView
    {
        return Buffer::make_buffer(this->data_);
    }

    auto DataView() const -> Buffer::ConstBufferView
    {
        return Buffer::make_buffer(this->data_);
    }

    auto StringView() const -> std::string_view
    {
        return std::string_view{(char*)data_, header_length_};
    }
};

struct GenericMessage
{
    using MessageHeaderType = MessageHeader;
    using MsgType = MessageType;
    //static_assert(sizeof(MessageHeader) == MessageHeader::header_length_, "Misaligned header layout!");

    GenericMessage() noexcept = default;

    GenericMessage(MessageType type, const char* payload) noexcept
    {
        this->header_.fields_.type_ = static_cast<std::uint32_t>(type);
        auto payload_len{static_cast<std::uint32_t>(::strlen(payload))};
        this->header_.fields_.body_length_ = payload_len;
        this->body_.resize(payload_len);
        std::memcpy(this->body_.data(), payload, payload_len);
        ASRT_LOG_TRACE("Message construction, payload len: {} bytes", payload_len);
    } 

    MessageHeader header_{};
    std::vector<std::uint8_t> body_;

    auto Clear()
    {
        this->header_ = {};
        this->body_.clear();
    }

    auto size() const -> std::size_t
    {
        return body_.size();
    }

    auto Type() const -> std::uint8_t
    {
        return this->header_.fields_.type_;
    }

    bool HasBody() const
    {
        return this->BodyLength() != 0;
    }

    auto BodyLength() const -> std::size_t
    {
        return this->header_.fields_.body_length_;
    }

    auto Resize(std::size_t new_size)
    {
        this->body_.resize(new_size);
    }

    auto HeaderView() -> Buffer::MutableBufferView
    {
        return this->header_.DataView();
    }

    auto HeaderView() const -> Buffer::ConstBufferView
    {
        return this->header_.DataView();
    }

    auto BodyView() -> Buffer::MutableBufferView
    {
        return Buffer::make_buffer(this->body_);
    }

    auto BodyView() const -> Buffer::ConstBufferView
    {
        return Buffer::make_buffer(this->body_);
    }

    friend std::ostream& operator<<(std::ostream& os, const GenericMessage& msg)
    {
        os << "[MsgType]: " << msg.Type() <<
              "[MsgSize]: " << msg.BodyLength();
        return os;
    }
    
    std::string_view StringView() const {
         return std::string_view{(char*)body_.data(), body_.size()};
    }

    // template <typename DataType>
    // friend GenericMessage& operator<<(GenericMessage& msg, const DataType& data)
    // {

    // }
};

struct GenericMessage2
{
public:
    static constexpr std::uint8_t kDiagMsgHeaderLength{5u};
    static constexpr std::uint8_t kDiagMsgTypeOffset{0u};
    static constexpr std::uint8_t kDiagMsgBodyLenOffset{1u};

    using MessageHeaderType = MessageHeader;
    using MsgType = MessageType;
    //static_assert(sizeof(MessageHeader) == MessageHeader::header_length_, "Misaligned header layout!");

    GenericMessage2() noexcept = default;

    GenericMessage2(MessageType type, const char* payload) noexcept
        : type_{type}
    {
        this->payload_len_ = static_cast<std::uint32_t>(::strlen(payload) + 1u);
        const NetworkOrder<std::uint32_t> payload_len_net{this->payload_len_};

        this->data_.resize(kDiagMsgHeaderLength + this->payload_len_);
        std::memcpy(this->data_.data(), &type, sizeof(MessageType));
        std::memcpy(this->data_.data() + sizeof(MessageType), payload_len_net.data(), sizeof(payload_len_net));
        std::memcpy(this->data_.data() + kDiagMsgHeaderLength, payload, this->payload_len_);
        ASRT_LOG_TRACE("Message construction, total size {}, payload len: {} bytes", 
            this->data_.size(), this->payload_len_);
    }

    static constexpr auto 
    HeaderLength() noexcept
    {
        return MessageHeader::header_length_;
    }

    auto Clear()
    {
        this->data_.clear();
        //todo change payload_len_ ?
    }

    auto size() const -> std::size_t
    {
        return data_.size();
    }

    auto Type() const
    {
        return this->type_;
    }

    bool HasBody() const
    {
        return this->payload_len_ > 0;
    }

    void CommitHeaderUpdate()
    {
        if(this->data_.size() < kDiagMsgHeaderLength)
            return;

        NetworkOrder<std::uint32_t> payload_length;
        this->type_ = static_cast<MsgType>(this->data_[kDiagMsgTypeOffset]);
        std::memcpy(payload_length.data(), data_.data() + kDiagMsgBodyLenOffset, sizeof(this->payload_len_)); 
        this->payload_len_ = payload_length.ToHost();
    }

    auto BodyLength() const -> std::size_t
    {
        return this->payload_len_;
    }

    auto Resize(std::size_t new_size)
    {
        this->data_.resize(new_size);
    }

    auto HeaderView() -> Buffer::MutableBufferView
    {
        return {this->data_.data(), MessageHeader::header_length_};
    }

    auto HeaderView() const -> Buffer::ConstBufferView
    {
        return {this->data_.data(), MessageHeader::header_length_};
    }

    auto BodyView() -> Buffer::MutableBufferView
    {
        return {this->data_.data() + MessageHeader::header_length_, this->payload_len_};
    }

    auto BodyView() const -> Buffer::ConstBufferView
    {
        return {this->data_.data() + MessageHeader::header_length_, this->payload_len_};
    }
    
    auto DataView() const -> Buffer::ConstBufferView
    {
        return Buffer::make_buffer(this->data_);
    }

    friend std::ostream& operator<<(std::ostream& os, const GenericMessage2& msg)
    {
        os << "[MsgType]: " << msg.Type() <<
              "[MsgSize]: " << msg.BodyLength();
        return os;
    }
    
    std::string_view StringView() const {
        return (char*)data_.data() + MessageHeader::header_length_;
    }

private:
    std::vector<std::uint8_t> data_;
    MsgType type_{MsgType::kReserved};
    std::uint32_t payload_len_{};
};


}

template<>
struct fmt::formatter<ClientServer::GenericMessage> : fmt::formatter<std::string_view>
{
    auto format(ClientServer::GenericMessage& msg, format_context &ctx) const -> decltype(ctx.out())
    {
        return format_to(ctx.out(), "Header: {}", msg.header_.StringView(),
            "Body: {}", msg.StringView());
    }
};

template<>
struct fmt::formatter<ClientServer::GenericMessage2> : fmt::formatter<std::string_view>
{
    auto format(ClientServer::GenericMessage2& msg, format_context &ctx) const -> decltype(ctx.out())
    {
        return format_to(ctx.out(), "[{}]: {}",
            ClientServer::ToStringView(msg.Type()),
            msg.StringView());
    }
};

#endif /* FD334935_482A_4A09_98EF_2C197F06DB6B */
