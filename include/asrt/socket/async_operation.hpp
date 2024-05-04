#ifndef DFF39485_29D4_4653_9317_76E553235B43
#define DFF39485_29D4_4653_9317_76E553235B43

#include <cstdint>
#include <utility>
#include <functional>

#include "asrt/util.hpp"
#include "asrt/error_code.hpp"
#include "asrt/sys/syscall.hpp"
#include "asrt/socket/types.hpp"
#include "asrt/common_types.hpp"
#include "asrt/socket/socket_base.hpp"

namespace Socket{

    using namespace Socket::Types;

    using namespace Util::Expected_NS;

    using SockErrorCode = ErrorCode_Ns::ErrorCode;
    template <typename T> using Result = Expected<T, SockErrorCode>;

    /**
     * @brief 
     * 
     * @tparam OpType 
     * @tparam BufferView 
     * @tparam CompletionResult 
     * @tparam CompletionHandler 
     */
    template <
        OperationType OpType, 
        typename BufferView, 
        typename CompletionResult, 
        typename CompletionHandler>
    class AsyncOperation{
    public:  
        AsyncOperation() noexcept = default;

        static constexpr const char* OperationTypeStr() noexcept
        {
            if constexpr (OperationType::kSend == OpType)
                return "send";
            else if constexpr (OperationType::kReceive == OpType)
                return "receive";
            else
                return "connect";
        }

        /**
         * @brief Perform i/o and notify completion in continuation context
         * 
         * @tparam OnImmediateCompletion 
         * @param on_immediate 
         */
        template <typename OnImmediateCompletion>
        OperationStatus Perform(
            asrt::NativeHandle native_handle, 
            OnImmediateCompletion&& on_immediate) noexcept
        {
            assert(this->IsOngoing()); /* assert we're in the middle of an async operation */

            const auto [op_status, handled_bytes]{this->DoPerform(native_handle)};

            if(op_status == OperationStatus::kComplete){
                ASRT_LOG_TRACE("Completed async {} operation, calling completion handler", OperationTypeStr());
                this->OnCompletion(std::move(on_immediate));
            }else{
                (void)on_immediate;
                ASRT_LOG_TRACE("Async {} operation in progress", OperationTypeStr());
                this->OnContinuation(handled_bytes);
            }

            return op_status;
        }

        /**
         * @brief Perform i/o and notify completion in initiation context
         * 
         * @tparam CompletionCallback void(Result&&)
         * @tparam OnImmediateCompletion void(Callback&&, Result&&)
         * @param op_mode kSpeculative and/or kExhaustive
         * @param buff_view buffer view associated with the i/o operation
         * @param user_callback handler to be posted after successful i/o
         * @param on_immediate routine to be invoked if speculative i/o is successful
         * @return kComplete: i/o is complete and on_immediate callback will be executed right away
         * @return kAsyncNeeded: i/o could not complete without blocking;
         *    on_immediate callback is stored for later execution
         */
        template <typename CompletionCallback, typename OnImmediateCompletion>
        OperationStatus Perform(
            asrt::NativeHandle native_handle,
            int op_mode,
            BufferView buff_view,
            CompletionCallback&& user_callback,
            OnImmediateCompletion&& on_immediate) noexcept
        {
            using namespace Socket::Types;
            const bool is_exhaustive{bool(op_mode & kExhaustive)};
            const bool allow_speculative{bool(op_mode & kSpeculative)};

            if(!allow_speculative){
                this->OnInitiation(std::move(user_callback), buff_view, is_exhaustive, 0);
                return OperationStatus::kAsyncNeeded;
            }

            const auto [op_status, handled_bytes]{
                this->DoPerform(native_handle, buff_view, is_exhaustive)};

            if(op_status == OperationStatus::kComplete){
                ASRT_LOG_TRACE("Completed async {} operation, calling completion handler",
                    OperationTypeStr());
                this->OnCompletion(std::move(on_immediate), std::move(user_callback));
            }else{ /* async needed */
                ASRT_LOG_TRACE("Async {} operation started", OperationTypeStr());
                this->OnInitiation(std::move(user_callback), buff_view, is_exhaustive, handled_bytes);
            }
            return op_status;
        }

        bool IsOngoing() const {return this->opeartion_ongoing_;}

