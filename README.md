# CppCoro - A coroutine library for C++

The 'cppcoro' library provides a large set of general-purpose primitives for making use of the coroutines TS proposal described in [N4680](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/n4680.pdf).

These include:
* Coroutine Types
  * [`task<T>`](#taskt)
  * [`shared_task<T>`](#shared_taskt)
  * [`generator<T>`](#generatort)
  * [`recursive_generator<T>`](#recursive_generatort)
  * [`async_generator<T>`](#async_generatort)
* Awaitable Types
  * [`single_consumer_event`](#single_consumer_event)
  * [`single_consumer_async_auto_reset_event`](#single_consumer_async_auto_reset_event)
  * [`async_mutex`](#async_mutex)
  * [`async_manual_reset_event`](#async_manual_reset_event)
  * [`async_auto_reset_event`](#async_auto_reset_event)
  * [`async_latch`](#async_latch)
  * [`sequence_barrier`](#sequence_barrier)
  * [`multi_producer_sequencer`](#multi_producer_sequencer)
  * [`single_producer_sequencer`](#single_producer_sequencer)
* Functions
  * [`sync_wait()`](#sync_wait)
  * [`when_all()`](#when_all)
  * [`when_all_ready()`](#when_all_ready)
  * [`fmap()`](#fmap)
  * [`schedule_on()`](#schedule_on)
  * [`resume_on()`](#resume_on)
* [Cancellation](#Cancellation)
  * `cancellation_token`
  * `cancellation_source`
  * `cancellation_registration`
* Schedulers and I/O
  * [`static_thread_pool`](#static_thread_pool)
  * [`io_service` and `io_work_scope`](#io_service-and-io_work_scope)
  * [`file`, `readable_file`, `writable_file`](#file-readable_file-writable_file)
  * [`read_only_file`, `write_only_file`, `read_write_file`](#read_only_file-write_only_file-read_write_file)
* Networking
  * [`socket`](#socket)
  * [`ip_address`, `ipv4_address`, `ipv6_address`](#ip_address-ipv4_address-ipv6_address)
  * [`ip_endpoint`, `ipv4_endpoint`, `ipv6_endpoint`](#ip_endpoint-ipv4_endpoint-ipv6_endpoint)
* Metafunctions
  * [`is_awaitable<T>`](#is_awaitablet)
  * [`awaitable_traits<T>`](#awaitable_traitst)
* Concepts
  * [`Awaitable<T>`](#Awaitablet-concept)
  * [`Awaiter<T>`](#Awaitert-concept)
  * [`Scheduler`](#Scheduler-concept)
  * [`DelayedScheduler`](#DelayedScheduler-concept)

This library is an experimental library that is exploring the space of high-performance,
scalable asynchronous programming abstractions that can be built on top of the C++ coroutines
proposal.

It has been open-sourced in the hope that others will find it useful and that the C++ community
can provide feedback on it and ways to improve it.

It requires a compiler that supports the coroutines TS:
- Windows + Visual Studio 2017 [![Windows Build Status](https://ci.appveyor.com/api/projects/status/github/lewissbaker/cppcoro?branch=master&svg=true&passingText=master%20-%20OK&failingText=master%20-%20Failing&pendingText=master%20-%20Pending)](https://ci.appveyor.com/project/lewissbaker/cppcoro/branch/master)
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
    Awaiter<T&> operator co_await() const & noexcept;
    Awaiter<T&&> operator co_await() const && noexcept;

    // Returns an awaitable that can be co_await'ed to suspend the current
    // coroutine until the task completes.
    //
    // The 'co_await t.when_ready()' expression differs from 'co_await t' in
    // that when_ready() only performs synchronization, it does not return
    // the result or rethrow the exception.
    //
    // This can be useful if you want to synchronize with the task without
    // the possibility of it throwing an exception.
    Awaitable<void> when_ready() const noexcept;
  };

  template<typename T>
  void swap(task<T>& a, task<T>& b);

  // Creates a task that yields the result of co_await'ing the specified awaitable.
  //
  // This can be used as a form of type-erasure of the concrete awaitable, allowing
  // different awaitables that return the same await-result type to be stored in
  // the same task<RESULT> type.
  template<
    typename AWAITABLE,
    typename RESULT = typename awaitable_traits<AWAITABLE>::await_result_t>
  task<RESULT> make_task(AWAITABLE awaitable);
}
```

You can create a `task<T>` object by calling a coroutine function that returns
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

If the `task` object is destroyed before it is awaited then the coroutine
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
Subsequent awaiters will either be suspended and be queued for resumption
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
    Awaiter<T&> operator co_await() const noexcept;

    // Returns an operation that when awaited will suspend the
    // calling coroutine until the task completes and the result
    // is available.
    //
    // The result is not returned from the co_await expression.
    // This can be used to synchronize with the task without the
    // possibility of the co_await expression throwing an exception.
    Awaiter<void> when_ready() const noexcept;

  };

  template<typename T>
  bool operator==(const shared_task<T>& a, const shared_task<T>& b) noexcept;
  template<typename T>
  bool operator!=(const shared_task<T>& a, const shared_task<T>& b) noexcept;

  template<typename T>
  void swap(shared_task<T>& a, shared_task<T>& b) noexcept;

  // Wrap an awaitable value in a shared_task to allow multiple coroutines
  // to concurrently await the result.
  template<
    typename AWAITABLE,
    typename RESULT = typename awaitable_traits<AWAITABLE>::await_result_t>
  shared_task<RESULT> make_shared_task(AWAITABLE awaitable);
}
```

All const-methods on `shared_task<T>` are safe to call concurrently with other
const-methods on the same instance from multiple threads. It is not safe to call
non-const methods of `shared_task<T>` concurrently with any other method on the
same instance of a `shared_task<T>`.

### Comparison to `task<T>`

The `shared_task<T>` class is similar to `task<T>` in that the task does
not start execution immediately upon the coroutine function being called.
The task only starts executing when it is first awaited.

It differs from `task<T>` in that the resulting task object can be copied,
allowing multiple task objects to reference the same asynchronous result.
It also supports multiple coroutines concurrently awaiting the result of the task.

The trade-off is that the result is always an l-value reference to the
result, never an r-value reference (since the result may be shared) which
may limit ability to move-construct the result into a local variable.
It also has a slightly higher run-time cost due to the need to maintain
a reference count and support multiple awaiters.

## `generator<T>`

A `generator` represents a coroutine type that produces a sequence of values of type, `T`,
where values are produced lazily and synchronously.

The coroutine body is able to yield values of type `T` using the `co_yield` keyword.
Note, however, that the coroutine body is not able to use the `co_await` keyword;
values must be produced synchronously.

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

If the returned iterator is not equal to the `end()` iterator then dereferencing the iterator will
return a reference to the value passed to the `co_yield` statement.

Calling `operator++()` on the iterator will resume execution of the coroutine and continue until
either the next `co_yield` point is reached or the coroutine runs to completion().

Any unhandled exceptions thrown by the coroutine will propagate out of the `begin()` or
`operator++()` calls to the caller.

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
  auto sequence = ticker(10, tp);
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
      Awaitable<iterator&> operator++() noexcept;

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
    Awaitable<iterator> begin() noexcept;
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
    Awaiter<void> operator co_await() const noexcept;
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

This class provides an async synchronization primitive that allows a single coroutine to
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
    Awaiter<void> operator co_await() const noexcept;

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
    bool await_suspend(std::coroutine_handle<> awaiter) noexcept;
    void await_resume() const noexcept;
  };

  class async_mutex_scoped_lock_operation
  {
  public:
    bool await_ready() const noexcept;
    bool await_suspend(std::coroutine_handle<> awaiter) noexcept;
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

A manual-reset event is a coroutine/thread-synchronization primitive that allows one or more threads
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
    async_manual_reset_event_operation operator co_await() const noexcept;

    bool is_set() const noexcept;

    void set() noexcept;

    void reset() noexcept;

  };

  class async_manual_reset_event_operation
  {
  public:
    async_manual_reset_event_operation(async_manual_reset_event& event) noexcept;

    bool await_ready() const noexcept;
    bool await_suspend(std::coroutine_handle<> awaiter) noexcept;
    void await_resume() const noexcept;
  };
}
```

## `async_auto_reset_event`

An auto-reset event is a coroutine/thread-synchronization primitive that allows one or more threads
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
    bool await_suspend(std::coroutine_handle<> awaiter) noexcept;
    void await_resume() const noexcept;

  };
}
```

## `async_latch`

An async latch is a synchronization primitive that allows coroutines to asynchronously
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

## `sequence_barrier`

A `sequence_barrier` is a synchronization primitive that allows a single-producer
and multiple consumers to coordinate with respect to a monotonically increasing
sequence number.

A single producer advances the sequence number by publishing new sequence numbers
in a monotonically increasing order. One or more consumers can query the last
published sequence number and can wait until a particular sequence number has been
published.

A sequence barrier can be used to represent a cursor into a thread-safe producer/consumer
ring-buffer

See the LMAX Disruptor pattern for more background:
https://lmax-exchange.github.io/disruptor/files/Disruptor-1.0.pdf

API Synopsis:
```c++
namespace cppcoro
{
  template<typename SEQUENCE = std::size_t,
           typename TRAITS = sequence_traits<SEQUENCE>>
  class sequence_barrier
  {
  public:
    sequence_barrier(SEQUENCE initialSequence = TRAITS::initial_sequence) noexcept;
	~sequence_barrier();

	SEQUENCE last_published() const noexcept;

	// Wait until the specified targetSequence number has been published.
	//
	// If the operation does not complete synchronously then the awaiting
	// coroutine is resumed on the specified scheduler. Otherwise, the
	// coroutine continues without suspending.
	//
	// The co_await expression resumes with the updated last_published()
	// value, which is guaranteed to be at least 'targetSequence'.
	template<typename SCHEDULER>
	[[nodiscard]]
	Awaitable<SEQUENCE> wait_until_published(SEQUENCE targetSequence,
                                             SCHEDULER& scheduler) const noexcept;

    void publish(SEQUENCE sequence) noexcept;
  };
}
```

## `single_producer_sequencer`

A `single_producer_sequencer` is a synchronization primitive that can be used to
coordinate access to a ring-buffer for a single producer and one or more consumers.

A producer first acquires one or more slots in a ring-buffer, writes to the ring-buffer
elements corresponding to those slots, and then finally publishes the values written to
those slots. A producer can never produce more than 'bufferSize' elements in advance
of where the consumer has consumed up to.

A consumer then waits for certain elements to be published, processes the items and
then notifies the producer when it has finished processing items by publishing the
sequence number it has finished consuming in a `sequence_barrier` object.


API Synopsis:
```c++
// <cppcoro/single_producer_sequencer.hpp>
namespace cppcoro
{
  template<
    typename SEQUENCE = std::size_t,
    typename TRAITS = sequence_traits<SEQUENCE>>
  class single_producer_sequencer
  {
  public:
    using size_type = typename sequence_range<SEQUENCE, TRAITS>::size_type;

    single_producer_sequencer(
      const sequence_barrier<SEQUENCE, TRAITS>& consumerBarrier,
      std::size_t bufferSize,
      SEQUENCE initialSequence = TRAITS::initial_sequence) noexcept;

    // Publisher API:

    template<typename SCHEDULER>
    [[nodiscard]]
    Awaitable<SEQUENCE> claim_one(SCHEDULER& scheduler) noexcept;

    template<typename SCHEDULER>
    [[nodiscard]]
    Awaitable<sequence_range<SEQUENCE>> claim_up_to(
      std::size_t count,
      SCHEDULER& scheduler) noexcept;

    void publish(SEQUENCE sequence) noexcept;

    // Consumer API:

    SEQUENCE last_published() const noexcept;

    template<typename SCHEDULER>
    [[nodiscard]]
    Awaitable<SEQUENCE> wait_until_published(
      SEQUENCE targetSequence,
      SCHEDULER& scheduler) const noexcept;

  };
}
```

Example usage:
```c++
using namespace cppcoro;
using namespace std::chrono;

struct message
{
  int id;
  steady_clock::time_point timestamp;
  float data;
};

constexpr size_t bufferSize = 16384; // Must be power-of-two
constexpr size_t indexMask = bufferSize - 1;
message buffer[bufferSize];

task<void> producer(
  io_service& ioSvc,
  single_producer_sequencer<size_t>& sequencer)
{
  auto start = steady_clock::now();
  for (int i = 0; i < 1'000'000; ++i)
  {
    // Wait until a slot is free in the buffer.
    size_t seq = co_await sequencer.claim_one(ioSvc);

    // Populate the message.
    auto& msg = buffer[seq & indexMask];
    msg.id = i;
    msg.timestamp = steady_clock::now();
    msg.data = 123;

    // Publish the message.
    sequencer.publish(seq);
  }

  // Publish a sentinel
  auto seq = co_await sequencer.claim_one(ioSvc);
  auto& msg = buffer[seq & indexMask];
  msg.id = -1;
  sequencer.publish(seq);
}

task<void> consumer(
  static_thread_pool& threadPool,
  const single_producer_sequencer<size_t>& sequencer,
  sequence_barrier<size_t>& consumerBarrier)
{
  size_t nextToRead = 0;
  while (true)
  {
    // Wait until the next message is available
    // There may be more than one available.
    const size_t available = co_await sequencer.wait_until_published(nextToRead, threadPool);
    do {
      auto& msg = buffer[nextToRead & indexMask];
      if (msg.id == -1)
      {
        consumerBarrier.publish(nextToRead);
        co_return;
      }

      processMessage(msg);
    } while (nextToRead++ != available);

    // Notify the producer that we've finished processing
    // up to 'nextToRead - 1'.
    consumerBarrier.publish(available);
  }
}

task<void> example(io_service& ioSvc, static_thread_pool& threadPool)
{
  sequence_barrier<size_t> barrier;
  single_producer_sequencer<size_t> sequencer{barrier, bufferSize};

  co_await when_all(
    producer(tp, sequencer),
    consumer(tp, sequencer, barrier));
}
```

## `multi_producer_sequencer`

The `multi_producer_sequencer` class is a synchronization primitive that coordinates
access to a ring-buffer for multiple producers and one or more consumers.

For a single-producer variant see the `single_producer_sequencer` class.

Note that the ring-buffer must have a size that is a power-of-two. This is because
the implementation uses bitmasks instead of integer division/modulo to calculate
the offset into the buffer. Also, this allows the sequence number to safely wrap
around the 32-bit/64-bit value.

API Summary:
```c++
// <cppcoro/multi_producer_sequencer.hpp>
namespace cppcoro
{
  template<typename SEQUENCE = std::size_t,
           typename TRAITS = sequence_traits<SEQUENCE>>
  class multi_producer_sequencer
  {
  public:
    multi_producer_sequencer(
      const sequence_barrier<SEQUENCE, TRAITS>& consumerBarrier,
      SEQUENCE initialSequence = TRAITS::initial_sequence);

    std::size_t buffer_size() const noexcept;

    // Consumer interface
    //
    // Each consumer keeps track of their own 'lastKnownPublished' value
    // and must pass this to the methods that query for an updated last-known
    // published sequence number.

    SEQUENCE last_published_after(SEQUENCE lastKnownPublished) const noexcept;

    template<typename SCHEDULER>
    Awaitable<SEQUENCE> wait_until_published(
      SEQUENCE targetSequence,
      SEQUENCE lastKnownPublished,
      SCHEDULER& scheduler) const noexcept;

    // Producer interface

    // Query whether any slots available for claiming (approx.)
    bool any_available() const noexcept;

    template<typename SCHEDULER>
    Awaitable<SEQUENCE> claim_one(SCHEDULER& scheduler) noexcept;

    template<typename SCHEDULER>
    Awaitable<sequence_range<SEQUENCE, TRAITS>> claim_up_to(
      std::size_t count,
      SCHEDULER& scheduler) noexcept;

    // Mark the specified sequence number as published.
    void publish(SEQUENCE sequence) noexcept;

    // Mark all sequence numbers in the specified range as published.
    void publish(const sequence_range<SEQUENCE, TRAITS>& range) noexcept;
  };
}
```

## Cancellation

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

## `static_thread_pool`

The `static_thread_pool` class provides an abstraction that lets you schedule work
on a fixed-size pool of threads.

This class implements the **Scheduler** concept (see below).

You can enqueue work to the thread-pool by executing `co_await threadPool.schedule()`.
This operation will suspend the current coroutine, enqueue it for execution on the
thread-pool and the thread pool will then resume the coroutine when a thread in the
thread-pool is next free to run the coroutine. **This operation is guaranteed not
to throw and, in the common case, will not allocate any memory**.

This class makes use of a work-stealing algorithm to load-balance work across multiple
threads. Work enqueued to the thread-pool from a thread-pool thread will be scheduled
for execution on the same thread in a LIFO queue. Work enqueued to the thread-pool from
a remote thread will be enqueued to a global FIFO queue. When a worker thread runs out
of work from its local queue it first tries to dequeue work from the global queue. If
that queue is empty then it next tries to steal work from the back of the queues of
the other worker threads.

API Summary:
```c++
namespace cppcoro
{
  class static_thread_pool
  {
  public:
    // Initialise the thread-pool with a number of threads equal to
    // std::thread::hardware_concurrency().
    static_thread_pool();

    // Initialise the thread pool with the specified number of threads.
    explicit static_thread_pool(std::uint32_t threadCount);

    std::uint32_t thread_count() const noexcept;

    class schedule_operation
    {
    public:
      schedule_operation(static_thread_pool* tp) noexcept;

      bool await_ready() noexcept;
      bool await_suspend(std::coroutine_handle<> h) noexcept;
      bool await_resume() noexcept;

    private:
      // unspecified
    };

    // Return an operation that can be awaited by a coroutine.
    //
    //
    [[nodiscard]]
    schedule_operation schedule() noexcept;

  private:

    // Unspecified

  };
}
```

Example usage: Simple
```c++
cppcoro::task<std::string> do_something_on_threadpool(cppcoro::static_thread_pool& tp)
{
  // First schedule the coroutine onto the threadpool.
  co_await tp.schedule();

  // When it resumes, this coroutine is now running on the threadpool.
  do_something();
}
```

Example usage: Doing things in parallel - using `schedule_on()` operator with `static_thread_pool`.
```c++
cppcoro::task<double> dot_product(static_thread_pool& tp, double a[], double b[], size_t count)
{
  if (count > 1000)
  {
    // Subdivide the work recursively into two equal tasks
    // The first half is scheduled to the thread pool so it can run concurrently
    // with the second half which continues on this thread.
    size_t halfCount = count / 2;
    auto [first, second] = co_await when_all(
      schedule_on(tp, dot_product(tp, a, b, halfCount),
      dot_product(tp, a + halfCount, b + halfCount, count - halfCount));
    co_return first + second;
  }
  else
  {
    double sum = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
      sum += a[i] * b[i];
    }
    co_return sum;
  }
}
```

## `io_service` and `io_work_scope`

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
    void await_suspend(std::coroutine_handle<> awaiter) noexcept;
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
    void await_suspend(std::coroutine_handle<> awaiter);
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

namespace fs = std::filesystem;

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

An `io_service` class implements the interfaces for the `Scheduler` and `DelayedScheduler` concepts.

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
    bool await_suspend(std::coroutine_handle<> awaiter);
    std::size_t await_resume();

  };

  class file_write_operation
  {
  public:

    file_write_operation(file_write_operation&& other) noexcept;

    bool await_ready() const noexcept;
    bool await_suspend(std::coroutine_handle<> awaiter);
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
      const std::filesystem::path& path,
      file_share_mode shareMode = file_share_mode::read,
      file_buffering_mode bufferingMode = file_buffering_mode::default_);

  };

  class write_only_file : public writable_file
  {
  public:

    [[nodiscard]]
    static write_only_file open(
      io_service& ioService,
      const std::filesystem::path& path,
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
      const std::filesystem::path& path,
      file_open_mode openMode = file_open_mode::create_or_open,
      file_share_mode shareMode = file_share_mode::none,
      file_buffering_mode bufferingMode = file_buffering_mode::default_);

  };
}
```

All `open()` functions throw `std::system_error` on failure.

# Networking

NOTE: Networking abstractions are currently only supported on the Windows platform.
Linux support will be coming soon.

## `socket`

The socket class can be used to send/receive data over the network asynchronously.

Currently only supports TCP/IP, UDP/IP over IPv4 and IPv6.

API Summary:
```c++
// <cppcoro/net/socket.hpp>
namespace cppcoro::net
{
  class socket
  {
  public:

    static socket create_tcpv4(ip_service& ioSvc);
    static socket create_tcpv6(ip_service& ioSvc);
    static socket create_updv4(ip_service& ioSvc);
    static socket create_udpv6(ip_service& ioSvc);

    socket(socket&& other) noexcept;

    ~socket();

    socket& operator=(socket&& other) noexcept;

    // Return the native socket handle for the socket
    <platform-specific> native_handle() noexcept;

    const ip_endpoint& local_endpoint() const noexcept;
    const ip_endpoint& remote_endpoint() const noexcept;

    void bind(const ip_endpoint& localEndPoint);

    void listen();

    [[nodiscard]]
    Awaitable<void> connect(const ip_endpoint& remoteEndPoint) noexcept;
    [[nodiscard]]
    Awaitable<void> connect(const ip_endpoint& remoteEndPoint,
                            cancellation_token ct) noexcept;

    [[nodiscard]]
    Awaitable<void> accept(socket& acceptingSocket) noexcept;
    [[nodiscard]]
    Awaitable<void> accept(socket& acceptingSocket,
                           cancellation_token ct) noexcept;

    [[nodiscard]]
    Awaitable<void> disconnect() noexcept;
    [[nodiscard]]
    Awaitable<void> disconnect(cancellation_token ct) noexcept;

    [[nodiscard]]
    Awaitable<std::size_t> send(const void* buffer, std::size_t size) noexcept;
    [[nodiscard]]
    Awaitable<std::size_t> send(const void* buffer,
                                std::size_t size,
                                cancellation_token ct) noexcept;

    [[nodiscard]]
    Awaitable<std::size_t> recv(void* buffer, std::size_t size) noexcept;
    [[nodiscard]]
    Awaitable<std::size_t> recv(void* buffer,
                                std::size_t size,
                                cancellation_token ct) noexcept;

    [[nodiscard]]
    socket_recv_from_operation recv_from(
        void* buffer,
        std::size_t size) noexcept;
    [[nodiscard]]
    socket_recv_from_operation_cancellable recv_from(
        void* buffer,
        std::size_t size,
        cancellation_token ct) noexcept;

    [[nodiscard]]
    socket_send_to_operation send_to(
        const ip_endpoint& destination,
        const void* buffer,
        std::size_t size) noexcept;
    [[nodiscard]]
    socket_send_to_operation_cancellable send_to(
        const ip_endpoint& destination,
        const void* buffer,
        std::size_t size,
        cancellation_token ct) noexcept;

    void close_send();
    void close_recv();

  };
}
```

Example: Echo Server
```c++
#include <cppcoro/net/socket.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/async_scope.hpp>
#include <cppcoro/on_scope_exit.hpp>

#include <memory>
#include <iostream>

cppcoro::task<void> handle_connection(socket s)
{
  try
  {
    const size_t bufferSize = 16384;
    auto buffer = std::make_unique<unsigned char[]>(bufferSize);
    size_t bytesRead;
    do {
      // Read some bytes
      bytesRead = co_await s.recv(buffer.get(), bufferSize);

      // Write some bytes
      size_t bytesWritten = 0;
      while (bytesWritten < bytesRead) {
        bytesWritten += co_await s.send(
          buffer.get() + bytesWritten,
          bytesRead - bytesWritten);
      }
    } while (bytesRead != 0);

    s.close_send();

    co_await s.disconnect();
  }
  catch (...)
  {
    std::cout << "connection failed" << std::
  }
}

cppcoro::task<void> echo_server(
  cppcoro::net::ipv4_endpoint endpoint,
  cppcoro::io_service& ioSvc,
  cancellation_token ct)
{
  cppcoro::async_scope scope;

  std::exception_ptr ex;
  try
  {
    auto listeningSocket = cppcoro::net::socket::create_tcpv4(ioSvc);
    listeningSocket.bind(endpoint);
    listeningSocket.listen();

    while (true) {
      auto connection = cppcoro::net::socket::create_tcpv4(ioSvc);
      co_await listeningSocket.accept(connection, ct);
      scope.spawn(handle_connection(std::move(connection)));
    }
  }
  catch (cppcoro::operation_cancelled)
  {
  }
  catch (...)
  {
    ex = std::current_exception();
  }

  // Wait until all handle_connection tasks have finished.
  co_await scope.join();

  if (ex) std::rethrow_exception(ex);
}

int main(int argc, const char* argv[])
{
    cppcoro::io_service ioSvc;

    if (argc != 2) return -1;

    auto endpoint = cppcoro::ipv4_endpoint::from_string(argv[1]);
    if (!endpoint) return -1;

    (void)cppcoro::sync_wait(cppcoro::when_all(
        [&]() -> task<>
        {
            // Shutdown the event loop once finished.
            auto stopOnExit = cppcoro::on_scope_exit([&] { ioSvc.stop(); });

            cppcoro::cancellation_source canceller;
            co_await cppcoro::when_all(
                [&]() -> task<>
                {
                    // Run for 30s then stop accepting new connections.
                    co_await ioSvc.schedule_after(std::chrono::seconds(30));
                    canceller.request_cancellation();
                }(),
                echo_server(*endpoint, ioSvc, canceller.token()));
        }(),
        [&]() -> task<>
        {
            ioSvc.process_events();
        }()));

    return 0;
}
```

## `ip_address`, `ipv4_address`, `ipv6_address`

Helper classes for representing an IP address.

API Synopsis:
```c++
namespace cppcoro::net
{
  class ipv4_address
  {
    using bytes_t = std::uint8_t[4];
  public:
    constexpr ipv4_address();
    explicit constexpr ipv4_address(std::uint32_t integer);
    explicit constexpr ipv4_address(const std::uint8_t(&bytes)[4]);
    explicit constexpr ipv4_address(std::uint8_t b0,
                                    std::uint8_t b1,
                                    std::uint8_t b2,
                                    std::uint8_t b3);

    constexpr const bytes_t& bytes() const;

    constexpr std::uint32_t to_integer() const;

    static constexpr ipv4_address loopback();

    constexpr bool is_loopback() const;
    constexpr bool is_private_network() const;

    constexpr bool operator==(ipv4_address other) const;
    constexpr bool operator!=(ipv4_address other) const;
    constexpr bool operator<(ipv4_address other) const;
    constexpr bool operator>(ipv4_address other) const;
    constexpr bool operator<=(ipv4_address other) const;
    constexpr bool operator>=(ipv4_address other) const;

    std::string to_string();

    static std::optional<ipv4_address> from_string(std::string_view string) noexcept;
  };

  class ipv6_address
  {
    using bytes_t = std::uint8_t[16];
  public:
    constexpr ipv6_address();

    explicit constexpr ipv6_address(
      std::uint64_t subnetPrefix,
      std::uint64_t interfaceIdentifier);

    constexpr ipv6_address(
      std::uint16_t part0,
      std::uint16_t part1,
      std::uint16_t part2,
      std::uint16_t part3,
      std::uint16_t part4,
      std::uint16_t part5,
      std::uint16_t part6,
      std::uint16_t part7);

    explicit constexpr ipv6_address(
        const std::uint16_t(&parts)[8]);

    explicit constexpr ipv6_address(
        const std::uint8_t(bytes)[16]);

    constexpr const bytes_t& bytes() const;

    constexpr std::uint64_t subnet_prefix() const;
    constexpr std::uint64_t interface_identifier() const;

    static constexpr ipv6_address unspecified();
    static constexpr ipv6_address loopback();

    static std::optional<ipv6_address> from_string(std::string_view string) noexcept;

    std::string to_string() const;

    constexpr bool operator==(const ipv6_address& other) const;
    constexpr bool operator!=(const ipv6_address& other) const;
    constexpr bool operator<(const ipv6_address& other) const;
    constexpr bool operator>(const ipv6_address& other) const;
    constexpr bool operator<=(const ipv6_address& other) const;
    constexpr bool operator>=(const ipv6_address& other) const;

  };

  class ip_address
  {
  public:

    // Constructs to IPv4 address 0.0.0.0
    ip_address() noexcept;

    ip_address(ipv4_address address) noexcept;
    ip_address(ipv6_address address) noexcept;

    bool is_ipv4() const noexcept;
    bool is_ipv6() const noexcept;

    const ipv4_address& to_ipv4() const;
    const ipv6_address& to_ipv6() const;

    const std::uint8_t* bytes() const noexcept;

    std::string to_string() const;

    static std::optional<ip_address> from_string(std::string_view string) noexcept;

    bool operator==(const ip_address& rhs) const noexcept;
    bool operator!=(const ip_address& rhs) const noexcept;

    //  ipv4_address sorts less than ipv6_address
    bool operator<(const ip_address& rhs) const noexcept;
    bool operator>(const ip_address& rhs) const noexcept;
    bool operator<=(const ip_address& rhs) const noexcept;
    bool operator>=(const ip_address& rhs) const noexcept;

  };
}
```

## `ip_endpoint`, `ipv4_endpoint` `ipv6_endpoint`

Helper classes for representing an IP address and port-number.

API Synopsis:
```c++
namespace cppcoro::net
{
  class ipv4_endpoint
  {
  public:
    ipv4_endpoint() noexcept;
    explicit ipv4_endpoint(ipv4_address address, std::uint16_t port = 0) noexcept;

    const ipv4_address& address() const noexcept;
    std::uint16_t port() const noexcept;

    std::string to_string() const;
    static std::optional<ipv4_endpoint> from_string(std::string_view string) noexcept;
  };

  bool operator==(const ipv4_endpoint& a, const ipv4_endpoint& b);
  bool operator!=(const ipv4_endpoint& a, const ipv4_endpoint& b);
  bool operator<(const ipv4_endpoint& a, const ipv4_endpoint& b);
  bool operator>(const ipv4_endpoint& a, const ipv4_endpoint& b);
  bool operator<=(const ipv4_endpoint& a, const ipv4_endpoint& b);
  bool operator>=(const ipv4_endpoint& a, const ipv4_endpoint& b);

  class ipv6_endpoint
  {
  public:
    ipv6_endpoint() noexcept;
    explicit ipv6_endpoint(ipv6_address address, std::uint16_t port = 0) noexcept;

    const ipv6_address& address() const noexcept;
    std::uint16_t port() const noexcept;

    std::string to_string() const;
    static std::optional<ipv6_endpoint> from_string(std::string_view string) noexcept;
  };

  bool operator==(const ipv6_endpoint& a, const ipv6_endpoint& b);
  bool operator!=(const ipv6_endpoint& a, const ipv6_endpoint& b);
  bool operator<(const ipv6_endpoint& a, const ipv6_endpoint& b);
  bool operator>(const ipv6_endpoint& a, const ipv6_endpoint& b);
  bool operator<=(const ipv6_endpoint& a, const ipv6_endpoint& b);
  bool operator>=(const ipv6_endpoint& a, const ipv6_endpoint& b);

  class ip_endpoint
  {
  public:
     // Constructs to IPv4 end-point 0.0.0.0:0
     ip_endpoint() noexcept;

     ip_endpoint(ipv4_endpoint endpoint) noexcept;
     ip_endpoint(ipv6_endpoint endpoint) noexcept;

     bool is_ipv4() const noexcept;
     bool is_ipv6() const noexcept;

     const ipv4_endpoint& to_ipv4() const;
     const ipv6_endpoint& to_ipv6() const;

     ip_address address() const noexcept;
     std::uint16_t port() const noexcept;

     std::string to_string() const;

     static std::optional<ip_endpoint> from_string(std::string_view string) noexcept;

     bool operator==(const ip_endpoint& rhs) const noexcept;
     bool operator!=(const ip_endpoint& rhs) const noexcept;

     //  ipv4_endpoint sorts less than ipv6_endpoint
     bool operator<(const ip_endpoint& rhs) const noexcept;
     bool operator>(const ip_endpoint& rhs) const noexcept;
     bool operator<=(const ip_endpoint& rhs) const noexcept;
     bool operator>=(const ip_endpoint& rhs) const noexcept;
  };
}
```

# Functions

## `sync_wait()`

The `sync_wait()` function can be used to synchronously wait until the specified `awaitable`
completes.

The specified awaitable will be `co_await`ed on current thread inside a newly created coroutine.

The `sync_wait()` call will block until the operation completes and will return the result of
the `co_await` expression or rethrow the exception if the `co_await` expression completed with
an unhandled exception.

The `sync_wait()` function is mostly useful for starting a top-level task from within `main()`
and waiting until the task finishes, in practice it is the only way to start the first/top-level
`task`.

API Summary:
```c++
// <cppcoro/sync_wait.hpp>
namespace cppcoro
{
  template<typename AWAITABLE>
  auto sync_wait(AWAITABLE&& awaitable)
    -> typename awaitable_traits<AWAITABLE&&>::await_result_t;
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
  sync_wait(task); // -> "foo"
  sync_wait(makeTask()); // -> "foo"
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

The `when_all_ready()` function can be used to create a new awaitable that completes when
all of the input awaitables complete.

Input tasks can be any type of awaitable.

When the returned awaitable is `co_await`ed it will `co_await` each of the input awaitables
in turn on the awaiting thread in the order they are passed to the `when_all_ready()`
function. If these tasks to not complete synchronously then they will execute concurrently.

Once all of the `co_await` expressions on input awaitables have run to completion the
returned awaitable will complete and resume the awaiting coroutine. The awaiting coroutine
will be resumed on the thread of the input awaitable that is last to complete.

The returned awaitable is guaranteed not to throw an exception when `co_await`ed,
even if some of the input awaitables fail with an unhandled exception.

Note, however, that the `when_all_ready()` call itself may throw `std::bad_alloc` if it
was unable to allocate memory for the coroutine frames required to await each of the
input awaitables. It may also throw an exception if any of the input awaitable objects
throw from their copy/move constructors.

The result of `co_await`ing the returned awaitable is a `std::tuple` or `std::vector`
of `when_all_task<RESULT>` objects. These objects allow you to obtain the result (or exception)
of each input awaitable separately by calling the `when_all_task<RESULT>::result()`
method of the corresponding output task.
This allows the caller to concurrently await multiple awaitables and synchronize on
their completion while still retaining the ability to subsequently inspect the results of
each of the `co_await` operations for success/failure.

This differs from `when_all()` where the failure of any individual `co_await` operation
causes the overall operation to fail with an exception. This means you cannot determine
which of the component `co_await` operations failed and also prevents you from obtaining
the results of the other `co_await` operations.

API summary:
```c++
// <cppcoro/when_all_ready.hpp>
namespace cppcoro
{
  // Concurrently await multiple awaitables.
  //
  // Returns an awaitable object that, when co_await'ed, will co_await each of the input
  // awaitable objects and will resume the awaiting coroutine only when all of the
  // component co_await operations complete.
  //
  // Result of co_await'ing the returned awaitable is a std::tuple of detail::when_all_task<T>,
  // one for each input awaitable and where T is the result-type of the co_await expression
  // on the corresponding awaitable.
  //
  // AWAITABLES must be awaitable types and must be movable (if passed as rvalue) or copyable
  // (if passed as lvalue). The co_await expression will be executed on an rvalue of the
  // copied awaitable.
  template<typename... AWAITABLES>
  auto when_all_ready(AWAITABLES&&... awaitables)
    -> Awaitable<std::tuple<detail::when_all_task<typename awaitable_traits<AWAITABLES>::await_result_t>...>>;

  // Concurrently await each awaitable in a vector of input awaitables.
  template<
    typename AWAITABLE,
    typename RESULT = typename awaitable_traits<AWAITABLE>::await_result_t>
  auto when_all_ready(std::vector<AWAITABLE> awaitables)
    -> Awaitable<std::vector<detail::when_all_task<RESULT>>>;
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

  // Unpack the result of each task
  std::string& record1 = task1.result();
  std::string& record2 = task2.result();
  std::string& record3 = task3.result();

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
  std::vector<detail::when_all_task<std::string>> resultTasks =
    co_await when_all_ready(std::move(tasks));

  // Unpack and handle each result individually once they're all complete.
  for (int i = 0; i < 1000; ++i)
  {
    try
    {
      std::string& record = tasks[i].result();
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

The `when_all()` function can be used to create a new Awaitable that when `co_await`ed
will `co_await` each of the input awaitables concurrently and return an aggregate of
their individual results.

When the returned awaitable is awaited, it will `co_await` each of the input awaitables
on the current thread. Once the first awaitable suspends, the second task will be started,
and so on. The operations execute concurrently until they have all run to completion.

Once all component `co_await` operations have run to completion, an aggregate of the
results is constructed from each individual result. If an exception is thrown by any
of the input tasks or if the construction of the aggregate result throws an exception
then the exception will propagate out of the `co_await` of the returned awaitable.

If multiple `co_await` operations fail with an exception then one of the exceptions
will propagate out of the `co_await when_all()` expression the other exceptions will be silently
ignored. It is not specified which operation's exception will be chosen.

If it is important to know which component `co_await` operation failed or to retain
the ability to obtain results of other operations even if some of them fail then you
you should use `when_all_ready()` instead.

API Summary:
```c++
// <cppcoro/when_all.hpp>
namespace cppcoro
{
  // Variadic version.
  //
  // Note that if the result of `co_await awaitable` yields a void-type
  // for some awaitables then the corresponding component for that awaitable
  // in the tuple will be an empty struct of type detail::void_value.
  template<typename... AWAITABLES>
  auto when_all(AWAITABLES&&... awaitables)
    -> Awaitable<std::tuple<typename awaitable_traits<AWAITABLES>::await_result_t...>>;

  // Overload for vector<Awaitable<void>>.
  template<
    typename AWAITABLE,
    typename RESULT = typename awaitable_traits<AWAITABLE>::await_result_t,
    std::enable_if_t<std::is_void_v<RESULT>, int> = 0>
  auto when_all(std::vector<AWAITABLE> awaitables)
    -> Awaitable<void>;

  // Overload for vector<Awaitable<NonVoid>> that yield a value when awaited.
  template<
    typename AWAITABLE,
    typename RESULT = typename awaitable_traits<AWAITABLE>::await_result_t,
    std::enable_if_t<!std::is_void_v<RESULT>, int> = 0>
  auto when_all(std::vector<AWAITABLE> awaitables)
    -> Awaitable<std::vector<std::conditional_t<
         std::is_lvalue_reference_v<RESULT>,
         std::reference_wrapper<std::remove_reference_t<RESULT>>,
         std::remove_reference_t<RESULT>>>>;
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

The `fmap()` function can apply a function to values of type `generator<T>`, `recursive_generator<T>`
and `async_generator<T>` as well as any value that supports the `Awaitable` concept (eg. `task<T>`).

Each of these types provides an overload for `fmap()` that takes two arguments; a function to apply
and the container value.
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

  // Generic overload for all awaitable types.
  //
  // Returns an awaitable that when co_awaited, co_awaits the specified awaitable
  // and applies the specified func to the result of the 'co_await awaitable'
  // expression as if by 'std::invoke(func, co_await awaitable)'.
  //
  // If the type of 'co_await awaitable' expression is 'void' then co_awaiting the
  // returned awaitable is equivalent to 'co_await awaitable, func()'.
  template<
    typename FUNC,
    typename AWAITABLE,
    std::enable_if_t<is_awaitable_v<AWAITABLE>, int> = 0>
  auto fmap(FUNC&& func, AWAITABLE&& awaitable)
    -> Awaitable<std::invoke_result_t<FUNC, typename awaitable_traits<AWAITABLE>::await_result_t>>;
}
```

The `fmap()` function is designed to look up the correct overload by argument-dependent
lookup (ADL) so it should generally be called without the `cppcoro::` prefix.

## `resume_on()`

The `resume_on()` function can be used to control the execution context that an awaitable
will resume the awaiting coroutine on when awaited. When applied to an `async_generator`
it controls which execution context the `co_await g.begin()` and `co_await ++it` operations
resume the awaiting coroutines on.

Normally, the awaiting coroutine of an awaitable (eg. a `task`) or `async_generator` will
resume execution on whatever thread the operation completed on. In some cases this may not
be the thread that you want to continue executing on. In these cases you can use the
`resume_on()` function to create a new awaitable or generator that will resume execution
on a thread associated with a specified scheduler.

The `resume_on()` function can be used either as a normal function returning a new awaitable/generator.
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
  template<typename SCHEDULER, typename AWAITABLE>
  auto resume_on(SCHEDULER& scheduler, AWAITABLE awaitable)
    -> Awaitable<typename awaitable_traits<AWAITABLE>::await_traits_t>;

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
awaitable or `async_generator` starts executing on.

When applied to an `async_generator` it also affects which execution context it resumes
on after `co_yield` statement.

Note that the `schedule_on` transform does not specify the thread that the awaitable or
`async_generator` will complete or yield results on, that is up to the implementation of
the awaitable or generator.

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
```c++
// <cppcoro/schedule_on.hpp>
namespace cppcoro
{
  // Return a task that yields the same result as 't' but that
  // ensures that 't' is co_await'ed on a thread associated with
  // the specified scheduler. Resulting task will complete on
  // whatever thread 't' would normally complete on.
  template<typename SCHEDULER, typename AWAITABLE>
  auto schedule_on(SCHEDULER& scheduler, AWAITABLE awaitable)
    -> Awaitable<typename awaitable_traits<AWAITABLE>::await_result_t>;

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

# Metafunctions

## `awaitable_traits<T>`

This template metafunction can be used to determine what the resulting type of a `co_await` expression
will be if applied to an expression of type `T`.

Note that this assumes the value of type `T` is being awaited in a context where it is unaffected by
any `await_transform` applied by the coroutine's promise object. The results may differ if a value
of type `T` is awaited in such a context.

The `awaitable_traits<T>` template metafunction does not define the `awaiter_t` or `await_result_t`
nested typedefs if type, `T`, is not awaitable. This allows its use in SFINAE contexts that disables
overloads when `T` is not awaitable.

API Summary:
```c++
// <cppcoro/awaitable_traits.hpp>
namespace cppcoro
{
  template<typename T>
  struct awaitable_traits
  {
    // The type that results from applying `operator co_await()` to a value
    // of type T, if T supports an `operator co_await()`, otherwise is type `T&&`.
    typename awaiter_t = <unspecified>;

    // The type of the result of co_await'ing a value of type T.
    typename await_result_t = <unspecified>;
  };
}
```

## `is_awaitable<T>`

The `is_awaitable<T>` template metafunction allows you to query whether or not a given
type can be `co_await`ed or not from within a coroutine.

API Summary:
```c++
// <cppcoro/is_awaitable.hpp>
namespace cppcoro
{
  template<typename T>
  struct is_awaitable : std::bool_constant<...>
  {};

  template<typename T>
  constexpr bool is_awaitable_v = is_awaitable<T>::value;
}
```

# Concepts

## `Awaitable<T>` concept

An `Awaitable<T>` is a concept that indicates that a type can be `co_await`ed in a coroutine context
that has no `await_transform` overloads and that the result of the `co_await` expression has type, `T`.

For example, the type `task<T>` implements the concept `Awaitable<T&&>` whereas the type `task<T>&`
implements the concept `Awaitable<T&>`.

## `Awaiter<T>` concept

An `Awaiter<T>` is a concept that indicates a type contains the `await_ready`, `await_suspend` and
`await_resume` methods required to implement the protocol for suspending/resuming an awaiting
coroutine.

A type that satisfies `Awaiter<T>` must have, for an instance of the type, `awaiter`:
- `awaiter.await_ready()` -> `bool`
- `awaiter.await_suspend(std::coroutine_handle<void>{})` -> `void` or `bool` or `std::coroutine_handle<P>` for some `P`.
- `awaiter.await_resume()` -> `T`

Any type that implements the `Awaiter<T>` concept also implements the `Awaitable<T>` concept.

## `Scheduler` concept

A `Scheduler` is a concept that allows scheduling execution of coroutines within some execution context.

```c++
concept Scheduler
{
  Awaitable<void> schedule();
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
  Awaitable<void> schedule_after(std::chrono::duration<REP, RATIO> delay);

  template<typename REP, typename RATIO>
  Awaitable<void> schedule_after(
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
* `test/build.cake@testresult` will build and run the tests

For example:
```
$ cake --debug=run release=debug lib/build.cake
```

### Customising location of Clang

If your clang compiler is not located at `/usr/bin/clang` then you can specify an
alternative location using one or more of the following command-line options for `cake`:

* `--clang-executable=<name>` - Specify the clang executable name to use instead of `clang`.
  eg. to force use of Clang 8.0 pass `--clang-executable=clang-8`
* `--clang-executable=<abspath>` - Specify the full path to clang executable.
  The build system will also look for other executables in the same directory.
  If this path has the form `<prefix>/bin/<name>` then this will also set the default clang-install-prefix to `<prefix>`.
* `--clang-install-prefix=<path>` - Specify path where clang has been installed.
  This will cause the build system to look for clang under `<path>/bin` (unless overridden by `--clang-executable`).
* `--libcxx-install-prefix=<path>` - Specify path where libc++ has been installed.
  By default the build system will look for libc++ in the same location as clang.
  Use this command-line option if it is installed in a different location.

Example: Use a specific version of clang installed in the default location
```
$ cake --clang-executable=clang-8
```

Example: Use the default version of clang from a custom location
```
$ cake --clang-install-prefix=/path/to/clang-install
```

Example: Use a specific version of clang, in a custom location, with libc++ from a different location
```
$ cake --clang-executable=/path/to/clang-install/bin/clang-8 --libcxx-install-prefix=/path/to/libcxx-install
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

## Installing from vcpkg

The cppcoro port in vcpkg is kept up to date by Microsoft team members and community contributors. The url of vcpkg is: https://github.com/Microsoft/vcpkg . You can download and install cppcoro using the vcpkg dependency manager:

```shell
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh  # ./bootstrap-vcpkg.bat for Windows
./vcpkg integrate install
./vcpkg install cppcoro
```

If the version is out of date, please [create an issue or pull request](https://github.com/Microsoft/vcpkg) on the vcpkg repository.

# Support

GitHub issues are the primary mechanism for support, bug reports and feature requests.

Contributions are welcome and pull-requests will be happily reviewed.
I only ask that you agree to license any contributions that you make under the MIT license.

If you have general questions about C++ coroutines, you can generally find someone to help
in the `#coroutines` channel on [Cpplang Slack](https://cpplang.slack.com/) group.
