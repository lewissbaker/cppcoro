///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/file_write_operation.hpp>

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <Windows.h>

bool cppcoro::file_write_operation_impl::try_start(
	cppcoro::detail::win32_overlapped_operation_base& operation) noexcept
{
	const DWORD numberOfBytesToWrite =
		m_byteCount <= 0xFFFFFFFF ?
		static_cast<DWORD>(m_byteCount) : DWORD(0xFFFFFFFF);

	DWORD numberOfBytesWritten = 0;
	BOOL ok = ::WriteFile(
		m_fileHandle,
		m_buffer,
		numberOfBytesToWrite,
		&numberOfBytesWritten,
		operation.get_overlapped());
	const DWORD errorCode = ok ? ERROR_SUCCESS : ::GetLastError();
	if (errorCode != ERROR_IO_PENDING)
	{
		// Completed synchronously.
		//
		// We are assuming that the file-handle has been set to the
		// mode where synchronous completions do not post a completion
		// event to the I/O completion port and thus can return without
		// suspending here.

		operation.m_errorCode = errorCode;
		operation.m_numberOfBytesTransferred = numberOfBytesWritten;

		return false;
	}

	return true;
}

void cppcoro::file_write_operation_impl::cancel(
	cppcoro::detail::win32_overlapped_operation_base& operation) noexcept
{
	(void)::CancelIoEx(m_fileHandle, operation.get_overlapped());
}

#endif // CPPCORO_OS_WINNT

#if CPPCORO_OS_LINUX
#include "io_uring.hpp"

bool cppcoro::file_write_operation_impl::try_start(
	cppcoro::detail::linux_async_operation_base& operation) noexcept
{
	io_uring_sqe sqe{};
	sqe.opcode = IORING_OP_WRITE;
	sqe.fd = m_fd;
	sqe.off = m_offset;
	sqe.addr = reinterpret_cast<std::uint64_t>(m_buffer);
	sqe.len = m_byteCount;
	sqe.user_data = reinterpret_cast<std::uint64_t>(&operation);

	bool ok;

	try
	{
		ok = operation.m_aioContext->submit_one(sqe);
	}
	catch (std::system_error& ex)
	{
		operation.m_res = -ex.code().value();

		return false;
	}

	return ok;
}

void cppcoro::file_write_operation_impl::cancel(
	cppcoro::detail::linux_async_operation_base& operation) noexcept
{
	io_uring_sqe sqe{};
	sqe.opcode = IORING_OP_ASYNC_CANCEL;

	try
	{
		operation.m_aioContext->submit_one(sqe);
	}
	catch (...)
	{
	}
}

#endif // CPPCORO_OS_LINUX
