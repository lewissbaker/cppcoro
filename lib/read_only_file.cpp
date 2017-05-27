///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro\read_only_file.hpp>

#if CPPCORO_OS_WINNT
# define WIN32_LEAN_AND_MEAN
# include <Windows.h>

cppcoro::read_only_file cppcoro::read_only_file::open(
	io_context& ioContext,
	const std::experimental::filesystem::path& path,
	file_share_mode shareMode,
	file_buffering_mode bufferingMode)
{
	return read_only_file(file::open(
		GENERIC_READ,
		ioContext,
		path,
		file_open_mode::open_existing,
		shareMode,
		bufferingMode));
}

cppcoro::read_only_file::read_only_file(
	detail::win32::safe_handle&& fileHandle) noexcept
	: file(std::move(fileHandle))
	, readable_file(detail::win32::safe_handle{})
{
}

#endif
