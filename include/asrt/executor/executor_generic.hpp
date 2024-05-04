#ifndef C180FFF1_446F_4789_80BC_978C442AE0E0
#define C180FFF1_446F_4789_80BC_978C442AE0E0

#include <deque>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>

namespace ExecutorNS{


class GenericExecutor
{

public: 
    using Operation = std::function<void()>;

    template <typename Executable>
    void Post(Executable&& op)
    {
        std::scoped_lock const lock{this->mtx_};
        this->operation_queue_.push_back(std::move(op));
        std::cout << "enqueued operation\n";
        this->cv_.notify_one(); //todo notify_one or notify_all?
    }

    void Run(void)
    {
        while(!this->exit_requested_){
            Operation op; 
            {
                std::unique_lock<std::mutex> lock{this->mtx_};

                this->cv_.wait(lock, [this](){
                    return !this->operation_queue_.empty() || this->exit_requested_ ;
                });

                if(!exit_requested_){
                    op = std::move(this->operation_queue_.front());
                    this->operation_queue_.pop_front();
                }
            }

            if(op){
                std::cout << "calling operation...\n";
                op();
            }else{
                std::cout << "dequeued empty operation\n";
                this->TriggerShutdown();
                return; //exit loop
                //todo
            }
        }

        std::cout << "exiting...\n";
    }
    

    void Shutdown()
    {
        std::cout << "shutting down...\n";

        this->TriggerShutdown();
    }

    void Stop()
    {
        std::cout << "stopping...\n";
        this->exit_requested_ = true;
        this->cv_.notify_all(); //todo notify_one or notify_all?
    }

private:

    void TriggerShutdown()
    {
        this->Post(nullptr);
    }

    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic_bool exit_requested_{false};
    std::deque<Operation> operation_queue_;
    
};



}



#endif /* C180FFF1_446F_4789_80BC_978C442AE0E0 */
