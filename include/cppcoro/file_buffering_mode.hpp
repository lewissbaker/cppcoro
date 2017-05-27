///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_FILE_BUFFERING_MODE_HPP_INCLUDED
#define CPPCORO_FILE_BUFFERING_MODE_HPP_INCLUDED

namespace cppcoro
{
	enum class file_buffering_mode
	{
		default_ = 0,
		sequential = 1,
		random_access = 2,
		unbuffered = 4,
		write_through = 8,
		temporary = 16
	};

	constexpr file_buffering_mode operator&(file_buffering_mode a, file_buffering_mode b)
	{
		return static_cast<file_buffering_mode>(
			static_cast<int>(a) & static_cast<int>(b));
	}

	constexpr file_buffering_mode operator|(file_buffering_mode a, file_buffering_mode b)
	{
		return static_cast<file_buffering_mode>(static_cast<int>(a) | static_cast<int>(b));
	}
}

#endif
