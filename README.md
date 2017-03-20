# CppCoro - A coroutine library for C++

The 'cppcoro' library provides a set of general-purpose primitives for making use of the coroutines TS proposal described in (N4628)[http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/n4628.pdf].

These include:
* Coroutine Types
  * `task<T>`
  * `lazy_task<T>`
  * `shared_task<T>`
  * `shared_lazy_task<T>`
  * `generator<T>`
  * `async_generator<T>`
* Awaitable Types
  * `async_mutex`
  * `async_manual_reset_event` 
* Functions
  * `when_all()`
