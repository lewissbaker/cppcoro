# CppCoro - A coroutine library for C++

The 'cppcoro' library provides a set of general-purpose primitives for making use of the coroutines TS proposal described in [N4628](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/n4628.pdf).

These include:
* Coroutine Types
  * `task<T>`
  * `lazy_task<T>`
  * `shared_task<T>` (coming - lewissbaker/cppcoro#2)
  * `shared_lazy_task<T>` (coming - lewissbaker/cppcoro#2)
  * `generator<T>` (coming - lewissbaker/cppcoro#5)
  * `recursive_generator<T>` (coming - lewissbaker/cppcoro#6)
  * `async_generator<T>` (coming)
* Awaitable Types
  * `single_consumer_event`
  * `async_mutex`
  * `async_manual_reset_event` (coming)
* Functions
  * `when_all()` (coming)
* Cancellation
  * `cancellation_token` (coming)

This library is an experimental library that is exploring the space of high-performance,
scalable asynchronous programming abstractions that can be built on top of the C++ coroutines
proposal.

It has been open-sourced in the hope that others will find it useful and that the C++ community
can provide feedback on it and ways to improve it.

# Class Details

## `task<T>`

The `task<T>` type represents a computation that completes asynchronously,
yielding either a result of type `T` or an exception.

API Overview:
```c++
// <cppcoro/task.hpp>
namespace cppcoro
{
  template<typename T = void>
  class task
  {
  public:
    using promise_type = <unspecified>;

    // Construct to a detached task.
    task() noexcept;

    task(task&& other) noexcept;
    task& operator=(task&&) noexcept;

    // Task must either be detached or ready.
    ~task();

    // task is move-only
    task(const task&) = delete;
    task& operator=(const task&) = delete;

    // Query if the task result is ready yet.
    bool is_ready() const noexcept;

    // Detach the task from the coroutine.
    //
    // You will not be able to retrieve the result of a task
    // once it has been detached.
    void detach() noexcept;

    // Result of 'co_await task' has type:
    // - void if T is void
    // - T if T is a reference type
    // - T& if T is not a reference and task is an l-value reference
    // - T&& if T is not a reference and task is an r-value reference
    //
    // Either returns the result of the task or rethrows the
    // uncaught exception if the coroutine terminated due to
    // an unhandled exception.
    // Attempting to await a detached task results in the
    // cppcoro::broken_promsise exception being thrown.
    <unspecified> operator co_await() const & noexcept;
    <unspecified> operator co_await() const && noexcept;

    // Await this instead of awaiting directly on the task if
    // you just want to synchronise with the task and don't
    // need the result.
    //
    // Result of 'co_await t.when_ready()' expression has type
    // 'void' and is guaranteed not to throw an exception.
    <unspecified> when_ready() noexcept;
  };
}
```

Example:
```c++
#include <cppcoro/task.hpp>

cppcoro::task<std::string> get_name(int id)
{
  auto database = co_await open_database();
  auto record = co_await database.load_record_by_id(id);
  co_return record.name;
}

cppcoro::task<> usage_example()
{
  // Calling get_name() creates a new task and starts it immediately.
  cppcoro::task<std::string> nameTask = get_name(123);

  // The get_name() coroutine is now potentially executing concurrently
  // with the current coroutine.

  // We can later co_await the task to suspend the current coroutine
  // until the task completes. The result of the co_await expression
  // is the return value of the get_name() function.
  std::string name = co_await nameTask;
}
```

You create a `task<T>` object by calling a coroutine function that returns
a `task<T>`.

When a coroutine that returns a `task<T>` is called the coroutine starts
executing immediately and continues until the coroutine reaches the first
suspend point or runs to completion. Execution then returns to the caller
and a `task<T>` value representing the asynchronous computation is returned
from the function call.

Note that while execution returns to the caller on the current thread,
the coroutine/task should be considered to be still executing concurrently
with the current thread.

You must either `co_await` the task or otherwise ensure it has run to
completion or you must call `task.detach()` to detach the task from
the asynchronous computation before the returned task object is destroyed.
Failure to do so will result in the task<T> destructor calling std::terminate().

If the task is not yet ready when it is co_await'ed then the awaiting coroutine
will be suspended and will later be resumed on the thread that completes
execution of the coroutine.

## `lazy_task<T>`

A lazy_task represents an asynchronous computation that is executed lazily in
that the execution of the coroutine does not start until the task is awaited.

A lazy_task<T> has lower overhead than task<T> as it does not need to use
atomic operations to synchronise between consumer and producer coroutines
since the consumer coroutine suspends before the producer coroutine starts.

Example:
```c++
#include <cppcoro/lazy_task.hpp>

cppcoro::lazy_task<int> count_lines(std::string path)
{
  auto file = co_await open_file_async(path);

  int lineCount = 0;

  char buffer[1024];
  size_t bytesRead;
  do
  {
    bytesRead = co_await file.read_async(buffer, sizeof(buffer));
    lineCount += std::count(buffer, buffer + bytesRead, '\n');
  } while (bytesRead > 0);
  
  co_return lineCount;
}

cppcoro::task<> usage_example()
{
  // Calling function creates a new lazy_task but doesn't start
  // executing the coroutine yet.
  cppcoro::lazy_task<int> countTask = count_lines("foo.txt");
  
  // ...
  
  // Coroutine is only started when we later co_await the task.
  int lineCount = co_await countTask;

  std::cout << "line count = " << lineCount << std::endl;
}
```

API Overview:
```c++
// <cppcoro/lazy_task.hpp>
namespace cppcoro
{
  template<typename T>
  class lazy_task
  {
  public:
    using promise_type = <unspecified>;
	lazy_task() noexcept;
	lazy_task(lazy_task&& other) noexcept;
	lazy_task(const lazy_task& other) = delete;
	lazy_task& operator=(lazy_task&& other);
	lazy_task& operator=(const lazy_task& other) = delete;
	bool is_ready() const noexcept;
	<unspecified> operator co_await() const & noexcept;
	<unspecified> operator co_await() const && noexcept;
	<unspecified> when_ready() const noexcept;
  };
}
```

Something to be aware of with `lazy_task<T>` is that if the coroutine
completes synchronously then the awaiting coroutine is resumed
from within the call to `await_suspend()`. If your compiler is not
able to guarantee tail-call optimisations for the `await_suspend()`
and `coroutine_handle<>::resume()` calls then this can result in
consumption of extra stack-space for each `co_await` of a `lazy_task`
that completes synchronously which can lead to stack-overflow if
performed in a loop.

Using `task<T>` is safer than `lazy_task<T>` with regards to potential
stack-overflow as it starts executing the task immediately on calling
the coroutine function and unwinds the stack back to the caller before
it can be awaited. The awaiting coroutine will continue execution without
suspending if the coroutine completed synchronously.

## single_consumer_event

This is a simple manual-reset event type that supports only a single
coroutine awaiting it at a time.
This can be used to 

API Summary:
```c++
// <cppcoro/single_consumer_event.hpp>
namespace cppcoro
{
  class single_consumer_event
  {
  public:
    single_consumer_event(bool initiallySet = false) noexcept;
    bool is_set() const noexcept;
    void set();
    void reset() noexcept;
    <unspecified> operator co_await() const noexcept;
  };
}
```

Example:
```c++
#include <cppcoro/single_consumer_event.hpp>

cppcoro::single_consumer_event event;
std::string value;

cppcoro::task<> consumer()
{
  co_await event;
  std::cout << value << std::endl;
}

void producer()
{
  value = "foo";
  event.set();
}
```

## async_mutex

Provides a simple mutual exclusion abstraction that allows the caller to 'co_await' the mutex
from within a coroutine to suspend the coroutine until the mutex lock is acquired.

The implementation is lock-free in that a coroutine that awaits the mutex will not
block the thread but will instead suspend

API Summary:
```c++
// <cppcoro/async_mutex.hpp>
namespace cppcoro
{
  class async_mutex_lock_operation;

  class async_mutex
  {
  public:
    async_mutex() noexcept;
    ~async_mutex();

    async_mutex(const async_mutex&) = delete;
    async_mutex& operator(const async_mutex&) = delete;

    bool try_lock() noexcept;
    async_mutex_lock_operation lock_async() noexcept;
    void unlock();
  };

  using async_mutex_lock_result = <implementation-defined>;

  class async_mutex_lock_operation
  {
  public:
    bool await_ready() const noexcept;
    bool await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept;
    async_mutex_lock_result await_resume() const noexcept;
  };

  class async_mutex_lock
  {
  public:
    // Takes ownership of the lock.
    async_mutex_lock(async_mutex_lock_result lockResult) noexcept;
    async_mutex_lock(async_mutex& mutex, std::adopt_lock_t) noexcept;

    async_mutex_lock(const async_mutex_lock&) = delete;
    async_mutex_lock& operator=(const async_mutex_lock&) = delete;

    // Releases the lock by calling unlock() on the mutex.
    ~async_mutex_lock();
  };
}
```

Example usage:
```c++
#include <cppcoro/async_mutex.hpp>
#include <cppcoro/task.hpp>
#include <set>
#include <string>

cppcoro::async_mutex mutex;
std::set<std::string> values;

cppcoro::task<> add_item(std::string value)
{
  cppcoro::async_mutex_lock lock = co_await mutex;
  values.insert(std::move(value));
}
```

# Building

This library makes use of the [Cake build system](https://github.com/lewissbaker/cake) (no, not the [C# one](http://cakebuild.net/)).

This library has been tested with C++ compiler from Visual Studio 2015 Update 3.
Support for Visual Studio 2017 will be coming soon.

## Prerequisites

The Cake build-system is implemented in Python and requires Python 2.7 to be installed.

Ensure Python 2.7 interpreter is in your PATH and available as 'python'.

Ensure Visual Studio 2015 Update 3 is installed.

## Building from the command-line

To build from the command-line just run 'cake.bat' in the workspace root.

eg.
```
C:\cppcoro> cake.bat
Building with C:\cppcoro\config.cake - Variant(release='debug', platform='windows', architecture='x86', compilerFamily='msvc', compiler='msvc14.0')
Building with C:\cppcoro\config.cake - Variant(release='optimised', platform='windows', architecture='x64', compilerFamily='msvc', compiler='msvc14.0')
Building with C:\cppcoro\config.cake - Variant(release='debug', platform='windows', architecture='x64', compilerFamily='msvc', compiler='msvc14.0')
Building with C:\cppcoro\config.cake - Variant(release='optimised', platform='windows', architecture='x86', compilerFamily='msvc', compiler='msvc14.0')
Compiling test\main.cpp
Compiling test\main.cpp
Compiling test\main.cpp
Compiling test\main.cpp
Linking build\windows_x86_msvc14.0_debug\test\run.exe
Linking build\windows_x64_msvc14.0_optimised\test\run.exe
Linking build\windows_x86_msvc14.0_optimised\test\run.exe
Linking build\windows_x64_msvc14.0_debug\test\run.exe
Generating code
Finished generating code
Generating code
Finished generating code
Build succeeded.
Build took 0:00:02.419.
```

By default this will build all projects with all build variants.
You can narrow what is built by passing additional command-line arguments.
eg.
```
c:\cppcoro> cake.bat release=debug architecture=x64 lib/build.cake
Building with C:\Users\Lewis\Code\cppcoro\config.cake - Variant(release='debug', platform='windows', architecture='x64', compilerFamily='msvc', compiler='msvc14.0')
Archiving build\windows_x64_msvc14.0_debug\lib\cppcoro.lib
Build succeeded.
Build took 0:00:00.321.
```

You can run `cake --help` to list available command-line options.

## Building Visual Studio project files

To develop from within Visual Studio you can build .vcproj/.sln files by running `cake -p`.

eg.
```
c:\cppcoro> cake.bat -p
Building with C:\cppcoro\config.cake - Variant(release='debug', platform='windows', architecture='x86', compilerFamily='msvc', compiler='msvc14.0')
Building with C:\cppcoro\config.cake - Variant(release='optimised', platform='windows', architecture='x64', compilerFamily='msvc', compiler='msvc14.0')
Building with C:\cppcoro\config.cake - Variant(release='debug', platform='windows', architecture='x64', compilerFamily='msvc', compiler='msvc14.0')
Building with C:\cppcoro\config.cake - Variant(release='optimised', platform='windows', architecture='x86', compilerFamily='msvc', compiler='msvc14.0')
Generating Solution build/project/cppcoro.sln
Generating Project build/project/cppcoro_tests.vcxproj
Generating Filters build/project/cppcoro_tests.vcxproj.filters
Generating Project build/project/cppcoro.vcxproj
Generating Filters build/project/cppcoro.vcxproj.filters
Build succeeded.
Build took 0:00:00.247.
```

When you build these projects from within Visual Studio it will call out to cake to perform the compilation.
