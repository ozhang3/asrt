#ifndef B5CDC4CB_5334_4774_987C_9E30A26E197B
#define B5CDC4CB_5334_4774_987C_9E30A26E197B

namespace ClientServer{
namespace internal{

template <typename Owner>
    requires requires (Owner& o) {o.OnMessage();}
struct ServerMessageHandler{
    explicit ServerMessageHandler(Owner& owner) noexcept
        : owner_{owner} {}
    
    template <typename Client, typename Message>
    void operator()(Client client, Message&& message) noexcept
    {
        owner_.OnMessage(client, std::move(message));
    }

private:
    Owner& owner_;
};

}
}

#endif /* B5CDC4CB_5334_4774_987C_9E30A26E197B */
