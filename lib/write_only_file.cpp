///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/write_only_file.hpp>

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <Windows.h>
#elif CPPCORO_OS_LINUX
#define GENERIC_WRITE 0
#endif

cppcoro::write_only_file cppcoro::write_only_file::open(
	io_service& ioService,
	const stdfs::path& path,
	file_open_mode openMode,
	file_share_mode shareMode,
	file_buffering_mode bufferingMode)
{
	auto file = write_only_file(file::open(
		GENERIC_WRITE,
		ioService,
		path,
		openMode,
		shareMode,
		bufferingMode));
#if CPPCORO_OS_LINUX
	file.m_ioService = &ioService;
#endif
	return std::move(file);
}

cppcoro::write_only_file::write_only_file(
	detail::safe_handle&& fileHandle) noexcept
	: file(std::move(fileHandle))
	, writable_file(detail::safe_handle{})
{
}

