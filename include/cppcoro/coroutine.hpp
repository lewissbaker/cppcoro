#ifndef CPPCORO_COROUTINE_HPP_INCLUDED
#define CPPCORO_COROUTINE_HPP_INCLUDED

#if __has_include(<coroutine>)

#include <coroutine>

namespace cppcoro {
  template<typename Promise=void>
  using coroutine_handle = std::coroutine_handle<Promise>;

  using suspend_always = std::suspend_always;
  using suspend_never = std::suspend_never;
  static inline auto noop_coroutine() { return std::noop_coroutine(); }
}

#elif __has_include(<experimental/coroutine>)

#include <experimental/coroutine>

namespace cppcoro {
  template<typename Promise=void>
  using coroutine_handle = std::experimental::coroutine_handle<Promise>;

  using suspend_always = std::experimental::suspend_always;
  using suspend_never = std::experimental::suspend_never;
  static inline auto noop_coroutine() { return std::experimental::noop_coroutine(); }
}

#else
#error Cppcoro requires a C++20 compiler with coroutine support
#endif

#endif
