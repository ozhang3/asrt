#ifndef B3F0A5C8_285B_45C9_8CA2_F84F09A338A8
#define B3F0A5C8_285B_45C9_8CA2_F84F09A338A8

namespace ExecutorNS{

    template <typename Executor>
    class Executable{
    public:
        Executable() noexcept {}
        virtual ~Executable() = default;
        
        virtual void Run() = 0;
        virtual void Stop() = 0;

    protected:
        auto GetExecutor() ->Executor& {return this->executor_;}
    private:
        Executor executor_;
    };
}

#endif /* B3F0A5C8_285B_45C9_8CA2_F84F09A338A8 */
