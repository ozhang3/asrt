# `ASRT`
> An open source task scheduling library ASRT (Async Runtime) written in modern C++ tailored for embedded linux systems. 

`ASRT` is a header-only C++ **concurrency/networking** library that makes writing performant and safe embedded applications a breeze.  Implementations of a task scheduler and C++ abstractions for posix objects like sockets and pipes are provided out of the box. If you are comfortable with C++11 or above and have written networking applications, you are good to go! No more awkward wrappers over raw system calls and manual event loops in your otherwise structured (I hope) C++ program. 

## Table of Contents
* [Why Another Task Scheduling Library?](#111)
* [Architecture Overview](#111)
* [Using the library](#111)
* [Task Scheduling](#222)
	* [The Executor](#222)
* [Os Abstractions](#222)
	* [Synchronous Socket I/O](#111)
	* [Asynchronous Socket I/O](#111)
	* [The Reactor](#111)
	*  [Synchronous Signal Handling](#111)
* [Client Server Interfaces](#222)
	* [Asynchronous Tcp Client](#111)
	* [Asynchronous Udp Server](#111)
* [Using the Library](#222)
	

## Why Another Task Scheduling Library?

Although task libraries abound in C++, with [Boost::Asio](https://github.com/boostorg/asio),  Intel's [TBB](https://github.com/oneapi-src/oneTBB) and of course, NVDIA's [stdexec](https://github.com/NVID(https://man7.org/linux/man-pages/man7/packet.7.html)IA/stdexec) that is on course for standard shipment with C++26, there seems to be a void in development of a task scheduler that's written specifically with embedded applications in mind. ASRT fills that void. 

The design objectives of ASRT are as follows:

 - **Safety**. Safety always comes first in embedded applications. The ability of a piece of software to correctly function for extended periods of time without crashing or stalling is especially criticial In fields such as medical equipments and autonomous vehicles. `ASRT` is designed with a robust error handling and tracing mechanism. Users are shielded from directly interacting with the operation system. We use RAII abstractions over raw system resources whenever possible. All systems calls are traceable and systems erros are never ignored (either internally handled or passed on to the user). The library does not start or manage any threads whatsoever and that responsibility is left solely to the user. [**Synchronous signal handling**](#111)  is supported (with kernel verison > Linux 2.6.22) so that signals are handled gracefully and do not disrupt the normal program flow.
 
 - **Performance**.  We understand the importance of performance to any C++ programmer ; ) As modern embedded applications become increasingly complex and increasingly inter-connected, the need for an efficient and low-latency task schedulign and networking framework is criticial is ever more important. Throughout the entire framework,  expensive operations such as virtual function callss and dynamic allocations are avoided whenever posssible. **Compile-time computations** are preferred over run-time ones. **Static polymorphism** are utilised in place of run-time polymorphism. Use zero-cost/low-cost abstractions whenever possible. Benchmarks are regularly executed on differnt platforms and with differnt compilers. 
 
 - **Resources**. Memory resources are often limited in embedded applications. `ASRT` understands this and avoids dynamic allocation at all costs. When that is not possible, `ASRT` gives users the abillity to integrate their **custom memory allocation** schemes into the framework through injecting a`std::pmr::allocator` (available with C++17)  when constructing entites such as `asrt::Executor` and `asrt::BasicSocket`. These objects are said to be *allocator-aware* as they understand and can work with user-defined *memory resources* rather than always calling `new` and `delete`. Using allocators is of cource not mandatory. All `ASRT` objects can work without alloators, in which case they just default to using the global `new` and `delete` when dynamically allocating.


 - **Extensibility** Objects like `asrt::Executor` and `asrt::BasicSocket` are designed to be easily customized and extended. These base types are designed as [CRTP](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern) interfaces to be extended by derived classes. For example. if you want to implement a custom [UDP-Lite](https://en.wikipedia.org/wiki/UDP-Lite) socket, you can implement `asrt::BasicSocket`'s CRTP interface without having to write everything from scratch since the logic for opening. binding, and closing the sockets are entirely reusable. In fact, sockets like `asrt::BasicStreamSocket` and `asrt::BasicDatagramSocket` are exactly implemented by implementing the same `asrt::BasicSocket` interface. You can also see an example in the library of an implementation of a [packet socket](https://man7.org/linux/man-pages/man7/packet.7.html) called `asrt::BasicPacketSocket`that gives you an idea of you might want to implement your own socket. 


## Architecture Overview
The core components of the ASRT library can be logically grouped into three layers. 

![ASRT Core Componets](https://private-user-images.githubusercontent.com/68801845/358845015-832c7ace-844c-41bd-b91e-91c7e1719020.png?jwt=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3MjM5NjIxNzQsIm5iZiI6MTcyMzk2MTg3NCwicGF0aCI6Ii82ODgwMTg0NS8zNTg4NDUwMTUtODMyYzdhY2UtODQ0Yy00MWJkLWI5MWUtOTFjN2UxNzE5MDIwLnBuZz9YLUFtei1BbGdvcml0aG09QVdTNC1ITUFDLVNIQTI1NiZYLUFtei1DcmVkZW50aWFsPUFLSUFWQ09EWUxTQTUzUFFLNFpBJTJGMjAyNDA4MTglMkZ1cy1lYXN0LTElMkZzMyUyRmF3czRfcmVxdWVzdCZYLUFtei1EYXRlPTIwMjQwODE4VDA2MTc1NFomWC1BbXotRXhwaXJlcz0zMDAmWC1BbXotU2lnbmF0dXJlPWQ3ZDc2YTgzMTU1NjFlNWQyMTc0NDA5YWFmZDc5OTgwOGZhNzFhZjU4OGE3NWRkN2VmODIxODIxYTEzZWRmYTkmWC1BbXotU2lnbmVkSGVhZGVycz1ob3N0JmFjdG9yX2lkPTAma2V5X2lkPTAmcmVwb19pZD0wIn0.zBIcKT_ngG-5pfFShOpIGMHNwwwC6P7zN2ZNN2SLQks)

### Os Abtstraction
At the very bottom, there is the *Os Abtstraction* layer that implements abstractions towards APIs/objects provided by or used to interact with the operation system. ASRT implements abstractions over communication objects such as `asrt::BasicSocket` and `asrt::BasicNamedPipe` on top of their posix equivalents. Another core component in this layer is the `asrt::Reactor` abstraction, which implemenmts the [reactor design pattern](https://en.wikipedia.org/wiki/Reactor_pattern). A reactor is a device that encapasulates a dedicated event loop that repeatedly calls `poll()`, `epoll_wait()`or `io_uring_enter()`and dispatches the reaped i/o events from those calls. Without native support on Linux for asynchronous i/o such as that provided by Window's overlapped i/o, a reactor is needed to emulate asynchrony. All ASRT i/o objects such as `asrt::BasicStreamSocket` and `asrt::BasicNamedPipe` support asynchronous i/o through underlying reactor. 

### Asynchrony/Concurrency
In this layer, asynchronous task scheduling is implememted through abstractions such as `asrt::Executor`. From a high-level view, an *executor* is simply a combination of task storage and execution policy. Tasks are function callables that are submitted by user for execution to the executor. Execution policy controls the how (can tasks be executed in parrallel), when (execute now or some time later) and where (which thread to execute the task on) the execution takes place. ASRT also implements *timer* abstractions   such as `asrt::BasicWaitableTimer` and `asrt::BasicPeriodicTimer` which enable delayed or periodic task scheduling. They can also be used as standalone objects that enable custom timer expiry handling. *Strands* are another useful abstraction provided by the library that simplifies task synchronization in multithreaded use cases. By enqueuing mutually exclusive tasks in a `asrt::Strand`, you avoid needing to manually syncrhonize the tasks to prevent concurrent execution. 

### Application Prototypes
At the highest level, ASRT provides out-of-the-box implementation of reusable application components such as the `asrt::TcpConnection`and  `asrt::UnixStreamConnection`, which are simply different template instantiations of the same `asrt::Connection` base type. Interfaces such `asrt::ClientInterface` and `asrt::ServerInterface`can also be inherited/encapsulated by user implementations to enable typical client-server use cases.


## Task Scheduling
Scheduling tasks is as easy as calling the global api `asrt::Post()` , which signals the underlying global default `asrt::Executor` to schedule your callable for immediate asynchronous execution (synchronous execution is also possible, although rarely desired). Variants of the `Post()` are also provided, ie: `asrt::PostDeferred()` and `asrt::PostPeriodic()`, if you need delayed or repeated executions. You can easily customize the executor by instantiating your own executor with desired template parameters, which control the execution policy and service providers (more on this later), and replacing the global executor with your custom one. The executor event loop need to be run on its own thread (separate from the main application thread from which tasks are posted) by calling `Run()` on the executor for asynchronous task scheduling. 

The following program posts user tasks to the gloabl executor for execution under different conditions. 
```c++
#include <iostream>
#include <chrono>
#include <thread>
using std::chrono_literals;
	
int user_task_func()
{
    std::cout << "What is the answer?\n";
}

int main(int argc, const char* argv[])
{	
    // post the function pointer to the executor for immediate execution
    asrt::post(user_task_func);

    // schedule the lambda for delayed execution
    int const answer = 42;
    asrt::post_deferred([answer](){
	    std::cout << "The answer is " << answer << "\n";
    }, 42ms);

    // schedule function for periodic execution
    asrt::post_periodic(user_task_func, 42ms);
	
	// run the executor on separate thread. This is where tasks will be executed.
	std::thread t{[]{asrt::DefaultExecutor()->Run()}}；
	t.join();
}
```

### The Executor
The `asrt::Executor` is a core abstraction of the `ASRT` library. It handles task submission, task scheduling and task execution. By default, the first time when you call the `asrt::Post() ` API, a default global `asrt::Executor` instance is created and you can interact with this executor through global APIs like `asrt::GetDefaultExecutor()` and `asrt::PostDeferred()`. However, you may not always want a singleton executor lying around in your program. You can always create a local executor at block scope and pass its reference to any `ASRT` objects that require it. 

Below is an example of how you might create a local executor and use it to initialize a socket.

```c++
#include <asrt/ip/tcp.hpp>

int  main() {
	
	// construct a local executor object	
	asrt::ip::tcp::Executor executor;
	
	// initialize the tcp socket with the executor to enable async io
	asrt::ip::tcp::Socket tcp_socket{asrt::ip::tcp::V4(), executor};
	
}
```

#### Executor Work Guard

When an executor runs out of tasks to execute (or is never given tasks to begin with), it returns from the `Run()`call and exits the execution loop. In cases where:
```c++
	std::thread t{[&executor](){
		executor.Run();
	}};
```
that thread will exit as well. In order to prevent this from happening, you can give the executor a dummy job that never completes to keep it busy (busy in the sense that it blocks waiting for the task completion rather than busy-waiting). Here's how you might want to use it.

```c++
	std::thread t{[&executor](){ 
		asrt::ExecutorNS::WorkGuard dummy_work{executor};
		executor.Run();
	}};
```
Essentially, it's a RAII object that notifies executor of job arrival on construction and 
job completion on object destruction.

## OS Abstractions
The library also provides a full os-abstraction layer towards the Linux kernel. All system calls made to the kernel need to go through an *os_abstraction* layer that logs all interactions with the underlying os, through calls such as: 
```c++
asrt::OsAbstraction::Socket(asrt::tcp::Protocol(), SOCK_CLOEXEC); //calls posix ::socket()
```

`ASRT` gives you object-oriented, extensible RAII abstractions over posix i/o objects , such as `asrt::BasicSocket`, `asrt::BasicUnnamedPipe`and `asrt::BasicSharedMemory`. Common sockets types such as `tcp::Socket`, `udp::Socket` and `unix::StreamSocket`, `unix::DatagramSocket` are already implemented and are ready to use. You can easily extend the basic abstractions to create custom abstractions taillored to your specific application needs. You will find an example implementation in `asrt::BasicPacketSocket`of a L2 packet socket by extending the `asrt::BasicSocket`. 

### Synchronous Socket I/O


The following program creates and opens a tcp socket and attempts connection to remote server synchronously. Error handling is omitted for simplicity.

```c++
#include <iostream>
#include <asrt/ip/tcp.hpp>

int main(int argc, const char* argv[])
{	
    // create a tcp socket and a server endpoint
    tcp::Socket tcp_socket;
	tcp::Endpoint server{"127.0.0.1", 50000u}; //asume we have a running tcp server listening at this local address
	
	// actually open the socket so that it's ready for communication
	tcp_socket.Open();
	
	// connect the socket to server endpoint
	tcp_socket.Connect(server);
	
    // send a message to server
    const char[] say_hi{"Hello server!"};
    std::cout << "Sending: " << say_hi << "\n";
    tcp_socket.SendSync(server, say_hi);

    // perform a blocking receive from server
    char server_response[4096];
    tcp_socket.ReceiveSync(server_response);

	std::cout << "Server replied with " << "server_response" << std::endl;
}
```

### Asynchronous Socket I/O

The following program creates and opens a tcp socket and attempts connection to remote server asynchronously. Error handling is omitted for simplicity.

```c++
#include <iostream>
#include <asrt/asrt.hpp>
#include <asrt/ip/tcp.hpp>

int main(int argc, const char* argv[])
{	
    // create a tcp socket and a server endpoint
    tcp::Socket tcp_socket{asrt::GetDefaultExecutor()};
	tcp::Endpoint server{"127.0.0.1", 50000u}; //asume we have a running tcp server listening at this local address
	
	// actually open the socket so that it's ready for communication
	tcp_socket.Open();
	
	// connect the socket to server endpoint
	tcp_socket.Connect(server);
	
    // send a message to server
    const char[] say_hi{"Hello server!"};
    std::cout << "Sending: " << say_hi << "\n";
    tcp_socket.SendSync(server, say_hi);

    // perform a blocking receive from server
    char server_response[4096];
    tcp_socket.ReceiveSync(server_response);

	std::cout << "Server replied with " << "server_response" << std::endl;
}
```

### The Reactor

***
### Synchronous Signal Handling

Posix signals are **asynchronous** by nature, ie:  they can be delivered to a process at any point in time, interrupting the normal flow of the program's execution. 

Because of their asynchronous nature, signals introduce challenges in handling them safely and correctly. In traditional signal handling schemes, we either have to 

 1. install a gloabl signal handler 
 2. dedicate a thread to capturing and handling signals

If we go with method 1, we need to be extra careful we do not introduce race conditions or leave the program in an inconsistent state.  Method 2 is a lot safer though there's the added complexity of threading and with threading you almost always have to synchronize. Linux is unique among posix systems in that it implements a **synchronous** signal handling interface revolved around [signalfd](https://www.man7.org/linux/man-pages/man2/signalfd.2.html).  Here's an excellent [article](https://unixism.net/2021/02/making-signals-less-painful-under-linux/) that compares asynchronous and synchronous signal handling and makes the case for the latter. 

`ASRT` gives you the ability to take advantage of this unique Linux feature by introducing an abstraction called the `asrt::BasicSignalSet` that allows you to install a signal handler that is invoked synchronously by the `asrt::Executor`.

The below example masks out all signals execept `SIGABRT` and `SIGFPE` for the current thread and asynchronously waits for incoming `SIGINT` and `SIGTERM` signals.

```c++
#include <csignal>
#include <iostream>
#include <asrt/reactor/epoll_reactor.hpp> 
#include <asrt/executor/io_executor.hpp>  
#include <asrt/sinalset/basic_signalset.hpp>  

using namespace asrt;

int  main()  { 
	ExecutorNS::Io_Executor<ReacorNS::EpollReactor> executor;
	BasicSignalSet signal_set{executor};
	
	signal_set.Fill();
	
	signal_set.Remove(SIGABRT, SIGFPE);
	
	asrt::SetCurrentThreadMask(signal_set)
	
	signal_set.Clear();
	
	signal_set.Add(SIGINT, SIGTERM);
	
	signal_set.WaitAsync([](int signal){
		if(SIGINT == signal){
			std::cout << "Caught SIGINT\n";
			//handle signal here
		}else if(SIGTERM == SIGNAL){
			std::cout << "Caught SIGINT\n";
			//handle signal here
		}else{
			std::cout << "Unexpected signal number " << signal << "\n";
			//handle signal here
		}
	});  
}
```

## Client Server Interfaces

You can easily rapid-prototype your networking application using resuable application components provided by `ASRT`such as the `asrt::ClientInterface` and `asrt::ServerInterface`.  For example, to create a tcp client, you just need to inheir from the interface and provide concret implementations for the `OnMessage()` and `OnServerDisconnect()`API while the rest of the connection and I/O logic is already provided and ready to use;

In the following program we will implement an tcp client that performs asynchronous i/o with the remote server.

### Asynchronous Tcp Client
```c++
#include <iostream>
#include <asrt/asrt.hpp>
#include <asrt/ip/tcp.hpp>
#include <asrt/client_server/client_interface.hpp>

class AsyncTcpClient : asrt::ClientInterface {

};

int main(int argc, const char* argv[])
{	
    // create a tcp socket and a server endpoint
    tcp::Socket tcp_socket{asrt::GetDefaultExecutor()};
	tcp::Endpoint server{"127.0.0.1", 50000u}; //asume we have a running tcp server listening at this local address
	
	// actually open the socket so that it's ready for communication
	tcp_socket.Open();
	
	// connect the socket to server endpoint
	tcp_socket.Connect(server);
	
    // send a message to server
    const char[] say_hi{"Hello server!"};
    std::cout << "Sending: " << say_hi << "\n";
    tcp_socket.SendSync(server, say_hi);

    // perform a blocking receive from server
    char server_response[4096];
    tcp_socket.ReceiveSync(server_response);

	std::cout << "Server replied with " << "server_response" << std::endl;
}
```

### Asynchronous Udp Server

In contrast with the previous example, we do not directly inherit from the interface class. Rather, we **encapsulate** the server interface inside our udp server class. We instantiate the server interface template class by supplying our message handler as a template parameter. This avoids the overhead that comes with virtual function calls as seen in the previous example.

```c++
#include  <span>
#include  <iostream>
#include  <asrt/socket/basic_datagram_socket.hpp>
#include  <asrt/ip/udp.hpp>
#include  <asrt/client_server/datagram_server.hpp>

using namespace asrt::ip;

class AsyncUdpServer {
	AsyncUdpServer(udp::Executor& executor, udp::Endpoint const& address)
		: server_{executor, address, *this} {}

	void OnMessage(udp::Endpoint  const&  peer, ClientServer::ConstMessageView  message){
		std::cout << "Received: " << message.data() //asume message is a string
		          << "from " << peer.Address().ToString() << "\n"; 
	}

	using Server = asrt::ClientServer::DatagramServer<
		udp::Executor, 
		udp::ProtocolType, 
		AsyncUdpServer, 
		1500, 
		&AsyncUdpServer::OnMessage>;

	Server server_;
};

int  main() {

	udp::Executor executor;

	udp::Endpoint ep{udp::v4(), 50000u};

	AsyncUdpServer server{executor, ep};
	
	executor.Run();

}
```

## Using the library
The library is regularly tested on on the following platforms:
* Linux with g++ 9.4.0 or newer

Currently, the library requires a C++17-capable compiler. Work in currently underway to port the project to C++11/C++14.

There are different ways to integrate the library into your existing project. The easiet way is simply copying the `asrt/` subfolder into your `include/` or `lib/`directory. 

An example of integration with CMake is given in the `examples/` folder.

