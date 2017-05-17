# CppCoro - A coroutine library for C++

The 'cppcoro' library provides a set of general-purpose primitives for making use of the coroutines TS proposal described in [N4628](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/n4628.pdf).

These include:
* Coroutine Types
  * `task<T>`
  * `lazy_task<T>`
  * `shared_task<T>`
  * `shared_lazy_task<T>`
  * `generator<T>` (coming - lewissbaker/cppcoro#5)
  * `recursive_generator<T>` (coming - lewissbaker/cppcoro#6)
  * `async_generator<T>`
* Awaitable Types
  * `single_consumer_event`
  * `async_mutex`
  * `async_manual_reset_event` (coming)
* Functions
  * `when_all()` (coming)
* Cancellation
  * `cancellation_token`

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

## `shared_task<T>`

The `shared_task<T>` class is a coroutine type that yields a single value
asynchronously.

The task value can be copied, allowing multiple references
to the result of the task to be created. It also allows multiple coroutines
to concurrently await the result.

API Summary
```c++
namespace cppcoro
{
  template<typename T = void>
  class shared_task
  {
  public:
    shared_task() noexcept;
    shared_task(const shared_task& other) noexcept;
    shared_task(shared_task&& other) noexcept;
    shared_task& operator=(const shared_task& other) noexcept;
    shared_task& operator=(shared_task&& other) noexcept;

    void swap(shared_task& other) noexcept;

    // Query if the task has completed and the result is ready.
    bool is_ready() const noexcept;

    // Returns an operation that when awaited will suspend the
    // current coroutine until the task completes and the result
    // is available.
    //
    // The type of the result of the 'co_await someTask' expression
    // is an l-value reference to the task's result value (unless T
    // is void in which case the expression has type 'void').
    // If the task completed with an unhandled exception then the
    // exception will be rethrown by the co_await expression.
    <unspecified> operator co_await() const noexcept;

    // Returns an operation that when awaited will suspend the
    // calling coroutine until the task completes and the result
    // is available.
    //
    // The result is not returned from the co_await expression.
    // This can be used to synchronise with the task without the
    // possibility of the co_await expression throwing an exception.
    <unspecified> when_ready() const noexcept;

  };

  template<typename T>
  bool operator==(const shared_task<T>& a, const shared_task<T>& b) noexcept;
  template<typename T>
  bool operator!=(const shared_task<T>& a, const shared_task<T>& b) noexcept;

  template<typename T>
  void swap(shared_task<T>& a, shared_task<T>& b) noexcept;

  // Wrap a task in a shared_task to allow multiple coroutines to concurrently
  // await the result.
  template<typename T>
  shared_task<T> make_shared_task(task<T> task);
}
```

All const-methods on `shared_task<T>` are safe to call concurrently with other const-methods on the same instance from multiple threads.
It is not safe to call non-const methods of `shared_task<T>` concurrently with any other method on the same instance of a `shared_task<T>`.

### Comparison to `task<T>`

The `shared_task<T>` class is similar to `task<T>` in that the task starts execution
immediately upon the coroutine function being called.

It differs from `task<T>` in that the resulting task object can
be copied, allowing multiple task objects to reference the same
asynchronous result. It also supports multiple coroutines concurrently
awaiting the result of the task.

The trade-off is that the result is always an l-value reference to the
result, never an r-value reference (since the result may be shared) which
may limit ability to move-construct the result into a local variable.
It also has a slightly higher run-time cost due to the need to maintain
a reference count and support multiple awaiters.

## `shared_lazy_task<T>`

The `shared_lazy_task<T>` class is a coroutine type that yields a single value
asynchronously.

It is 'lazy' in that execution of the task does not start until it is awaited by some
coroutine.

It is 'shared' in that the task value can be copied, allowing multiple references to
the result of the task to be created. It also allows multiple coroutines to
concurrently await the result.

API Summary
```c++
namespace cppcoro
{
  template<typename T = void>
  class shared_lazy_task
  {
  public:
    shared_lazy_task() noexcept;
    shared_lazy_task(const shared_lazy_task& other) noexcept;
    shared_lazy_task(shared_lazy_task&& other) noexcept;
    shared_lazy_task& operator=(const shared_lazy_task& other) noexcept;
    shared_lazy_task& operator=(shared_lazy_task&& other) noexcept;

    void swap(shared_lazy_task& other) noexcept;

    // Query if the task has completed and the result is ready.
    bool is_ready() const noexcept;

    // Returns an operation that when awaited will suspend the
    // current coroutine until the task completes and the result
    // is available.
    //
    // The type of the result of the 'co_await someTask' expression
    // is an l-value reference to the task's result value (unless T
    // is void in which case the expression has type 'void').
    // If the task completed with an unhandled exception then the
    // exception will be rethrown by the co_await expression.
    <unspecified> operator co_await() const noexcept;

    // Returns an operation that when awaited will suspend the
    // calling coroutine until the task completes and the result
    // is available.
    //
    // The result is not returned from the co_await expression.
    // This can be used to synchronise with the task without the
    // possibility of the co_await expression throwing an exception.
    <unspecified> when_ready() const noexcept;

  };

  template<typename T>
  bool operator==(const shared_lazy_task<T>& a, const shared_lazy_task<T>& b) noexcept;
  template<typename T>
  bool operator!=(const shared_lazy_task<T>& a, const shared_lazy_task<T>& b) noexcept;

  template<typename T>
  void swap(shared_lazy_task<T>& a, shared_lazy_task<T>& b) noexcept;

  // Wrap a lazy_task in a shared_lazy_task to allow multiple coroutines to concurrently
  // await the result.
  template<typename T>
  shared_lazy_task<T> make_shared_task(lazy_task<T> task);
}
```

All const-methods on `shared_lazy_task<T>` are safe to call concurrently with other const-methods on the same instance from multiple threads.
It is not safe to call non-const methods of `shared_lazy_task<T>` concurrently with any other method on the same instance of a `shared_lazy_task<T>`.

## `async_generator<T>`

An `async_generator` represents a coroutine type that produces a sequence of values of type, `T`, where values are produced lazily and values may be produced asynchronously.

The coroutine body is able to use both `co_await` and `co_yield` expressions.

Consumers of the generator can use a `for co_await` range-based for-loop to consume the values.

Example
```c++
cppcoro::async_generator<int> ticker(int count, threadpool& tp)
{
  for (int i = 0; i < count; ++i)
  {
    co_await tp.delay(std::chrono::seconds(1));
    co_yield i;
  }
}

cppcoro::task<> consumer(threadpool& tp)
{
  auto sequence = ticker(tp);
  for co_await(std::uint32_t i : sequence)
  {
    std::cout << "Tick " << i << std::endl;
  }
}
```

API Summary
```c++
// <cppcoro/async_generator.hpp>
namespace cppcoro
{
  template<typename T>
  class async_generator
  {
  public:

    class iterator
    {
    public:
      using iterator_tag = std::forward_iterator_tag;
      using difference_type = std::size_t;
      using value_type = std::remove_reference_t<T>;
      using reference = value_type&;
      using pointer = value_type*;
      
      iterator(const iterator& other) noexcept;
      iterator& operator=(const iterator& other) noexcept;

      // Resumes the generator coroutine if suspended
      // Returns an operation object that must be awaited to wait
      // for the increment operation to complete.
      // If the coroutine runs to completion then the iterator
      // will subsequently become equal to the end() iterator.
      // If the coroutine completes with an unhandled exception then
      // that exception will be rethrown from the co_await expression.
      <unspecified> operator++() noexcept;

      // Dereference the iterator.
      pointer operator->() const noexcept;
      reference operator*() const noexcept;

      bool operator==(const iterator& other) const noexcept;
      bool operator!=(const iterator& other) const noexcept;
    };

    // Construct to the empty sequence.
    async_generator() noexcept;
    async_generator(const async_generator&) = delete;
    async_generator(async_generator&& other) noexcept;
    ~async_generator();

    async_generator& operator=(const async_generator&) = delete;
    async_generator& operator=(async_generator&& other) noexcept;

    void swap(async_generator& other) noexcept;

    // Starts execution of the coroutine and returns an operation object
    // that must be awaited to wait for the first value to become available.
    // The result of co_await'ing the returned object is an iterator that
    // can be used to advance to subsequent elements of the sequence.
    //
    // This method is not valid to be called once the coroutine has
    // run to completion.
    <unspecified> begin() noexcept;
    iterator end() noexcept;

  };
}
```

### Early termination of an async_generator

When the `async_generator` object is destructed it requests cancellation of the underlying coroutine.
If the coroutine has already run to completion or is currently suspended in a `co_yield` expression
then the coroutine is destroyed immediately. Otherwise, the coroutine will continue execution until
it either runs to completion or reaches the next `co_yield` expression.

When the coroutine frame is destroyed the destructors of all variables in scope at that point will be
executed to ensure the resources of the generator are cleaned up.

Note that the caller must ensure that the `async_generator` object must not be destroyed while a
consumer coroutine is executing a `co_await` expression waiting for the next item to be produced.

## `single_consumer_event`

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

## `cancellation_token`

A `cancellation_token` is a value that can be passed to a function that allows the caller to subsequently communicate a request to cancel the operation to that function.

To obtain a `cancellation_token` that is able to be cancelled you must first create a `cancellation_source` object.
The `cancellation_source::token()` method can be used to manufacture new `cancellation_token` values that are linked to that `cancellation_source` object.

When you want to later request cancellation of an operation you have passed a `cancellation_token` to
you can call `cancellation_source::request_cancellation()` on an associated `cancellation_source` object.

Functions can respond to a request for cancellation in one of two ways:
1. Poll for cancellation at regular intervals by calling either `cancellation_token::is_cancellation_requested()` or `cancellation_token::throw_if_cancellation_requested()`.
2. Register a callback to be executed when cancellation is requested using the `cancellation_registration` class.

API Summary:
```c++
namespace cppcoro
{
  class cancellation_source
  {
  public:
    // Construct a new, independently cancellable cancellation source.
    cancellation_source();

    // Construct a new reference to the same cancellation state.
    cancellation_source(const cancellation_source& other) noexcept;
    cancellation_source(cancellation_source&& other) noexcept;

    ~cancellation_source();

    cancellation_source& operator=(const cancellation_source& other) noexcept;
    cancellation_source& operator=(cancellation_source&& other) noexcept;

    bool is_cancellation_requested() const noexcept;
    bool can_be_cancelled() const noexcept;
    void request_cancellation();

    cancellation_token token() const noexcept;
  };

  class cancellation_token
  {
  public:
    // Construct a token that can't be cancelled.
    cancellation_token() noexcept;

    cancellation_token(const cancellation_token& other) noexcept;
    cancellation_token(cancellation_token&& other) noexcept;

    ~cancellation_token();

    cancellation_token& operator=(const cancellation_token& other) noexcept;
    cancellation_token& operator=(cancellation_token&& other) noexcept;

    bool is_cancellation_requested() const noexcept;
    void throw_if_cancellation_requested() const;

    // Query if this token can ever have cancellation requested.
    // Code can use this to take a more efficient code-path in cases
    // that the operation does not need to handle cancellation.
    bool can_be_cancelled() const noexcept;
  };

  // RAII class for registering a callback to be executed if cancellation
  // is requested on a particular cancellation token.
  class cancellation_registration
  {
  public:

    // Register a callback to be executed if cancellation is requested.
    // Callback will be called with no arguments on the thread that calls
    // request_cancellation() if cancellation is not yet requested, or
    // called immediately if cancellation has already been requested.
    // Callback must not throw an unhandled exception when called.
    template<typename CALLBACK>
    cancellation_registration(cancellation_token token, CALLBACK&& callback);

    cancellation_registration(const cancellation_registration& other) = delete;

    ~cancellation_registration();
  };

  class operation_cancelled : public std::exception
  {
  public:
    operation_cancelled();
    const char* what() const override;
  };
}
```

Example: Polling Approach
```c++
cppcoro::task<> do_something_async(cppcoro::cancellation_token token)
{
  // Explicitly define cancellation points within the function
  // by calling throw_if_cancellation_requested().
  token.throw_if_cancellation_requested();

  co_await do_step_1();

  token.throw_if_cancellation_requested();

  do_step_2();

  // Alternatively, you can query if cancellation has been
  // requested to allow yourself to do some cleanup before
  // returning.
  if (token.is_cancellation_requested())
  {
    display_message_to_user("Cancelling operation...");
    do_cleanup();
    throw cppcoro::operation_cancelled{};
  }

  do_final_step();
}
```

Example: Callback Approach
```c++
// Say we already have a timer abstraction that supports being
// cancelled but it doesn't support cancellation_tokens natively.
// You can use a cancellation_registration to register a callback
// that calls the existing cancellation API. e.g.
cppcoro::task<> cancellable_timer_wait(cppcoro::cancellation_token token)
{
  auto timer = create_timer(10s);

  cppcoro::cancellation_registration registration(token, [&]
  {
    // Call existing timer cancellation API.
    timer.cancel();
  });

  co_await timer;
}
```

# Building

This library makes use of the [Cake build system](https://github.com/lewissbaker/cake) (no, not the [C# one](http://cakebuild.net/)).

This library has been tested with C++ compiler from Visual Studio 2015 Update 3 and Visual Studio 2017.

## Prerequisites

The Cake build-system is implemented in Python and requires Python 2.7 to be installed.

Ensure Python 2.7 interpreter is in your PATH and available as 'python'.

Ensure either Visual Studio 2015 Update 3 or Visual Studio 2017 is installed.

You can also use an experimental version of the Visual Studio compiler by downloading a NuGet package from http://vcppdogfooding.azurewebsites.net/ and unzipping the .nuget file to a directory.
Just update the `config.cake` file to point at the unzipped location by modifying and uncommenting the following line:
```python
#nugetPath = r'C:\Path\To\VisualCppTools.14.0.25224-Pre'
```

Ensure the Windows 10 SDK version 10.0.10586.0 is installed.
It's straight-forward to use a different Windows 10 SDK version by modifing the following line in the `config.cake` file.
```python
windows10SdkVersion = "10.0.10586.0"
```

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
