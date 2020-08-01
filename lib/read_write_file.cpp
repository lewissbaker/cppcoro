///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/read_write_file.hpp>

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <Windows.h>
#else
#define GENERIC_READ (S_IRUSR | S_IRGRP | S_IROTH)
#define GENERIC_WRITE (S_IWUSR | S_IWGRP /* | S_IWOTH */)
#endif

cppcoro::read_write_file cppcoro::read_write_file::open(
	io_service& ioService,
	const stdcoro::filesystem::path& path,
	file_open_mode openMode,
	file_share_mode shareMode,
	file_buffering_mode bufferingMode)
{
	auto file = read_write_file(file::open(
		GENERIC_READ | GENERIC_WRITE,
		ioService,
		path,
		openMode,
		shareMode,
		bufferingMode));
#ifdef CPPCORO_OS_LINUX
	file.m_ioService = &ioService;
#endif
	return std::move(file);
}

cppcoro::read_write_file::read_write_file(
	detail::safe_handle&& fileHandle) noexcept
	: file(std::move(fileHandle))
	, readable_file(detail::safe_handle{})
	, writable_file(detail::safe_handle{})
{
}
