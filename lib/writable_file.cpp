///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/writable_file.hpp>

#include <system_error>

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <Windows.h>

void cppcoro::writable_file::set_size(
	std::uint64_t fileSize)
{
	LARGE_INTEGER position;
	position.QuadPart = fileSize;

	BOOL ok = ::SetFilePointerEx(m_fileHandle.handle(), position, nullptr, FILE_BEGIN);
	if (!ok)
	{
		DWORD errorCode = ::GetLastError();
		throw std::system_error
		{
			static_cast<int>(errorCode),
			std::system_category(),
			"error setting file size: SetFilePointerEx"
		};
	}

	ok = ::SetEndOfFile(m_fileHandle.handle());
	if (!ok)
	{
		DWORD errorCode = ::GetLastError();
		throw std::system_error
		{
			static_cast<int>(errorCode),
			std::system_category(),
			"error setting file size: SetEndOfFile"
		};
	}
}
#else

void cppcoro::writable_file::set_size(
    std::uint64_t fileSize)
{
    if (ftruncate(m_fileHandle.fd(), fileSize) < 0) {
        throw std::system_error
            {
                static_cast<int>(errno),
                std::system_category(),
                "error setting file size: ftruncate"
            };
    }
}

#endif

cppcoro::file_write_operation cppcoro::writable_file::write(
	std::uint64_t offset,
	const void* buffer,
	std::size_t byteCount) noexcept
{
	return file_write_operation{
#if CPPCORO_OS_LINUX
		*m_ioService,
#endif
		m_fileHandle.handle(),
		offset,
		buffer,
		byteCount
	};
}

cppcoro::file_write_operation_cancellable cppcoro::writable_file::write(
	std::uint64_t offset,
	const void* buffer,
	std::size_t byteCount,
	cancellation_token ct) noexcept
{
	return file_write_operation_cancellable{
#if CPPCORO_OS_LINUX
        *m_ioService,
#endif
		m_fileHandle.handle(),
		offset,
		buffer,
		byteCount,
		std::move(ct)
	};
}
