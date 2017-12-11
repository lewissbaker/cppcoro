# CppCoro - A coroutine library for C++

The 'cppcoro' library provides a set of general-purpose primitives for making use of the coroutines TS proposal described in [N4680](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/n4680.pdf).

These include:
* Coroutine Types
  * `task<T>`
  * `shared_task<T>`
  * `generator<T>`
  * `recursive_generator<T>`
  * `async_generator<T>`
* Awaitable Types
  * `single_consumer_event`
  * `single_consumer_auto_reset_event`
  * `async_mutex`
  * `async_manual_reset_event`
  * `async_auto_reset_event`
  * `async_latch`
* Functions
  * `sync_wait()`
  * `when_all()`
  * `when_all_ready()`
  * `fmap()`
  * `schedule_on()`
  * `resume_on()`
* Cancellation
  * `cancellation_token`
  * `cancellation_source`
  * `cancellation_registration`
* Schedulers and I/O
  * `io_service`
  * `io_work_scope`
  * `file`, `readable_file`, `writable_file`
  * `read_only_file`, `write_only_file`, `read_write_file`

This library is an experimental library that is exploring the space of high-performance,
scalable asynchronous programming abstractions that can be built on top of the C++ coroutines
proposal.

It has been open-sourced in the hope that others will find it useful and that the C++ community
can provide feedback on it and ways to improve it.

