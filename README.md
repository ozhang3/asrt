# `ASRT`
> An open source task scheduling library ASRT (Async Runtime) written in modern C++ tailored for embedded linux systems. 

`ASRT` is a header-only, self-contained library that allows users to schedule arbitrary tasks on an `asrt::executor` at user-defined invocation points. For example, you can choose to schedule the task for immediate, delayed or conditional execution. The library also provides a full os-abstraction layer towards the Linux kernel. Common communication objects such as sockets, pipes, shared memory etc. are abstracted into instantiable object types. Theses objects types, ie: `asrt::BasicStreamSocket`, `asrt::SharedMemory` eases users' interaction with the os by encapsulating syscalls in the well-documented and error-proof APIs and takes care of managing the underlying system resouces with RAII for increased safety. 

The library is tested on and should work on the following platforms:
* Linux with g++ 9.4.0 or newer

Currently, the library requires a C++17-capable compiler. Work in currentlty underway to port the project to work with C++11 compilers.

## Getting started

The following program schedules task.

```c++
future<int> getRandomNumber()
{
    return std::async(std::launch::async, []() {
        return 4; // chosen by fair dice roll.
                  // guaranteed to be random.
    });
}

int main(int argc, const char* argv[])
{
    // 1. create the executor used for waiting and setting the value on futures.
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));

    // 2. set the executor as the default executor for the current scope.
    Default<Executor>::Setter execSetter(executor);

    // 3. attach a continuation that gets called only when the given future is ready.
    auto f = then(getRandomNumber(), [](future<int> f) {
        return to_string(f.get()); // f is ready - f.get() does not block
    });

    // 4. the resulting future becomes ready when the continuation produces a result.
    string result = f.get(); // result == "4"

    // 5. stop the executor and cancel all pending continuations.
    executor->stop();
}
```
