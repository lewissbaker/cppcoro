#ifndef CPPCORO_STDCORO_HPP_INCLUDED
#define CPPCORO_STDCORO_HPP_INCLUDED

#ifdef HAS_STD_COROUTINE_HEADER
#include <coroutine>
namespace stdcoro = std;
#else
#include <cppcoro/stdcoro.hpp>
namespace stdcoro = std::experimental;
#endif

#endif // CPPCORO_STDCORO_HPP_INCLUDED