It requires a compiler that supports the coroutines TS:
- Windows + Visual Studio 2017 ![Windows Build Status](https://ci.appveyor.com/api/projects/status/github/lewissbaker/cppcoro?branch=master&svg=true&passingText=master%20-%20OK&failingText=master%20-%20Failing&pendingText=master%20-%20Pending)
- Linux + Clang 5.0/6.0 + libc++ [![Build Status](https://travis-ci.org/lewissbaker/cppcoro.svg?branch=master)](https://travis-ci.org/lewissbaker/cppcoro)

The Linux version is functional except for the `io_context` and file I/O related classes which have not yet been implemented for Linux (see issue [#15](https://github.com/lewissbaker/cppcoro/issues/15) for more info).

# Class Details

## `task<T>`

A task represents an asynchronous computation that is executed lazily in
that the execution of the coroutine does not start until the task is awaited.

Example:
```c++
#include <cppcoro/read_only_file.hpp>
#include <cppcoro/task.hpp>

cppcoro::task<int> count_lines(std::string path)
{
  auto file = co_await cppcoro::read_only_file::open(path);

  int lineCount = 0;

  char buffer[1024];
  size_t bytesRead;
  std::uint64_t offset = 0;
  do
  {
    bytesRead = co_await file.read(offset, buffer, sizeof(buffer));
    lineCount += std::count(buffer, buffer + bytesRead, '\n');
    offset += bytesRead;
  } while (bytesRead > 0);
  
  co_return lineCount;
}

cppcoro::task<> usage_example()
{
  // Calling function creates a new task but doesn't start
  // executing the coroutine yet.
  cppcoro::task<int> countTask = count_lines("foo.txt");
  
  // ...
  
  // Coroutine is only started when we later co_await the task.
  int lineCount = co_await countTask;

  std::cout << "line count = " << lineCount << std::endl;
}
```

API Overview:
```c++
// <cppcoro/task.hpp>
namespace cppcoro
{
  template<typename T>
  class task
  {
  public:

    using promise_type = <unspecified>;
    using value_type = T;

    task() noexcept;

    task(task&& other) noexcept;
    task& operator=(task&& other);

    // task is a move-only type.
    task(const task& other) = delete;
    task& operator=(const task& other) = delete;

    // Query if the task result is ready.
    bool is_ready() const noexcept;

    // Wait for the task to complete and return the result or rethrow the
    // exception if the operation completed with an unhandled exception.
    //
    // If the task is not yet ready then the awaiting coroutine will be
    // suspended until the task completes. If the the task is_ready() then
    // this operation will return the result synchronously without suspending.
    <unspecified> operator co_await() const & noexcept;
    <unspecified> operator co_await() const && noexcept;

    // Returns an awaitable that can be co_await'ed to suspend the current
    // coroutine until the task completes.
    //
    // The 'co_await t.when_ready()' expression differs from 'co_await t' in
    // that when_ready() only performs synchronisation, it does not return
    // the result or rethrow the exception.
    //
    // This can be useful if you want to synchronise with the task without
    // the possibility of it throwing an exception.
    <unspecified> when_ready() const noexcept;
  };

  template<typename T>
  void swap(task<T>& a, task<T>& b);

  // Apply func() to the result of the task, returning a new task that
  // yields the result of 'func(co_await task)'.
  template<typename FUNC, typename T>
  task<std::invoke_result_t<FUNC, T&&>> fmap(FUNC func, task<T> task);

  // Call func() after task completes, returning a task containing the
  // result of func().
  template<typename FUNC>
  task<std::invoke_result_t<FUNC>> fmap(FUNC func, task<void> task);
}
```

You create a `task<T>` object by calling a coroutine function that returns
a `task<T>`.

The coroutine must contain a usage of either `co_await` or `co_return`.
Note that a `task<T>` coroutine may not use the `co_yield` keyword.

When a coroutine that returns a `task<T>` is called, a coroutine frame
is allocated if necessary and the parameters are captured in the coroutine
frame. The coroutine is suspended at the start of the coroutine body and
execution is returned to the caller and a `task<T>` value that represents
the asynchronous computation is returned from the function call.

The coroutine body will start executing when the `task<T>` value is
`co_await`ed. This will suspend the awaiting coroutine and start execution
of the coroutine associated with the awaited `task<T>` value.

The awaiting coroutine will later be resumed on the thread that completes
execution of the awaited `task<T>`'s coroutine. ie. the thread that
executes the `co_return` or that throws an unhandled exception that terminates
execution of the coroutine.

If the task has already run to completion then awaiting it again will obtain
the already-computed result without suspending the awaiting coroutine.

If the `task` value is destroyed before it is awaited then the coroutine
never executes and the destructor simply destructs the captured parameters
and frees any memory used by the coroutine frame.


## `shared_task<T>`

The `shared_task<T>` class is a coroutine type that yields a single value
asynchronously.

It is 'lazy' in that execution of the task does not start until it is awaited by some
coroutine.

It is 'shared' in that the task value can be copied, allowing multiple references to
the result of the task to be created. It also allows multiple coroutines to
concurrently await the result.

The task will start executing on the thread that first `co_await`s the task.
Subsequent awaiters will either be suspended and queued for resumption
when the task completes or will continue synchronously if the task has
already run to completion.

If an awaiter is suspended while waiting for the task to complete then
it will be resumed on the thread that completes execution of the task.
ie. the thread that executes the `co_return` or that throws the unhandled
exception that terminates execution of the coroutine.

API Summary
```c++
namespace cppcoro
{
  template<typename T = void>
  class shared_task
  {
  public:

    using promise_type = <unspecified>;
    using value_type = T;

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

  // Apply func() to the result of the task, returning a new task that
  // yields the result of 'func(co_await task)'.
  template<typename FUNC, typename T>
  task<std::invoke_result_t<FUNC, T&&>> fmap(FUNC func, shared_task<T> task);

  // Call func() after task completes, returning a task containing the
  // result of func().
  template<typename FUNC>
  task<std::invoke_result_t<FUNC>> fmap(FUNC func, shared_task<void> task);
}
```

All const-methods on `shared_task<T>` are safe to call concurrently with other const-methods on the same instance from multiple threads.
It is not safe to call non-const methods of `shared_task<T>` concurrently with any other method on the same instance of a `shared_task<T>`.

### Comparison to `task<T>`

The `shared_task<T>` class is similar to `task<T>` in that the task does
not start execution immediately upon the coroutine function being called. The task
only starts executing when it is first awaited.

It differs from `task<T>` in that the resulting task object can be copied,
allowing multiple task objects to reference the same asynchronous result.
It also supports multiple coroutines concurrently awaiting the result of the task.

The trade-off is that the result is always an l-value reference to the
result, never an r-value reference (since the result may be shared) which
may limit ability to move-construct the result into a local variable.
It also has a slightly higher run-time cost due to the need to maintain
a reference count and support multiple awaiters.

## `generator<T>`

A `generator` represents a coroutine type that produces a sequence of values of type, `T`, where values are produced lazily and synchronously.

The coroutine body is able to yield values of type `T` using the `co_yield` keyword.
Note, however, that the coroutine body is not able to use the `co_await` keyword; values must be produced synchronously.

For example:
```c++
cppcoro::generator<const std::uint64_t> fibonacci()
{
  std::uint64_t a = 0, b = 1;
  while (true)
  {
    co_yield b;
    auto tmp = a;
    a = b;
    b += tmp;
  }
}

void usage()
{
  for (auto i : fibonacci())
  {
    if (i > 1'000'000) break;
    std::cout << i << std::endl;
  }
}
```

When a coroutine function returning a `generator<T>` is called the coroutine is created initially suspended.
Execution of the coroutine enters the coroutine body when the `generator<T>::begin()` method is called and continues until
either the first `co_yield` statement is reached or the coroutine runs to completion.

If the returned iterator is not equal to the `end()` iterator then dereferencing the iterator will return a reference to the value passed to the `co_yield` statement.

Calling `operator++()` on the iterator will resume execution of the coroutine and continue until either the next `co_yield` point is reached or the coroutine runs to completion().

Any unhandled exceptions thrown by the coroutine will propagate out of the `begin()` or `operator++()` calls to the caller.

API Summary:
```c++
namespace cppcoro
{
    template<typename T>
    class generator
    {
    public:

        using promise_type = <unspecified>;

        class iterator
        {
        public:
            using iterator_category = std::input_iterator_tag;
            using value_type = std::remove_reference_t<T>;
            using reference = value_type&;
            using pointer = value_type*;
            using difference_type = std::size_t;

            iterator(const iterator& other) noexcept;
            iterator& operator=(const iterator& other) noexcept;

            // If the generator coroutine throws an unhandled exception before producing
            // the next element then the exception will propagate out of this call.
            iterator& operator++();

            reference operator*() const noexcept;
            pointer operator->() const noexcept;

            bool operator==(const iterator& other) const noexcept;
            bool operator!=(const iterator& other) const noexcept;
        };

        // Constructs to the empty sequence.
        generator() noexcept;

        generator(generator&& other) noexcept;
        generator& operator=(generator&& other) noexcept;
        
        generator(const generator& other) = delete;
        generator& operator=(const generator&) = delete;

        ~generator();

        // Starts executing the generator coroutine which runs until either a value is yielded
        // or the coroutine runs to completion or an unhandled exception propagates out of the
        // the coroutine.
        iterator begin();

        iterator end() noexcept;

        // Swap the contents of two generators.
        void swap(generator& other) noexcept;

    };

    template<typename T>
    void swap(generator<T>& a, generator<T>& b) noexcept;

    // Apply function, func, lazily to each element of the source generator
    // and yield a sequence of the results of calls to func().
    template<typename FUNC, typename T>
    generator<std::invoke_result_t<FUNC, T&>> fmap(FUNC func, generator<T> source);
}
```

## `recursive_generator<T>`

A `recursive_generator` is similar to a `generator` except that it is designed to more efficiently
support yielding the elements of a nested sequence as elements of an outer sequence.

In addition to being able to `co_yield` a value of type `T` you can also `co_yield` a value of type `recursive_generator<T>`.

When you `co_yield` a `recursive_generator<T>` value the all elements of the yielded generator are yielded as elements of the current generator.
The current coroutine is suspended until the consumer has finished consuming all elements of the nested generator, after which point execution
of the current coroutine will resume execution to produce the next element.

The benefit of `recursive_generator<T>` over `generator<T>` for iterating over recursive data-structures is that the `iterator::operator++()`
is able to directly resume the leaf-most coroutine to produce the next element, rather than having to resume/suspend O(depth) coroutines for each element.
The down-side is that there is additional overhead 

For example:
```c++
// Lists the immediate contents of a directory.
cppcoro::generator<dir_entry> list_directory(std::filesystem::path path);

cppcoro::recursive_generator<dir_entry> list_directory_recursive(std::filesystem::path path)
{
  for (auto& entry : list_directory(path))
  {
    co_yield entry;
    if (entry.is_directory())
    {
      co_yield list_directory_recursive(entry.path());
    }
  }
}
```

Note that applying the `fmap()` operator to a `recursive_generator<T>` will yield a `generator<U>`
type rather than a `recursive_generator<U>`. This is because uses of `fmap` are generally not used
in recursive contexts and we try to avoid the extra overhead incurred by `recursive_generator`.

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

  template<typename T>
  void swap(async_generator<T>& a, async_generator<T>& b);

  // Apply 'func' to each element of the source generator, yielding a sequence of
  // the results of calling 'func' on the source elements.
  template<typename FUNC, typename T>
  async_generator<std::invoke_result_t<FUNC, T&>> fmap(FUNC func, async_generator<T> source);
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
  // Coroutine will suspend here until some thread calls event.set()
  // eg. inside the producer() function below.
  co_await event;

  std::cout << value << std::endl;
}

void producer()
{
  value = "foo";

  // This will resume the consumer() coroutine inside the call to set()
  // if it is currently suspended.
  event.set();
}
```

## `single_consumer_async_auto_reset_event`

This class provides an async synchronisation primitive that allows a single coroutine to
wait until the event is signalled by a call to the `set()` method.

Once the coroutine that is awaiting the event is released by either a prior or subsequent call to `set()`
the event is automatically reset back to the 'not set' state.

This class is a more efficient version of `async_auto_reset_event` that can be used in cases where
only a single coroutine will be awaiting the event at a time. If you need to support multiple concurrent
awaiting coroutines on the event then use the `async_auto_reset_event` class instead.

API Summary:
```c++
// <cppcoro/single_consumer_async_auto_reset_event.hpp>
namespace cppcoro
{
  class single_consumer_async_auto_reset_event
  {
  public:

    single_consumer_async_auto_reset_event(
      bool initiallySet = false) noexcept;

    // Change the event to the 'set' state. If a coroutine is awaiting the
    // event then the event is immediately transitioned back to the 'not set'
    // state and the coroutine is resumed.
    void set() noexcept;

    // Returns an Awaitable type that can be awaited to wait until
    // the event becomes 'set' via a call to the .set() method. If
    // the event is already in the 'set' state then the coroutine
    // continues without suspending.
    // The event is automatically reset back to the 'not set' state
    // before resuming the coroutine.
    Awaitable<void> operator co_await() const noexcept;

  };
}
```

Example Usage:
```c++
std::atomic<int> value;
cppcoro::single_consumer_async_auto_reset_event valueDecreasedEvent;

cppcoro::task<> wait_until_value_is_below(int limit)
{
  while (value.load(std::memory_order_relaxed) >= limit)
  {
    // Wait until there has been some change that we're interested in.
    co_await valueDecreasedEvent;
  }
}

void change_value(int delta)
{
  value.fetch_add(delta, std::memory_order_relaxed);
  // Notify the waiter if there has been some change.
  if (delta < 0) valueDecreasedEvent.set();
}
```

## `async_mutex`

Provides a simple mutual exclusion abstraction that allows the caller to 'co_await' the mutex
from within a coroutine to suspend the coroutine until the mutex lock is acquired.

The implementation is lock-free in that a coroutine that awaits the mutex will not
block the thread but will instead suspend the coroutine and later resume it inside
the call to `unlock()` by the previous lock-holder.

API Summary:
```c++
// <cppcoro/async_mutex.hpp>
namespace cppcoro
{
  class async_mutex_lock;
  class async_mutex_lock_operation;
  class async_mutex_scoped_lock_operation;

  class async_mutex
  {
  public:
    async_mutex() noexcept;
    ~async_mutex();

    async_mutex(const async_mutex&) = delete;
    async_mutex& operator(const async_mutex&) = delete;

    bool try_lock() noexcept;
    async_mutex_lock_operation lock_async() noexcept;
    async_mutex_scoped_lock_operation scoped_lock_async() noexcept;
    void unlock();
  };

  class async_mutex_lock_operation
  {
  public:
    bool await_ready() const noexcept;
    bool await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept;
    void await_resume() const noexcept;
  };

  class async_mutex_scoped_lock_operation
  {
  public:
    bool await_ready() const noexcept;
    bool await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept;
    [[nodiscard]] async_mutex_lock await_resume() const noexcept;
  };

  class async_mutex_lock
  {
  public:
    // Takes ownership of the lock.
    async_mutex_lock(async_mutex& mutex, std::adopt_lock_t) noexcept;

    // Transfer ownership of the lock.
    async_mutex_lock(async_mutex_lock&& other) noexcept;

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
  cppcoro::async_mutex_lock lock = co_await mutex.scoped_lock_async();
  values.insert(std::move(value));
}
```

## `async_manual_reset_event`

A manual-reset event is a coroutine/thread-synchronisation primitive that allows one or more threads
to wait until the event is signalled by a thread that calls `set()`.

The event is in one of two states; *'set'* and *'not set'*.

If the event is in the *'set'* state when a coroutine awaits the event then the coroutine
continues without suspending. However if the coroutine is in the *'not set'* state then the
coroutine is suspended until some thread subsequently calls the `set()` method.

Any threads that were suspended while waiting for the event to become *'set'* will be resumed
inside the next call to `set()` by some thread.

Note that you must ensure that no coroutines are awaiting a *'not set'* event when the
event is destructed as they will not be resumed.

Example:
```c++
cppcoro::async_manual_reset_event event;
std::string value;

void producer()
{
  value = get_some_string_value();

  // Publish a value by setting the event.
  event.set();
}

// Can be called many times to create many tasks.
// All consumer tasks will wait until value has been published.
cppcoro::task<> consumer()
{
  // Wait until value has been published by awaiting event.
  co_await event;

  consume_value(value);
}
```

API Summary:
```c++
namespace cppcoro
{
  class async_manual_reset_event_operation;

  class async_manual_reset_event
  {
  public:
    async_manual_reset_event(bool initiallySet = false) noexcept;
    ~async_manual_reset_event();

    async_manual_reset_event(const async_manual_reset_event&) = delete;
    async_manual_reset_event(async_manual_reset_event&&) = delete;
    async_manual_reset_event& operator=(const async_manual_reset_event&) = delete;
    async_manual_reset_event& operator=(async_manual_reset_event&&) = delete;

    // Wait until the event becomes set.
    <unspecified> operator co_await() const noexcept;

    bool is_set() const noexcept;

    void set() noexcept;

    void reset() noexcept;

  };

  class async_manual_reset_event_operation
  {
  public:
    async_manual_reset_event_operation(async_manual_reset_event& event) noexcept;

    bool await_ready() const noexcept;
    bool await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept;
    void await_resume() const noexcept;
  };
}
```

## `async_auto_reset_event`

An auto-reset event is a coroutine/thread-synchronisation primitive that allows one or more threads
to wait until the event is signalled by a thread by calling `set()`.

Once a coroutine that is awaiting the event is released by either a prior or subsequent call to `set()`
the event is automatically reset back to the 'not set' state.

API Summary:
```c++
// <cppcoro/async_auto_reset_event.hpp>
namespace cppcoro
{
  class async_auto_reset_event_operation;

  class async_auto_reset_event
  {
  public:

    async_auto_reset_event(bool initiallySet = false) noexcept;

    ~async_auto_reset_event();

    async_auto_reset_event(const async_auto_reset_event&) = delete;
    async_auto_reset_event(async_auto_reset_event&&) = delete;
    async_auto_reset_event& operator=(const async_auto_reset_event&) = delete;
    async_auto_reset_event& operator=(async_auto_reset_event&&) = delete;

    // Wait for the event to enter the 'set' state.
    //
    // If the event is already 'set' then the event is set to the 'not set'
    // state and the awaiting coroutine continues without suspending.
    // Otherwise, the coroutine is suspended and later resumed when some
    // thread calls 'set()'.
    //
    // Note that the coroutine may be resumed inside a call to 'set()'
    // or inside another thread's call to 'operator co_await()'.
    async_auto_reset_event_operation operator co_await() const noexcept;

    // Set the state of the event to 'set'.
    //
    // If there are pending coroutines awaiting the event then one
    // pending coroutine is resumed and the state is immediately
    // set back to the 'not set' state.
    //
    // This operation is a no-op if the event was already 'set'.
    void set() noexcept;

    // Set the state of the event to 'not-set'.
    //
    // This is a no-op if the state was already 'not set'.
    void reset() noexcept;

  };

  class async_auto_reset_event_operation
  {
  public:
    explicit async_auto_reset_event_operation(async_auto_reset_event& event) noexcept;
    async_auto_reset_event_operation(const async_auto_reset_event_operation& other) noexcept;

    bool await_ready() const noexcept;
    bool await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept;
    void await_resume() const noexcept;

  };
}
```

## `async_latch`

An async latch is a synchronisation primitive that allows coroutines to asynchronously
wait until a counter has been decremented to zero.

The latch is a single-use object. Once the counter reaches zero the latch becomes 'ready'
and will remain ready until the latch is destroyed.

API Summary:
```c++
// <cppcoro/async_latch.hpp>
namespace cppcoro
{
  class async_latch
  {
  public:

    // Initialise the latch with the specified count.
    async_latch(std::ptrdiff_t initialCount) noexcept;

    // Query if the count has reached zero yet.
    bool is_ready() const noexcept;

    // Decrement the count by n.
    // This will resume any waiting coroutines if the count reaches zero
    // as a result of this call.
    // It is undefined behaviour to decrement the count below zero.
    void count_down(std::ptrdiff_t n = 1) noexcept;

    // Wait until the latch becomes ready.
    // If the latch count is not yet zero then the awaiting coroutine will
    // be suspended and later resumed by a call to count_down() that decrements
    // the count to zero. If the latch count was already zero then the coroutine
    // continues without suspending.
    Awaiter<void> operator co_await() const noexcept;

  };
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

## `io_service`

The `io_service` class provides an abstraction for processing I/O completion events
from asynchronous I/O operations.

When an asynchronous I/O operation completes, the coroutine that was awaiting
that operation will be resumed on an I/O thread inside a call to one of the
event-processing methods: `process_events()`, `process_pending_events()`,
`process_one_event()` or `process_one_pending_event()`.

The `io_service` class does not manage any I/O threads.
You must ensure that some thread calls one of the event-processing methods for coroutines awaiting I/O
completion events to be dispatched. This can either be a dedicated thread that calls `process_events()`
or mixed in with some other event loop (e.g. a UI event loop) by periodically polling for new events
via a call to `process_pending_events()` or `process_one_pending_event()`.

This allows integration of the `io_service` event-loop with other event loops, such as a user-interface event loop.

You can multiplex processing of events across multiple threads by having multiple threads call
`process_events()`. You can specify a hint as to the maximum number of threads to have actively
processing events via an optional `io_service` constructor parameter.

On Windows, the implementation makes use of the Windows I/O Completion Port facility to dispatch
events to I/O threads in a scalable manner.

API Summary:
```c++
namespace cppcoro
{
  class io_service
  {
  public:

    class schedule_operation;
    class timed_schedule_operation;

    io_service();
    io_service(std::uint32_t concurrencyHint);

    io_service(io_service&&) = delete;
    io_service(const io_service&) = delete;
    io_service& operator=(io_service&&) = delete;
    io_service& operator=(const io_service&) = delete;

    ~io_service();

    // Scheduler methods

    [[nodiscard]]
    schedule_operation schedule() noexcept;

    template<typename REP, typename RATIO>
    [[nodiscard]]
    timed_schedule_operation schedule_after(
      std::chrono::duration<REP, RATIO> delay,
      cppcoro::cancellation_token cancellationToken = {}) noexcept;

    // Event-loop methods
    //
    // I/O threads must call these to process I/O events and execute
    // scheduled coroutines.

    std::uint64_t process_events();
    std::uint64_t process_pending_events();
    std::uint64_t process_one_event();
    std::uint64_t process_one_pending_event();

    // Request that all threads processing events exit their event loops.
    void stop() noexcept;

    // Query if some thread has called stop()
    bool is_stop_requested() const noexcept;

    // Reset the event-loop after a call to stop() so that threads can
    // start processing events again.
    void reset();

    // Reference-counting methods for tracking outstanding references
    // to the io_service.
    //
    // The io_service::stop() method will be called when the last work
    // reference is decremented.
    //
    // Use the io_work_scope RAII class to manage calling these methods on
    // entry-to and exit-from a scope.
    void notify_work_started() noexcept;
    void notify_work_finished() noexcept;

  };

  class io_service::schedule_operation
  {
  public:
    schedule_operation(const schedule_operation&) noexcept;
    schedule_operation& operator=(const schedule_operation&) noexcept;

    bool await_ready() const noexcept;
    void await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept;
    void await_resume() noexcept;
  };

  class io_service::timed_schedule_operation
  {
  public:
    timed_schedule_operation(timed_schedule_operation&&) noexcept;

    timed_schedule_operation(const timed_schedule_operation&) = delete;
    timed_schedule_operation& operator=(const timed_schedule_operation&) = delete;
    timed_schedule_operation& operator=(timed_schedule_operation&&) = delete;

    bool await_ready() const noexcept;
    void await_suspend(std::experimental::coroutine_handle<> awaiter);
    void await_resume();
  };

  class io_work_scope
  {
  public:

    io_work_scope(io_service& ioService) noexcept;

    io_work_scope(const io_work_scope& other) noexcept;
    io_work_scope(io_work_scope&& other) noexcept;

    ~io_work_scope();

    io_work_scope& operator=(const io_work_scope& other) noexcept;
    io_work_scope& operator=(io_work_scope&& other) noexcept;

    io_service& service() const noexcept;
  };

}
```

Example:
```c++
#include <cppcoro/task.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/read_only_file.hpp>

#include <experimental/filesystem>
#include <memory>
#include <algorithm>
#include <iostream>

namespace fs = std::experimental::filesystem;

cppcoro::task<std::uint64_t> count_lines(cppcoro::io_service& ioService, fs::path path)
{
  auto file = cppcoro::read_only_file::open(ioService, path);

  constexpr size_t bufferSize = 4096;
  auto buffer = std::make_unique<std::uint8_t[]>(bufferSize);

  std::uint64_t newlineCount = 0;

  for (std::uint64_t offset = 0, fileSize = file.size(); offset < fileSize;)
  {
    const auto bytesToRead = static_cast<size_t>(
      std::min<std::uint64_t>(bufferSize, fileSize - offset));

    const auto bytesRead = co_await file.read(offset, buffer.get(), bytesToRead);

    newlineCount += std::count(buffer.get(), buffer.get() + bytesRead, '\n');

    offset += bytesRead;
  }

  co_return newlineCount;
}

cppcoro::task<> run(cppcoro::io_service& ioService)
{
  cppcoro::io_work_scope ioScope(ioService);

  auto lineCount = co_await count_lines(ioService, fs::path{"foo.txt"});

  std::cout << "foo.txt has " << lineCount << " lines." << std::endl;;
}

cppcoro::task<> process_events(cppcoro::io_service& ioService)
{
  // Process events until the io_service is stopped.
  // ie. when the last io_work_scope goes out of scope.
  ioService.process_events();
  co_return;
}

int main()
{
  cppcoro::io_service ioService;

  cppcoro::sync_wait(cppcoro::when_all_ready(
    run(ioService),
    process_events(ioService)));

  return 0;
}
```

### `io_service` as a scheduler

An `io_sevice` class implements the interfaces for the `Scheduler` and `DelayedScheduler` concepts.

This allows a coroutine to suspend execution on the current thread and schedule itself for resumption
on an I/O thread associated with a particular `io_service` object.

Example:
```c++
cppcoro::task<> do_something(cppcoro::io_service& ioService)
{
  // Coroutine starts execution on the thread of the task awaiter.

  // A coroutine can transfer execution to an I/O thread by awaiting the
  // result of io_service::schedule().
  co_await ioService.schedule();

  // At this point, the coroutine is now executing on an I/O thread
  // inside a call to one of the io_service event processing methods.

  // A coroutine can also perform a delayed-schedule that will suspend
  // the coroutine for a specified duration of time before scheduling
  // it for resumption on an I/O thread.
  co_await ioService.schedule_after(100ms);

  // At this point, the coroutine is executing on a potentially different I/O thread.
}
```

## `file`, `readable_file`, `writable_file`

These types are abstract base-classes for performing concrete file I/O.

API Summary:
```c++
namespace cppcoro
{
  class file_read_operation;
  class file_write_operation;

  class file
  {
  public:

    virtual ~file();

    std::uint64_t size() const;

  protected:

    file(file&& other) noexcept;

  };

  class readable_file : public virtual file
  {
  public:

    [[nodiscard]]
    file_read_operation read(
      std::uint64_t offset,
      void* buffer,
      std::size_t byteCount,
      cancellation_token ct = {}) const noexcept;

  };

  class writable_file : public virtual file
  {
  public:

    void set_size(std::uint64_t fileSize);

    [[nodiscard]]
    file_write_operation write(
      std::uint64_t offset,
      const void* buffer,
      std::size_t byteCount,
      cancellation_token ct = {}) noexcept;

  };

  class file_read_operation
  {
  public:

    file_read_operation(file_read_operation&& other) noexcept;

    bool await_ready() const noexcept;
    bool await_suspend(std::experimental::coroutine_handle<> awaiter);
    std::size_t await_resume();

  };

  class file_write_operation
  {
  public:

    file_write_operation(file_write_operation&& other) noexcept;

    bool await_ready() const noexcept;
    bool await_suspend(std::experimental::coroutine_handle<> awaiter);
    std::size_t await_resume();

  };
}
```

## `read_only_file`, `write_only_file`, `read_write_file`

These types represent concrete file I/O classes.

API Summary:
```c++
namespace cppcoro
{
  class read_only_file : public readable_file
  {
  public:

    [[nodiscard]]
    static read_only_file open(
      io_service& ioService,
      const std::experimental::filesystem::path& path,
      file_share_mode shareMode = file_share_mode::read,
      file_buffering_mode bufferingMode = file_buffering_mode::default_);

  };

  class write_only_file : public writable_file
  {
  public:

    [[nodiscard]]
    static write_only_file open(
      io_service& ioService,
      const std::experimental::filesystem::path& path,
      file_open_mode openMode = file_open_mode::create_or_open,
      file_share_mode shareMode = file_share_mode::none,
      file_buffering_mode bufferingMode = file_buffering_mode::default_);

  };

  class read_write_file : public readable_file, public writable_file
  {
  public:

    [[nodiscard]]
    static read_write_file open(
      io_service& ioService,
      const std::experimental::filesystem::path& path,
      file_open_mode openMode = file_open_mode::create_or_open,
      file_share_mode shareMode = file_share_mode::none,
      file_buffering_mode bufferingMode = file_buffering_mode::default_);

  };
}
```

All `open()` functions throw `std::system_error` on failure.

# Functions

## `sync_wait()`

The `sync_wait()`function can be used to synchronously wait until the specified `task`
or `shared_task` completes.

If the task has not yet started execution then it will be started on the current thread.

The `sync_wait()` call will block until the task completes and will return the task's result
or rethrow the task's exception if the task completed with an unhandled exception.

The `sync_wait()` function is mostly useful for starting a top-level task from within `main()`
and waiting until the task finishes, in practise it is the only way to start the first/top-level
`task`.

API Summary:
```c++
// <cppcoro/sync_wait.hpp>
namespace cppcoro
{
  template<typename TASKS>
  decltype(auto) sync_wait(TASK&& task);
}
```

Examples:
```c++
void example_task()
{
  auto makeTask = []() -> task<std::string>
  {
    co_return "foo";
  };

  auto task = makeTask();

  // start the lazy task and wait until it completes
  sync_wait(task) == "foo";
  sync_wait(makeTask()) == "foo";
}

void example_shared_task()
{
  auto makeTask = []() -> shared_task<std::string>
  {
    co_return "foo";
  };

  auto task = makeTask();
  // start the shared task and wait until it completes
  sync_wait(task) == "foo";
  sync_wait(makeTask()) == "foo";
}
```

## `when_all_ready()`

The `when_all_ready()` function can be used to create a new `task` that will
complete when all of the specified input tasks have completed.

Input tasks can either be `task<T>` or `shared_task<T>`.

When the returned `task` is `co_await`ed it will start executing each of the input
tasks in turn on the awaiting thread in the order they are passed to the `when_all_ready()`
function. If these tasks to not complete synchronously then they will execute concurrently.

Once all of the input tasks have run to completion the returned `task` will complete
and resume the awaiting coroutine. The awaiting coroutine will be resumed on the thread
of the input task that is last to complete.

The returned `task` is guaranteed not to throw an exception when `co_await`ed,
even if some of the input tasks fail with an unhandled exception.

Note, however, that the `when_all_ready()` call itself may throw `std::bad_alloc` if it
was unable to allocate memory for the returned `task`'s coroutine frame.

The input tasks are returned back to the awaiting coroutine upon completion.
This allows the caller to execute the coroutines concurrently and synchronise their
completion while still retaining the ability to subsequently inspect the results of
each of the input tasks for success/failure.

This differs from `when_all()` in a similar way that `co_await`ing `task<T>::when_ready()`
differs from `co_await'ing the `task<T>` directly.

API summary:
```c++
// <cppcoro/when_all_ready.hpp>
namespace cppcoro
{
  template<typename... TASKS>
  task<std::tuple<TASKS...>> when_all_ready(TASKS... tasks);

  template<typename T>
  task<std::vector<task<T>> when_all_ready(
    std::vector<task<T>> tasks);

  template<typename T>
  task<std::vector<shared_task<T>> when_all_ready(
    std::vector<shared_task<T>> tasks);
}
```

Example usage:
```c++
task<std::string> get_record(int id);

task<> example1()
{
  // Run 3 get_record() operations concurrently and wait until they're all ready.
  // Returns a std::tuple of tasks that can be unpacked using structured bindings.
  auto [task1, task2, task3] = co_await when_all_ready(
    get_record(123),
    get_record(456),
    get_record(789));

  // Unpack the result of each task (this will complete immediately)
  std::string& record1 = co_await task1;
  std::string& record2 = co_await task2;
  std::string& record3 = co_await task3;

  // Use records....
}

task<> example2()
{
  // Create the input tasks. They don't start executing yet.
  std::vector<task<std::string>> tasks;
  for (int i = 0; i < 1000; ++i)
  {
    tasks.emplace_back(get_record(i));
  }

  // Execute all tasks concurrently.
  // Returns the input vector of tasks.
  tasks = co_await when_all_ready(std::move(tasks));

  // Unpack and handle each result individually once they're all complete.
  for (int i = 0; i < 1000; ++i)
  {
    try
    {
      std::string& record = co_await tasks[i];
      std::cout << i << " = " << record << std::endl;
    }
    catch (const std::exception& ex)
    {
      std::cout << i << " : " << ex.what() << std::endl;
    }
  }
}
```

## `when_all()`

The `when_all()` function can be used to create a new `task` that will complete
when all of the input tasks have completed, and will return an aggregate of all of the
individual results.

When the returned `task` is awaited, it will start execution of all of the input
tasks on the current thread. Once the first task suspends, the second task will be started,
and so on. The tasks execute concurrently until they have all run to completion.

Once all input tasks have run to completion, an aggregate of the results is constructed
from each individual task result. If an exception is thrown by any of the input tasks
or if the construction of the aggregate result throws an exception then the exception
will propagate out of the `co_await` of the returned `task`.

If multiple tasks fail with an exception then one of the exceptions will propagate out
of the `when_all()` task and the other exceptions will be silently ignored. It is not
specified which task's exception will be chosen. If it is important to know which task(s)
failed then you should use `when_all_ready()` instead and `co_await` the result of each
task individually.

API Summary:
```c++
// <cppcoro/when_all.hpp>
namespace cppcoro
{
  // Variadic version.
  template<typename... TASKS>
  task<std::tuple<typename TASKS::value_type...>> when_all(TASKS... tasks);

  // Overloads for vector of value-returning tasks
  template<typename T>
  task<std::vector<T>> when_all(std::vector<task<T>> tasks);
  template<typename T>
  task<std::vector<T>> when_all(std::vector<shared_task<T>> tasks);

  // Overloads for vector of reference-returning tasks
  template<typename T>
  task<std::vector<std::reference_wrapper<T>>> when_all(std::vector<task<T&>> tasks);
  template<typename T>
  task<std::vector<std::reference_wrapper<T>>> when_all(std::vector<shared_task<T&>> tasks);

  // Overloads for vector of void-returning tasks
  task<> when_all(std::vector<task<>> tasks);
  task<> when_all(std::vector<shared_task<>> tasks);
}
```

Examples:
```c++
task<A> get_a();
task<B> get_b();

task<> example1()
{
  // Run get_a() and get_b() concurrently.
  // Task yields a std::tuple<A, B> which can be unpacked using structured bindings.
  auto [a, b] = co_await when_all(get_a(), get_b());

  // use a, b
}

task<std::string> get_record(int id);

task<> example2()
{
  std::vector<task<std::string>> tasks;
  for (int i = 0; i < 1000; ++i)
  {
    tasks.emplace_back(get_record(i));
  }

  // Concurrently execute all get_record() tasks.
  // If any of them fail with an exception then the exception will propagate
  // out of the co_await expression once they have all completed.
  std::vector<std::string> records = co_await when_all(std::move(tasks));

  // Process results
  for (int i = 0; i < 1000; ++i)
  {
    std::cout << i << " = " << records[i] << std::endl;
  }
}
```

## `fmap()`

The `fmap()` function can be used to apply a callable function to the value(s) contained within
a container-type, returning a new container-type of the results of applying the function the
contained value(s).

The `fmap()` function can apply a function to values of type `task<T>`, `shared_task<T>`, `generator<T>`,
`recursive_generator<T>` and `async_generator<T>`.

Each of these types provides an overload for `fmap()` that takes two arguments; a function to apply and the container value.
See documentation for each type for the supported `fmap()` overloads.

For example, the `fmap()` function can be used to apply a function to the eventual result of
a `task<T>`, producing a new `task<U>` that will complete with the return-value of the function.
```c++
// Given a function you want to apply that converts
// a value of type A to value of type B.
B a_to_b(A value);

// And a task that yields a value of type A
cppcoro::task<A> get_an_a();

// We can apply the function to the result of the task using fmap()
// and obtain a new task yielding the result.
cppcoro::task<B> bTask = fmap(a_to_b, get_an_a());

// An alternative syntax is to use the pipe notation.
cppcoro::task<B> bTask = get_an_a() | cppcoro::fmap(a_to_b);
```

API Summary:
```c++
// <cppcoro/fmap.hpp>
namespace cppcoro
{
  template<typename FUNC>
  struct fmap_transform
  {
    fmap_transform(FUNC&& func) noexcept(std::is_nothrow_move_constructible_v<FUNC>);
    FUNC func;
  };

  // Type-deducing constructor for fmap_transform object that can be used
  // in conjunction with operator|.
  template<typename FUNC>
  fmap_transform<FUNC> fmap(FUNC&& func);

  // operator| overloads for providing pipe-based syntactic sugar for fmap()
  // such that the expression:
  //   <value-expr> | cppcoro::fmap(<func-expr>)
  // is equivalent to:
  //   fmap(<func-expr>, <value-expr>)

  template<typename T, typename FUNC>
  decltype(auto) operator|(T&& value, fmap_transform<FUNC>&& transform);

  template<typename T, typename FUNC>
  decltype(auto) operator|(T&& value, fmap_transform<FUNC>& transform);

  template<typename T, typename FUNC>
  decltype(auto) operator|(T&& value, const fmap_transform<FUNC>& transform);
}
```

The `fmap()` function is designed to look up the correct overload by argument-dependent
lookup (ADL) so it should generally be called without the `cppcoro::` prefix.

## `resume_on()`

The `resume_on()` function can be used to control the execution context that a `task`,
`shared_task` or `async_generator` should resume its awaiting coroutine on.

Normally, the awaiter of a `task` or `async_generator` will resume execution on whatever
thread the `task` completed on. In some cases this may not be the thread that you want
to continue executing on. In these cases you can use the `resume_on()` function to create
a new task or generator that will resume execution on a thread associated with a specified
scheduler.

The `resume_on()` function can be used either as a normal function returning a new task/generator.
Or it can be used in a pipeline-syntax.

Example:
```c++
task<record> load_record(int id);

ui_thread_scheduler uiThreadScheduler;

task<> example()
{
  // This will start load_record() on the current thread.
  // Then when load_record() completes (probably on an I/O thread)
  // it will reschedule execution onto thread pool and call to_json
  // Once to_json completes it will transfer execution onto the
  // ui thread before resuming this coroutine and returning the json text.
  task<std::string> jsonTask =
    load_record(123)
    | cppcoro::resume_on(threadpool::default())
    | cppcoro::fmap(to_json)
    | cppcoro::resume_on(uiThreadScheduler);

  // At this point, all we've done is create a pipeline of tasks.
  // The tasks haven't started executing yet.

  // Await the result. Starts the pipeline of tasks.
  std::string jsonText = co_await jsonTask;

  // Guaranteed to be executing on ui thread here.

  someUiControl.set_text(jsonText);
}
```

API Summary:
```c++
// <cppcoro/resume_on.hpp>
namespace cppcoro
{
  template<typename SCHEDULER, typename T>
  task<T> resume_on(SCHEDULER& scheduler, task<T> t);

  template<typename SCHEDULER, typename T>
  task<T> resume_on(SCHEDULER& scheduler, shared_task<T> t);

  template<typename SCHEDULER, typename T>
  async_generator<T> resume_on(SCHEDULER& scheduler, async_generator<T> source);

  template<typename SCHEDULER>
  struct resume_on_transform
  {
    explicit resume_on_transform(SCHEDULER& scheduler) noexcept;
    SCHEDULER& scheduler;
  };

  // Construct a transform/operation that can be applied to a source object
  // using "pipe" notation (ie. operator|).
  template<typename SCHEDULER>
  resume_on_transform<SCHEDULER> resume_on(SCHEDULER& scheduler) noexcept;

  // Equivalent to 'resume_on(transform.scheduler, std::forward<T>(value))'
  template<typename T, typename SCHEDULER>
  decltype(auto) operator|(T&& value, resume_on_transform<SCHEDULER> transform)
  {
    return resume_on(transform.scheduler, std::forward<T>(value));
  }
}
```

## `schedule_on()`

The `schedule_on()` function can be used to change the execution context that a given
`task` or `async_generator` starts executing on.

When applied to an `async_generator` it also affects which execution context it resumes
on after `co_yield` statement.

Note that the `schedule_on` transform does not specify the thread that the `task` or `async_generator`
will complete or yield results on, that is up to the implementing coroutine.

See the `resume_on()` operator for a transform that controls the thread the operation completes on.

For example:
```c++
task<int> get_value();
io_service ioSvc;

task<> example()
{
  // Starts executing get_value() on the current thread.
  int a = co_await get_value();

  // Starts executing get_value() on a thread associated with ioSvc.
  int b = co_await schedule_on(ioSvc, get_value());
}
```

API Summary:
```
// <cppcoro/schedule_on.hpp>
namespace cppcoro
{
  // Return a task that yields the same result as 't' but that
  // ensures that 't' is co_await'ed on a thread associated with
  // the specified scheduler. Resulting task will complete on
  // whatever thread 't' would normally complete on.
  template<typename SCHEDULER, typename T>
  task<T> schedule_on(SCHEDULER& scheduler, task<T> t);

  // Return a generator that yields the same sequence of results as
  // 'source' but that ensures that execution of the coroutine starts
  // execution on a thread associated with 'scheduler' and resumes
  // after a 'co_yield' on a thread associated with 'scheduler'.
  template<typename SCHEDULER, typename T>
  async_generator<T> schedule_on(SCHEDULER& scheduler, async_generator<T> source);

  template<typename SCHEDULER>
  struct schedule_on_transform
  {
    explicit schedule_on_transform(SCHEDULER& scheduler) noexcept;
    SCHEDULER& scheduler;
  };

  template<typename SCHEDULER>
  schedule_on_transform<SCHEDULER> schedule_on(SCHEDULER& scheduler) noexcept;

  template<typename T, typename SCHEDULER>
  decltype(auto) operator|(T&& value, schedule_on_transform<SCHEDULER> transform);
}
```

# Concepts

## `Scheduler` concept

A `Scheduler` is a concept that allows scheduling execution of coroutines within some execution context.

```c++
concept Scheduler
{
  <awaitable-type> schedule();
}
```

Given a type, `S`, that implements the `Scheduler` concept, and an instance, `s`, of type `S`:
* The `s.schedule()` method returns an awaitable-type such that `co_await s.schedule()`
  will unconditionally suspend the current coroutine and schedule it for resumption on the
  execution context associated with the scheduler, `s`.
* The result of the `co_await s.schedule()` expression has type `void`.

```c++
cppcoro::task<> f(Scheduler& scheduler)
{
  // Execution of the coroutine is initially on the caller's execution context.

  // Suspends execution of the coroutine and schedules it for resumption on
  // the scheduler's execution context.
  co_await scheduler.schedule();

  // At this point the coroutine is now executing on the scheduler's
  // execution context.
}
```

## `DelayedScheduler` concept

A `DelayedScheduler` is a concept that allows a coroutine to schedule itself for execution on
the scheduler's execution context after a specified duration of time has elapsed.

```c++
concept DelayedScheduler : Scheduler
{
  template<typename REP, typename RATIO>
  <awaitable-type> schedule_after(std::chrono::duration<REP, RATIO> delay);

  template<typename REP, typename RATIO>
  <awaitable-type> schedule_after(
    std::chrono::duration<REP, RATIO> delay,
    cppcoro::cancellation_token cancellationToken);
}
```

Given a type, `S`, that implements the `DelayedScheduler` and an instance, `s` of type `S`:
* The `s.schedule_after(delay)` method returns an object that can be awaited
  such that `co_await s.schedule_after(delay)` suspends the current coroutine
  for a duration of `delay` before scheduling the coroutine for resumption on
  the execution context associated with the scheduler, `s`.
* The `co_await s.schedule_after(delay)` expression has type `void`.

# Building

The cppcoro library supports building under Windows with Visual Studio 2017 and Linux with Clang 5.0+.

This library makes use of the [Cake build system](https://github.com/lewissbaker/cake) (no, not the [C# one](http://cakebuild.net/)).

The cake build system is checked out automatically as a git submodule so you don't need to download or install it separately.

## Building on Windows

This library currently requires Visual Studio 2017 or later and the Windows 10 SDK.

Support for Clang ([#3](https://github.com/lewissbaker/cppcoro/issues/3)) and Linux ([#15](https://github.com/lewissbaker/cppcoro/issues/15)) is planned.

### Prerequisites

The Cake build-system is implemented in Python and requires Python 2.7 to be installed.

Ensure Python 2.7 interpreter is in your PATH and available as 'python'.

Ensure Visual Studio 2017 Update 3 or later is installed.
Note that there are some known issues with coroutines in Update 2 or earlier that have been fixed in Update 3.

You can also use an experimental version of the Visual Studio compiler by downloading a NuGet package from https://vcppdogfooding.azurewebsites.net/ and unzipping the .nuget file to a directory.
Just update the `config.cake` file to point at the unzipped location by modifying and uncommenting the following line:
```python
nugetPath = None # r'C:\Path\To\VisualCppTools.14.0.25224-Pre'
```

Ensure that you have the Windows 10 SDK installed.
It will use the latest Windows 10 SDK and Universal C Runtime version by default.

### Cloning the repository

The cppcoro repository makes use of git submodules to pull in the source for the Cake build system.

This means you need to pass the `--recursive` flag to the `git clone` command. eg.
```
c:\Code> git clone --recursive https://github.com/lewissbaker/cppcoro.git
```

If you have already cloned cppcoro, then you should update the submodules after pulling changes.
```
c:\Code\cppcoro> git submodule update --init --recursive
```

### Building from the command-line

To build from the command-line just run 'cake.bat' in the workspace root.

eg.
```
C:\cppcoro> cake.bat
Building with C:\cppcoro\config.cake - Variant(release='debug', platform='windows', architecture='x86', compilerFamily='msvc', compiler='msvc14.10')
Building with C:\cppcoro\config.cake - Variant(release='optimised', platform='windows', architecture='x64', compilerFamily='msvc', compiler='msvc14.10')
Building with C:\cppcoro\config.cake - Variant(release='debug', platform='windows', architecture='x64', compilerFamily='msvc', compiler='msvc14.10')
Building with C:\cppcoro\config.cake - Variant(release='optimised', platform='windows', architecture='x86', compilerFamily='msvc', compiler='msvc14.10')
Compiling test\main.cpp
Compiling test\main.cpp
Compiling test\main.cpp
Compiling test\main.cpp
...
Linking build\windows_x86_msvc14.10_debug\test\run.exe
Linking build\windows_x64_msvc14.10_optimised\test\run.exe
Linking build\windows_x86_msvc14.10_optimised\test\run.exe
Linking build\windows_x64_msvc14.10_debug\test\run.exe
Generating code
Finished generating code
Generating code
Finished generating code
Build succeeded.
Build took 0:00:02.419.
```

By default, running `cake` with no arguments will build all projects with all build variants and execute the unit-tests.
You can narrow what is built by passing additional command-line arguments.
eg.
```
c:\cppcoro> cake.bat release=debug architecture=x64 lib/build.cake
Building with C:\Users\Lewis\Code\cppcoro\config.cake - Variant(release='debug', platform='windows', architecture='x64', compilerFamily='msvc', compiler='msvc14.10')
Archiving build\windows_x64_msvc14.10_debug\lib\cppcoro.lib
Build succeeded.
Build took 0:00:00.321.
```

You can run `cake --help` to list available command-line options.

### Building Visual Studio project files

To develop from within Visual Studio you can build .vcproj/.sln files by running `cake.bat -p`.

eg.
```
c:\cppcoro> cake.bat -p
Building with C:\cppcoro\config.cake - Variant(release='debug', platform='windows', architecture='x86', compilerFamily='msvc', compiler='msvc14.10')
Building with C:\cppcoro\config.cake - Variant(release='optimised', platform='windows', architecture='x64', compilerFamily='msvc', compiler='msvc14.10')
Building with C:\cppcoro\config.cake - Variant(release='debug', platform='windows', architecture='x64', compilerFamily='msvc', compiler='msvc14.10')
Building with C:\cppcoro\config.cake - Variant(release='optimised', platform='windows', architecture='x86', compilerFamily='msvc', compiler='msvc14.10')
Generating Solution build/project/cppcoro.sln
Generating Project build/project/cppcoro_tests.vcxproj
Generating Filters build/project/cppcoro_tests.vcxproj.filters
Generating Project build/project/cppcoro.vcxproj
Generating Filters build/project/cppcoro.vcxproj.filters
Build succeeded.
Build took 0:00:00.247.
```

When you build these projects from within Visual Studio it will call out to cake to perform the compilation.

## Building on Linux

The cppcoro project can also be built under Linux using Clang + libc++ 5.0 or later.

Building cppcoro has been tested under Ubuntu 17.04.

### Prerequisities

Ensure you have the following packages installed:
* Python 2.7
* Clang >= 5.0
* LLD >= 5.0
* libc++ >= 5.0


### Building cppcoro

This is assuming you have Clang and libc++ built and installed.

If you don't have Clang configured yet, see the following sections
for details on setting up Clang for building with cppcoro.

Checkout cppcoro and its submodules:
```
git clone --recursive https://github.com/lewissbaker/cppcoro.git cppcoro
```

Run `init.sh` to setup the `cake` bash function:
```
cd cppcoro
source init.sh
```

Then you can run `cake` from the workspace root to build cppcoro and run tests:
```
$ cake
```

You can specify additional command-line arguments to customise the build:
* `--help` will print out help for command-line arguments
* `--debug=run` will show the build command-lines being run
* `release=debug` or `release=optimised` will limit the build variant to
   either debug or optimised (by default it will build both).
* `lib/build.cake` will just build the cppcoro library and not the tests.
* `test/build.cake@task_tests.cpp` will just compile a particular source file

For example:
```
$ cake --debug=run release=debug lib/build.cake
```

### Customising location of Clang

If your clang compiler is not located at `/usr/bin/clang` then you need to
modify the `config.cake` file to tell cake where to find clang.

Edit the following line in `config.cake`:
```python
  # If you have built your own version of Clang, you can modify
  # this variable to point to the CMAKE_INSTALL_PREFIX for
  # where you have installed your clang/libcxx build.
  clangInstallPrefix = '/usr'
```

If you have `libc++` installed in a different location then you can
customise its location by modifying the following line in `config.cake`.
```python
  # Set this to the install-prefix of where libc++ is installed.
  # You only need to set this if it is not installed at the same
  # location as clangInstallPrefix.
  libCxxInstallPrefix = None # '/path/to/install'
```

If the install location has multiple versions of Clang installed and
the one you want to use is not `<install-prefix>/bin/clang` then you
can explicitly specify which one to use by modifying the `config.cake`
file to specify the name of the clang binaries:
```python
  compiler = ClangCompiler(
    configuration=configuration,
    clangExe=cake.path.join(clangBinPath, 'clang-6.0'),
    llvmArExe=cake.path.join(clangBinPath, 'llvm-ar-6.0'),
    binPaths=[clangBinPath])
```

### Using a snapshot build of Clang

If your Linux distribution does not have a version of Clang 5.0 or later
available, you can install a snapshot build from the LLVM project.

Follow instructions at http://apt.llvm.org/ to setup your package manager
to support pulling from the LLVM package manager.

For example, for Ubuntu 17.04 Zesty:

Edit `/etc/apt/sources.list` and add the following lines:
```
deb http://apt.llvm.org/zesty/ llvm-toolchain-zesty main
deb-src http://apt.llvm.org/zesty/ llvm-toolchain-zesty main
```

Install the PGP key for those packages:
```
$ wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
```

Install Clang and LLD:
```
$ sudo apt-get install clang-6.0 lld-6.0
```

The LLVM snapshot builds do not include libc++ versions so you'll need to build that yourself.
See below.

### Building your own Clang

You can also use the bleeding-edge Clang version by building Clang from source yourself.

See instructions here: 

To do this you will need to install the following pre-requisites:
```
$ sudo apt-get install git cmake ninja-build clang lld
```

Note that we are using your distribution's version of clang to build
clang from source. GCC could also be used here instead.


Checkout LLVM + Clang + LLD + libc++ repositories:
```
mkdir llvm
cd llvm
git clone --depth=1 https://github.com/llvm-mirror/llvm.git llvm
git clone --depth=1 https://github.com/llvm-mirror/clang.git llvm/tools/clang
git clone --depth=1 https://github.com/llvm-mirror/lld.git llvm/tools/lld
git clone --depth=1 https://github.com/llvm-mirror/libcxx.git llvm/projects/libcxx
ln -s llvm/tools/clang clang
ln -s llvm/tools/lld lld
ln -s llvm/projects/libcxx libcxx
```

Configure and build Clang:
```
mkdir clang-build
cd clang-build
cmake -GNinja \
      -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
      -DCMAKE_C_COMPILER=/usr/bin/clang \
      -DCMAKE_BUILD_TYPE=MinSizeRel \
      -DCMAKE_INSTALL_PREFIX="/path/to/clang/install"
      -DCMAKE_BUILD_WITH_INSTALL_RPATH="yes" \
      -DLLVM_TARGETS_TO_BUILD=X86 \
      -DLLVM_ENABLE_PROJECTS="lld;clang" \
      ../llvm
ninja install-clang \
      install-clang-headers \
      install-llvm-ar \
      install-lld
```

### Building libc++

The cppcoro project requires libc++ as it contains the `<experimental/coroutine>`
header required to use C++ coroutines under Clang.

Checkout `libc++` + `llvm`:
```
mkdir llvm
cd llvm
git clone --depth=1 https://github.com/llvm-mirror/llvm.git llvm
git clone --depth=1 https://github.com/llvm-mirror/libcxx.git llvm/projects/libcxx
ln -s llvm/projects/libcxx libcxx
```

Build `libc++`:
```
mkdir libcxx-build
cd libcxx-build
cmake -GNinja \
      -DCMAKE_CXX_COMPILER="/path/to/clang/install/bin/clang++" \
      -DCMAKE_C_COMPILER="/path/to/clang/install/bin/clang" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="/path/to/clang/install" \
      -DLLVM_PATH="../llvm" \
      -DLIBCXX_CXX_ABI=libstdc++ \
      -DLIBCXX_CXX_ABI_INCLUDE_PATHS="/usr/include/c++/6.3.0/;/usr/include/x86_64-linux-gnu/c++/6.3.0/" \
      ../libcxx
ninja cxx
ninja install
```

This will build and install libc++ into the same install directory where you have clang installed.

# Support

GitHub issues are the primary mechanism for support, bug reports and feature requests.

Contributions are welcome and pull-requests will be happily reviewed.
I only ask that you agree to license any contributions that you make under the MIT license.

If you have general questions about C++ coroutines, you can generally find someone to help
in the `#coroutines` channel on [https://cpplang.slack.com/][Cpplang Slack] group.
