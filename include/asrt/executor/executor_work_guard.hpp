#ifndef C7AA1805_C928_42C3_BC4C_FA8FC67253B4
#define C7AA1805_C928_42C3_BC4C_FA8FC67253B4

namespace ExecutorNS{

template <typename Executor>
    requires requires (Executor& e) 
        {e.OnJobArrival(); e.OnJobCompletion();}
struct WorkGuard{
    explicit WorkGuard(Executor& ex) noexcept
        : ex_{ex} {ex.OnJobArrival();}

    ~WorkGuard() noexcept {ex_.OnJobCompletion();}
private:
    Executor& ex_;
};

}
#endif /* C7AA1805_C928_42C3_BC4C_FA8FC67253B4 */
