///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_PATH_HPP_INCLUDED
#define CPPCORO_PATH_HPP_INCLUDED

#if __has_include(<filesystem>)
# include <filesystem>
#else
# include <experimental/filesystem>
#endif

namespace cppcoro
{
#if __cpp_lib_filesystem >= 201703L
	using path = std::filesystem::path;
#else
	using path = std::experimental::filesystem::path;
#endif
}

#endif
