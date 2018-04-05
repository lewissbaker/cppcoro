///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/file_read_operation.hpp>

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <Windows.h>

bool cppcoro::file_read_operation::try_start() noexcept
{
	const DWORD numberOfBytesToRead =
		m_byteCount <= 0xFFFFFFFF ?
		static_cast<DWORD>(m_byteCount) : DWORD(0xFFFFFFFF);

	BOOL ok = ::ReadFile(
		m_fileHandle,
		m_buffer,
		numberOfBytesToRead,
		nullptr,
		get_overlapped());
	const DWORD errorCode = ok ? ERROR_SUCCESS : ::GetLastError();
	if (errorCode != ERROR_IO_PENDING)
	{
		// Completed synchronously.
		//
		// We are assuming that the file-handle has been set to the
		// mode where synchronous completions do not post a completion
		// event to the I/O completion port and thus can return without
		// suspending here.

		m_errorCode = errorCode;

		ok = ::GetOverlappedResult(
			m_fileHandle,
			get_overlapped(),
			&m_numberOfBytesTransferred,
			FALSE);
		if (!ok)
		{
			m_numberOfBytesTransferred = 0;
		}

		return false;
	}

	return true;
}

bool cppcoro::file_read_operation_cancellable::try_start() noexcept
{
	const DWORD numberOfBytesToRead =
		m_byteCount <= 0xFFFFFFFF ?
		static_cast<DWORD>(m_byteCount) : DWORD(0xFFFFFFFF);

	BOOL ok = ::ReadFile(
		m_fileHandle,
		m_buffer,
		numberOfBytesToRead,
		nullptr,
		get_overlapped());
	const DWORD errorCode = ok ? ERROR_SUCCESS : ::GetLastError();
	if (errorCode != ERROR_IO_PENDING)
	{
		// Completed synchronously.
		//
		// We are assuming that the file-handle has been set to the
		// mode where synchronous completions do not post a completion
		// event to the I/O completion port and thus can return without
		// suspending here.

		this->m_errorCode = errorCode;

		ok = ::GetOverlappedResult(
			m_fileHandle,
			get_overlapped(),
			&m_numberOfBytesTransferred,
			FALSE);
		if (!ok)
		{
			m_numberOfBytesTransferred = 0;
		}

		return false;
	}

	return true;
}

void cppcoro::file_read_operation_cancellable::cancel() noexcept
{
	(void)::CancelIoEx(m_fileHandle, this->get_overlapped());
}

#endif // CPPCORO_OS_WINNT
