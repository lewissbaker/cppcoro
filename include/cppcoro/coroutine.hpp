#ifndef CPPCORO_COROUTINE_HPP_INCLUDED
#define CPPCORO_COROUTINE_HPP_INCLUDED

#if __has_include(<coroutine>)

#include <coroutine>

namespace cppcoro {
  using std::coroutine_handle;
  using std::suspend_always;
  using std::noop_coroutine;
  using std::suspend_never;
}

#elif __has_include(<experimental/coroutine>)

#include <experimental/coroutine>

namespace cppcoro {
  using std::experimental::coroutine_handle;
  using std::experimental::suspend_always;
  using std::experimental::suspend_never;

#if CPPCORO_COMPILER_SUPPORTS_SYMMETRIC_TRANSFER
    using std::experimental::noop_coroutine;
#endif
}

#else
#error Cppcoro requires a C++20 compiler with coroutine support
#endif

#endif