        void Reset() noexcept
        {
            this->total_bytes_ = 0;
            this->opeartion_ongoing_ = false;
            this->is_exhaustive_ = false;
            this->buffer_view_ = {};
            this->completetion_handler_ = {};
            this->completion_result_ = {};
        }

    private:

        std::size_t total_bytes_{};
        bool opeartion_ongoing_{false};
        bool is_exhaustive_{false};
        
        BufferView buffer_view_;
        CompletionHandler completetion_handler_{};
        CompletionResult completion_result_{};

        struct PerformResult{
            OperationStatus op_status_;
            std::size_t handled_bytes_;
        };

    private:

        template <typename OnImmediateCompletion>
        void OnCompletion(OnImmediateCompletion&& on_immediate) noexcept
        {
            ASRT_LOG_TRACE("On {} completion (reactor context)", OperationTypeStr());
            /* reset per operation flags */
            this->opeartion_ongoing_ = false;
            on_immediate(
                std::move(this->completetion_handler_), 
                std::move(this->completion_result_));
        }

        template <typename OnImmediateCompletion, typename CompletionCallback>
        void OnCompletion(OnImmediateCompletion&& on_immediate, CompletionCallback&& user_callback) noexcept
        {
            ASRT_LOG_TRACE("On {} completion (initiation context)", OperationTypeStr());
            on_immediate(
                std::move(user_callback), 
                std::move(this->completion_result_));
        }

        template <typename CompletionCallback>
        void OnInitiation(
            CompletionCallback&& user_callback, 
            BufferView buff_view, 
            bool is_exhaustive, 
            std::size_t bytes_handled) noexcept
        {
            ASRT_LOG_TRACE("On {} initiation", OperationTypeStr());
            this->opeartion_ongoing_ = true;
            this->buffer_view_ = buff_view;
            this->total_bytes_ = buff_view.size();
            this->is_exhaustive_ = is_exhaustive;
            this->completetion_handler_ = std::move(user_callback);
            this->buffer_view_.Advance(bytes_handled);
        }

        void OnContinuation(std::size_t bytes_handled) noexcept
        {
            ASRT_LOG_TRACE("On {} continuation", OperationTypeStr());
            this->opeartion_ongoing_ = true;
            this->buffer_view_.Advance(bytes_handled);
        }

        PerformResult DoPerform(asrt::NativeHandle native_handle) noexcept
        {
            return this->DoPerform( 
                native_handle, 
                this->buffer_view_, 
                this->is_exhaustive_,
                kContinuation);
        }

