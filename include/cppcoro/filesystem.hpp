#ifndef CPPCORO_FILESYSTEM_HPP_INCLUDED
#define CPPCORO_FILESYSTEM_HPP_INCLUDED

#if __has_include(<filesystem>)

#include <filesystem>

namespace cppcoro {
    namespace filesystem = std::filesystem;
}

#elif __has_include(<experimental/filesystem>)

#include <experimental/filesystem>

namespace cppcoro {
    namespace filesystem = std::experimental::filesystem;
}

#else
#error Cppcoro requires a C++20 compiler with filesystem support
#endif

#endif
