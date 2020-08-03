///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/read_only_file.hpp>

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <Windows.h>
#elif CPPCORO_OS_LINUX
#define GENERIC_READ (S_IRUSR | S_IRGRP | S_IROTH)
#endif

cppcoro::read_only_file cppcoro::read_only_file::open(
	io_service& ioService,
	const stdfs::path& path,
	file_share_mode shareMode,
	file_buffering_mode bufferingMode)
{
	read_only_file file(file::open(
		GENERIC_READ,
		ioService,
		path,
		file_open_mode::open_existing,
		shareMode,
		bufferingMode));
#ifdef CPPCORO_OS_LINUX
	file.m_ioService = &ioService;
#endif
	return std::move(file);
}

cppcoro::read_only_file::read_only_file(
	detail::safe_handle&& fileHandle) noexcept
	: file(std::move(fileHandle))
	, readable_file(detail::safe_handle{})
{
}

