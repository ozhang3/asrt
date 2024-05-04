#ifndef BE71FAA4_3EB0_4AE5_B32F_8CEAD99048B5
#define BE71FAA4_3EB0_4AE5_B32F_8CEAD99048B5

namespace ClientServer{
namespace internal{

template <typename Owner>
    requires requires (Owner& o) {o.OnMessage();}
struct ClientMessageHandler{
    explicit ClientMessageHandler(Owner& owner) noexcept
        : owner_{owner} {}
    
    template <typename Message>
    void operator()(Message&& message) noexcept
    {
        owner_.OnMessage(std::move(message));
    }

private:
    Owner& owner_;
};

}
}

#endif /* BE71FAA4_3EB0_4AE5_B32F_8CEAD99048B5 */
