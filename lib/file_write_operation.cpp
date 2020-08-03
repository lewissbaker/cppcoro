///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/file_write_operation.hpp>

#if CPPCORO_OS_WINNT
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

bool cppcoro::file_write_operation_impl::try_start(
	cppcoro::detail::io_operation_base& operation) noexcept
{
	const DWORD numberOfBytesToWrite =
		m_byteCount <= 0xFFFFFFFF ? static_cast<DWORD>(m_byteCount) : DWORD(0xFFFFFFFF);

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
	cppcoro::detail::io_operation_base& operation) noexcept
{
	(void)::CancelIoEx(m_fileHandle, operation.get_overlapped());
}

#elif CPPCORO_OS_LINUX

bool cppcoro::file_write_operation_impl::try_start(
	cppcoro::detail::uring_operation_base& operation) noexcept
{
	const size_t numberOfBytesToWrite = m_byteCount <= std::numeric_limits<size_t>::max()
		? m_byteCount
		: std::numeric_limits<size_t>::max();
	return operation.try_start_write(m_fileHandle, m_buffer, numberOfBytesToWrite);
}

void cppcoro::file_write_operation_impl::cancel(
	cppcoro::detail::uring_operation_base& operation) noexcept
{
    if (operation.m_awaitingCoroutine.address() != nullptr) {
        operation.cancel_io();
    }
}

#endif  // CPPCORO_OS_WINNT
