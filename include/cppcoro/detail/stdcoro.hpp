#ifndef CPPCORO_STDCORO_HPP_INCLUDED
#define CPPCORO_STDCORO_HPP_INCLUDED

#if CPPCORO_HAS_STD_COROUTINE_HEADER
#include <coroutine>
namespace stdcoro = std;
#elif CPPCORO_HAS_STD_EXPERIMENTAL_COROUTINES_HEADER
#include <experimental/coroutine>
namespace stdcoro = std::experimental;
#else
#error "Coroutines are not supported"
#endif

#if CPPCORO_HAS_STD_FILESYSTEM_HEADER
#include <filesystem>
namespace stdfs = std::filesystem;
#elif CPPCORO_HAS_STD_EXPERIMENTAL_FILESYSTEM_HEADER
#include <experimental/filesystem>
namespace stdfs = std::experimental::filesystem;
#else
#error "Filesystem is not supported"
#endif

#endif // CPPCORO_STDCORO_HPP_INCLUDED
