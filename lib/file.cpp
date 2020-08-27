///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/file.hpp>
#include <cppcoro/io_service.hpp>

#include <system_error>
#include <cassert>

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <Windows.h>
#elif CPPCORO_OS_LINUX
# include <fcntl.h>
# include <sys/stat.h>
#endif

cppcoro::file::~file()
{}

std::uint64_t cppcoro::file::size() const
{
#if CPPCORO_OS_WINNT
	LARGE_INTEGER size;
	BOOL ok = ::GetFileSizeEx(m_fileHandle.handle(), &size);
	if (!ok)
	{
		DWORD errorCode = ::GetLastError();
		throw std::system_error
		{
			static_cast<int>(errorCode),
			std::system_category(),
			"error getting file size: GetFileSizeEx"
		};
	}

	return size.QuadPart;
#elif CPPCORO_OS_LINUX
	struct stat sb;
	if (fstat(m_fileData.fd.get(), &sb) < 0)
	{
		throw std::system_error
		{
			errno,
			std::system_category(),
			"error getting file size: fstat"
		};
	}

	return sb.st_size;
#endif
}
#if CPPCORO_OS_WINNT

cppcoro::file::file(detail::win32::safe_handle&& fileHandle) noexcept
	: m_fileHandle(std::move(fileHandle))
{
}

cppcoro::detail::win32::safe_handle cppcoro::file::open(
	detail::win32::dword_t fileAccess,
	io_service& ioService,
	const std::experimental::filesystem::path& path,
	file_open_mode openMode,
	file_share_mode shareMode,
	file_buffering_mode bufferingMode)
{
	DWORD flags = FILE_FLAG_OVERLAPPED;
	if ((bufferingMode & file_buffering_mode::random_access) == file_buffering_mode::random_access)
	{
		flags |= FILE_FLAG_RANDOM_ACCESS;
	}
	if ((bufferingMode & file_buffering_mode::sequential) == file_buffering_mode::sequential)
	{
		flags |= FILE_FLAG_SEQUENTIAL_SCAN;
	}
	if ((bufferingMode & file_buffering_mode::write_through) == file_buffering_mode::write_through)
	{
		flags |= FILE_FLAG_WRITE_THROUGH;
	}
	if ((bufferingMode & file_buffering_mode::temporary) == file_buffering_mode::temporary)
	{
		flags |= FILE_ATTRIBUTE_TEMPORARY;
	}
	if ((bufferingMode & file_buffering_mode::unbuffered) == file_buffering_mode::unbuffered)
	{
		flags |= FILE_FLAG_NO_BUFFERING;
	}

	DWORD shareFlags = 0;
	if ((shareMode & file_share_mode::read) == file_share_mode::read)
	{
		shareFlags |= FILE_SHARE_READ;
	}
	if ((shareMode & file_share_mode::write) == file_share_mode::write)
	{
		shareFlags |= FILE_SHARE_WRITE;
	}
	if ((shareMode & file_share_mode::delete_) == file_share_mode::delete_)
	{
		shareFlags |= FILE_SHARE_DELETE;
	}

	DWORD creationDisposition = 0;
	switch (openMode)
	{
	case file_open_mode::create_or_open:
		creationDisposition = OPEN_ALWAYS;
		break;
	case file_open_mode::create_always:
		creationDisposition = CREATE_ALWAYS;
		break;
	case file_open_mode::create_new:
		creationDisposition = CREATE_NEW;
		break;
	case file_open_mode::open_existing:
		creationDisposition = OPEN_EXISTING;
		break;
	case file_open_mode::truncate_existing:
		creationDisposition = TRUNCATE_EXISTING;
		break;
	}

	// Open the file
	detail::win32::safe_handle fileHandle(
		::CreateFileW(
			path.wstring().c_str(),
			fileAccess,
			shareFlags,
			nullptr,
			creationDisposition,
			flags,
			nullptr));
	if (fileHandle.handle() == INVALID_HANDLE_VALUE)
	{
		const DWORD errorCode = ::GetLastError();
		throw std::system_error
		{
			static_cast<int>(errorCode),
			std::system_category(),
			"error opening file: CreateFileW"
		};
	}

	// Associate with the I/O service's completion port.
	const HANDLE result = ::CreateIoCompletionPort(
		fileHandle.handle(),
		ioService.native_iocp_handle(),
		0,
		0);
	if (result == nullptr)
	{
		const DWORD errorCode = ::GetLastError();
		throw std::system_error
		{
			static_cast<int>(errorCode),
			std::system_category(),
			"error opening file: CreateIoCompletionPort"
		};
	}

	// Configure I/O operations to avoid dispatching a completion event
	// to the I/O service if the operation completes synchronously.
	// This avoids unnecessary suspension/resuption of the awaiting coroutine.
	const BOOL ok = ::SetFileCompletionNotificationModes(
		fileHandle.handle(),
		FILE_SKIP_COMPLETION_PORT_ON_SUCCESS |
		FILE_SKIP_SET_EVENT_ON_HANDLE);
	if (!ok)
	{
		const DWORD errorCode = ::GetLastError();
		throw std::system_error
		{
			static_cast<int>(errorCode),
			std::system_category(),
			"error opening file: SetFileCompletionNotificationModes"
		};
	}

	return std::move(fileHandle);
}

#endif // CPPCORO_OS_WINNT

#if CPPCORO_OS_LINUX

cppcoro::file::file(detail::linux::safe_file_data &&fileData) noexcept
	: m_fileData(std::move(fileData))
{
}

cppcoro::detail::linux::safe_file_data cppcoro::file::open(
	io_service &ioService,
	const std::filesystem::path &path,
	cppcoro::file_open_mode openMode,
	cppcoro::file_share_mode shareMode,
	cppcoro::file_buffering_mode bufferingMode)
{
	int flags = 0;

	if ((bufferingMode & file_buffering_mode::temporary) == file_buffering_mode::temporary)
	{
		// TODO
	}
	if ((bufferingMode & file_buffering_mode::unbuffered) == file_buffering_mode::unbuffered)
	{
		// TODO
	}

	if ((shareMode & file_share_mode::read_write) == file_share_mode::read_write)
	{
		flags |= O_RDWR;
	}
	else if ((shareMode & file_share_mode::read) == file_share_mode::read)
	{
		flags |= O_RDONLY;
	}
	else if ((shareMode & file_share_mode::write) == file_share_mode::write)
	{
		flags |= O_WRONLY;
	}
	if ((shareMode & file_share_mode::delete_) == file_share_mode::delete_)
	{
		// TODO
	}

	switch (openMode)
	{
	case file_open_mode::create_or_open:
		flags |= O_CREAT;
		break;
	case file_open_mode::create_always:
		flags |= O_CREAT | O_TRUNC;
		break;
	case file_open_mode::create_new:
		flags |= O_EXCL;
		break;
	case file_open_mode::open_existing:
		// This is default.
		break;
	case file_open_mode::truncate_existing:
		flags |= O_TRUNC;
		break;
	}

	cppcoro::detail::linux::safe_file_descriptor fd(::open(path.c_str(), flags));
	if (fd.get() < 0)
	{
		throw std::system_error
		{
			errno,
			std::system_category(),
			"error opening file: open"
		};
	}

	//posix_fadvise(fd.get(), 0, 0, advice);

	return { std::move(fd), ioService.io_uring_context() };
}

#endif // CPPCORO_OS_LINUX
