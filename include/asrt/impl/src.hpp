#ifndef A25C66CD_28D1_449E_AF3B_7C806A7B9FAE
#define A25C66CD_28D1_449E_AF3B_7C806A7B9FAE

#ifndef ASRT_COMPILED_LIB
# error Please define ASRT_COMPILED_LIB to compile this file.
#endif

#if defined(ASRT_HEADER_ONLY)
# error Do not compile ASRT library source with ASRT_HEADER_ONLY defined
#endif

#include "asrt/executor/impl/io_executor.ipp"
#include "asrt/executor/impl/strand.ipp"
#include "asrt/socket/impl/stream_socket.ipp"
#include "asrt/signalset/impl/basic_signalset.ipp"
#include "asrt/client_server/impl/client_interface.ipp"
#include "asrt/client_server/impl/server_interface.ipp"

//todo more includes to add

/* explicit template instantiation for class IO_Executor */
#include "asrt/reactor/epoll_reactor.hpp"
template class ExecutorNS::IO_Executor<ReactorNS::EpollReactor>;



#endif /* A25C66CD_28D1_449E_AF3B_7C806A7B9FAE */
