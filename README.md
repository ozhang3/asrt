# `ASRT`
> An open source task scheduling library ASRT (Async Runtime) written in modern C++ tailored for embedded linux systems. 

`ASRT` is a header-only C++ **concurrency/networking** library that makes writing performant and safe applications a breeze. Implementations of a task scheduler and common posix abstractions such as sockets and pipes are provided out of the box. If you are comfortable with C++11 or above and have written networking applications, you are good to go! No more awkward wrappers over raw system calls and manual event loops in your otherwise structured (I hope) C++ program. 

## Table of Contents

* [Architecture Overview](#111)
* [Using the library](#111)
* [Task Scheduling](#222)
	* [The Executor](#222)
* [Os Abstractions](#222)
	* [Synchronous Socket I/O](#111)
	* [Asynchronous Socket I/O](#111)
	* [The Reactor](#111)
* [Client Server Interfaces](#222)
	* [Asynchronous Tcp Client](#111)
	* [Asynchronous Udp Server](#111)
	
## Architecture Overview
The core components of the ASRT library can be logically grouped into three layers. 

![ASRT Core Componets](https://private-user-images.githubusercontent.com/68801845/358845015-832c7ace-844c-41bd-b91e-91c7e1719020.png?jwt=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3MjM4OTc3OTksIm5iZiI6MTcyMzg5NzQ5OSwicGF0aCI6Ii82ODgwMTg0NS8zNTg4NDUwMTUtODMyYzdhY2UtODQ0Yy00MWJkLWI5MWUtOTFjN2UxNzE5MDIwLnBuZz9YLUFtei1BbGdvcml0aG09QVdTNC1ITUFDLVNIQTI1NiZYLUFtei1DcmVkZW50aWFsPUFLSUFWQ09EWUxTQTUzUFFLNFpBJTJGMjAyNDA4MTclMkZ1cy1lYXN0LTElMkZzMyUyRmF3czRfcmVxdWVzdCZYLUFtei1EYXRlPTIwMjQwODE3VDEyMjQ1OVomWC1BbXotRXhwaXJlcz0zMDAmWC1BbXotU2lnbmF0dXJlPTRhYWJkM2VmMjJlMDBhY2RhZjFhY2VhNWVmZWQyMjJjNTY3YmJjY2M5Mjc5MTJmYmZlOWExMjNhZjU4YjczNWYmWC1BbXotU2lnbmVkSGVhZGVycz1ob3N0JmFjdG9yX2lkPTAma2V5X2lkPTAmcmVwb19pZD0wIn0.1Lxk-ZgtZ9TdG7ylBNgLUxWWlldIEtAl8uM8uik63qE)

### Os Abtstraction
At the very bottom, there is the *Os Abtstraction* layer that implements abstractions towards APIs/objects provided by or used to interact with the operation system. ASRT implements abstractions over communication objects such as `asrt::BasicSocket` and `asrt::BasicNamedPipe` on top of their posix equivalents. Another core component in this layer is the `asrt::Reactor` abstraction, which implemenmts the [reactor design pattern](https://en.wikipedia.org/wiki/Reactor_pattern). A reactor is a device that encapasulates a dedicated event loop that repeatedly calls `poll()`, `epoll_wait()`or `io_uring_enter()`and dispatches the reaped i/o events from those calls. Without native support on Linux for asynchronous i/o such as that provided by Window's overlapped i/o, a reactor is needed to emulate asynchrony. All ASRT i/o objects such as `asrt::BasicStreamSocket` and `asrt::BasicNamedPipe` support asynchronous i/o through underlying reactor. 

### Asyncrhony/Concurrency
In this layer, asynchronous task scheduling is implememted through abstractions such as `asrt::Executor`. From a high-level view, an *executor* is simply a combination of task storage and execution policy. Tasks are function callables that are submitted by user for execution to the executor. Execution policy controls the how (can tasks be executed in parrallel), when (execute now or some time later) and where (which thread to execute the task on) the execution takes place. ASRT also implements *timer* abstractions   such as `asrt::BasicWaitableTimer` and `asrt::BasicPeriodicTimer` which enable delayed or periodic task scheduling. They can also be used as standalone objects that enable custom timer expiry handling. *Strands* are another useful abstraction provided by the library that simplifies task synchronization in multithreaded use cases. By enqueuing mutually exclusive tasks in a `asrt::Strand`, you avoid needing to manually syncrhonize the tasks to prevent concurrent execution. 

### Application Prototypes
At the highest level, ASRT provides out-of-the-box implementation of reusable application components such as the `asrt::TcpConnection`and  `asrt::UnixStreamConnection`, which are simply different template instantiations of the same `asrt::Connection` base type. Interfaces such `asrt::ClientInterface` and `asrt::ServerInterface`can also be inherited/encapsulated by user implementations to enable typical client-server use cases.

## Using the library
The library is regularly tested on on the following platforms:
* Linux with g++ 9.4.0 or newer

Currently, the library requires a C++17-capable compiler. Work in currently underway to port the project to C++11/C++14.

<a name="222" />

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
## OS Abstraction
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

In contrast with the previous example, we do not directly inherit from the interface class. Rather, we encapsulate the server interface inside our udp server class. We instantiate the server interface template class by supplying our message handler as a template parameter. This avoids the overhead that comes with virtual function calls as seen in the previous example.


### Asynchronous Udp Server
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
	ServerImpl, 
	1500, 
	&ServerImpl::OnMessage>;

Server  server_;
};

int  main() {

	udp::Executor executor;

	udp::Endpoint ep{udp::v4(), 50000u};

	AsyncUdpServer server{executor, ep};

	executor.Run();

}
```

## Getting started