        PerformResult DoPerform(
            asrt::NativeHandle native_handle, 
            BufferView buffer_view,
            bool is_exhaustive,
            OperationContext op_context = kInitiation) noexcept
        {
            ASRT_LOG_TRACE("Performing {} {} on socket fd {}", 
                op_context == kInitiation ? "speculative" : "async",
                OperationTypeStr(), native_handle);

            /* check no existing operations are outstanding */
            if((op_context == kInitiation) && this->opeartion_ongoing_) [[unlikely]] {
                this->completion_result_ = MakeUnexpected(SockErrorCode::async_operation_in_progress);
                return {OperationStatus::kComplete, 0};
            }

            /* async connect operation */
            if constexpr (OpType == OperationType::kConnect) {
                if (op_context == OperationContext::kInitiation) {
                    Result<void> const connect_result{OsAbstraction::Connect(native_handle, buffer_view)};
                    if(not connect_result.has_value()){
                        bool const try_again{ErrorCode_Ns::IsConnectInProgress(connect_result.error())};
                        if(try_again){
                            return {OperationStatus::kAsyncNeeded, 0};
                        }
                    }
                    this->completion_result_ = connect_result;
                    return {OperationStatus::kComplete, 0};
                }else{
                
                    /* man connect(2): 
                        It is possible to select(2) or poll(2)
                        for completion by selecting the socket for writing.  After
                        select(2) indicates writability, use getsockopt(2) to read
                        the SO_ERROR option at level SOL_SOCKET to determine
                        whether connect() completed successfully (SO_ERROR is
                        zero) or unsuccessfully (SO_ERROR is one of the usual
                        error codes listed here, explaining the reason for the
                        failure). */
                    SocketBase::SocketError option_sockerror{};
                    Result<void> const connect_result{
                        OsAbstraction::GetSocketOptions(native_handle, option_sockerror)
                        .and_then([option_sockerror]() -> Result<void> {
                            if((option_sockerror.Value() != (int)0))
                                return MakeUnexpected(ErrorCode_Ns::MapLatestSysError());
                            return Result<void>{};
                        })
                    };

                    ASRT_LOG_TRACE("Connection establishment: Error = {}", 
                        ErrorCode_Ns::ToStringView(ErrorCode_Ns::FromErrno(option_sockerror.Value())));

                    bool const spurious_wakeup{ErrorCode_Ns::IsConnectInProgress(option_sockerror.Value())};
                    if(spurious_wakeup){ /* connect success */
                        return {OperationStatus::kAsyncNeeded, 0};
                    }
                    this->completion_result_ = connect_result;
                    return {OperationStatus::kComplete, 0}; 
                }
            }else{
                /* async send/recv opeartion */
                if constexpr (OpType == OperationType::kReceive) {
                    if(buffer_view.size() == 0) [[unlikely]] { /* zero-byte receives on stream sockets are no-ops */
                        ASRT_LOG_WARN("Requested to read async zero-bytes on sockfd {}", native_handle);
                        completion_result_.emplace();
                        return {OperationStatus::kComplete, 0};
                    }    
                }

                /* perform native i/o */
                Result<std::size_t> io_result;
                if constexpr (OpType == OperationType::kSend) {
                    io_result = OsAbstraction::NonBlockingSend(native_handle, buffer_view);
                } else if constexpr (OpType == OperationType::kReceive) {
                    io_result = OsAbstraction::ReceiveWithFlags(native_handle, buffer_view, MSG_DONTWAIT);
                } else {
                    LogFatalAndAbort("Unsupported op type!");
                }

                if(!io_result.has_value()) [[unlikely]] {
                    if(ErrorCode_Ns::IsBusy(io_result.error())) [[likely]] { /* resource not yet avalaible */
                        ASRT_LOG_TRACE("AsyncOperation: {} would block.", OperationTypeStr());
                        return {OperationStatus::kAsyncNeeded, 0}; /* just try again */
                    }else [[unlikely]] {
                        ASRT_LOG_DEBUG("AsyncOperation: {} got error {}.", OperationTypeStr(), io_result.error());
                        completion_result_ = MakeUnexpected(io_result.error()); /* report error */
                        return {OperationStatus::kComplete, 0};
                    }
                }

                const std::size_t bytes_handled{io_result.value()};
                if (bytes_handled == buffer_view.size()) { /* received full data */
                    ASRT_LOG_TRACE("AsyncOperation: {} full {} byte(s) of data on sockfd {}: {}", OperationTypeStr(),
                        (op_context == kContinuation ? 
                            this->total_bytes_ : buffer_view.size()), native_handle,
                            spdlog::to_hex((uint8_t*)buffer_view.data(), (uint8_t*)buffer_view.data() + buffer_view.size()));
                    completion_result_.emplace(bytes_handled); //report success
                    return {OperationStatus::kComplete, bytes_handled};
                }

                if constexpr (OpType == OperationType::kReceive){
                    if(bytes_handled == 0) [[unlikely]] { /* end of file reached */
                        ASRT_LOG_TRACE("AsyncOperation: {}, reached end of file on sockfd {}", 
                            OperationTypeStr(), native_handle);
                        completion_result_ = MakeUnexpected(SockErrorCode::end_of_file); /* report eof */
                        return {OperationStatus::kComplete, bytes_handled};
                    }
                }

                if(is_exhaustive) [[likely]] { /* if user requested full data but we only handled partial */
                    ASRT_LOG_TRACE("AsyncOperation: {} {} out of {} bytes of data on sockfd {}", OperationTypeStr(),
                        (op_context == kContinuation ?
                            this->buffer_view_.size() + bytes_handled : bytes_handled), 
                        (op_context == kContinuation ? 
                            this->total_bytes_ : buffer_view.size()), native_handle);
                    return {OperationStatus::kAsyncNeeded, bytes_handled};
                }else{ /* user requested ReceiveSome() */
                    ASRT_LOG_TRACE("AsyncOperation: {} {} byte(s) of data on sockfd {}", OperationTypeStr(),
                        bytes_handled, native_handle);
                    completion_result_.emplace(bytes_handled); //report success
                    return {OperationStatus::kComplete, bytes_handled};
                }
            }
        }

    };
} // end ns

#endif /* DFF39485_29D4_4653_9317_76E553235B43 */
