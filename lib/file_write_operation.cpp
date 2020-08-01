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

#elif defined(CPPCORO_OS_LINUX)

bool cppcoro::file_write_operation_impl::try_start(
    cppcoro::detail::uring_operation_base& operation) noexcept
{
    const size_t numberOfBytesToWrite =
        m_byteCount <= std::numeric_limits<size_t>::max() ?
        m_byteCount : std::numeric_limits<size_t>::max();
    m_message.m_ptr = operation.m_awaitingCoroutine.address();
    auto sqe = io_uring_get_sqe(m_ioService.native_uring_handle());
    m_vec.iov_base = const_cast<void*>(m_buffer);
	m_vec.iov_len = numberOfBytesToWrite;
    io_uring_prep_writev(sqe, m_fileHandle, &m_vec, 1, 0);
    io_uring_sqe_set_data(sqe, &m_message);
    io_uring_submit(m_ioService.native_uring_handle());
    return true;
}

void cppcoro::file_write_operation_impl::cancel(
    cppcoro::detail::uring_operation_base& operation) noexcept
{
    throw std::runtime_error("Not implemented");
    //(void)::CancelIoEx(m_fileHandle, operation.get_overlapped());
}

#endif // CPPCORO_OS_WINNT
