#ifndef CPPCORO_STDCORO_HPP_INCLUDED
#define CPPCORO_STDCORO_HPP_INCLUDED

#include <filesystem>

#ifdef HAS_STD_COROUTINE_HEADER
#include <coroutine>
namespace stdcoro = std;
#else
#include <experimental/coroutine>
namespace stdcoro = std::experimental;
#endif

#ifdef HAS_STD_FILESYSTEM_HEADER
#include <filesystem>
namespace stdfs = std::filesystem;
#else
#include <experimental/filesystem>
namespace stdfs = std::experimental::filesystem;
#endif

#endif // CPPCORO_STDCORO_HPP_INCLUDED
