///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/net/socket_recv_operation.hpp>
#include <cppcoro/net/socket.hpp>

#if CPPCORO_OS_WINNT
# include <WinSock2.h>
# include <WS2tcpip.h>
# include <MSWSock.h>
# include <Windows.h>

cppcoro::net::socket_recv_operation::socket_recv_operation(
	socket& s,
	void* buffer,
	std::size_t byteCount) noexcept
	: m_socketHandle(s.native_handle())
	, m_skipCompletionOnSuccess(s.skip_completion_on_success())
	, m_buffer(buffer, byteCount)
{
}

bool cppcoro::net::socket_recv_operation::try_start() noexcept
{
	DWORD numberOfBytesReceived = 0;
	DWORD flags = 0;
	int result = ::WSARecv(
		m_socketHandle,
		reinterpret_cast<WSABUF*>(&m_buffer),
		1, // buffer count
		&numberOfBytesReceived,
		&flags,
		get_overlapped(),
		nullptr);
	if (result == SOCKET_ERROR)
	{
		int errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			// Failed synchronously.
			m_errorCode = static_cast<DWORD>(errorCode);
			m_numberOfBytesTransferred = numberOfBytesReceived;
			return false;
		}
	}
	else if (m_skipCompletionOnSuccess)
	{
		// Completed synchronously, no completion event will be posted to the IOCP.
		m_errorCode = ERROR_SUCCESS;
		m_numberOfBytesTransferred = numberOfBytesReceived;
		return false;
	}

	// Operation will complete asynchronously.
	return true;
}

cppcoro::net::socket_recv_operation_cancellable::socket_recv_operation_cancellable(
	socket& s,
	void* buffer,
	std::size_t byteCount,
	cancellation_token&& ct) noexcept
	: cppcoro::detail::win32_overlapped_operation_cancellable<cppcoro::net::socket_recv_operation_cancellable>(
		std::move(ct))
	, m_socketHandle(s.native_handle())
	, m_skipCompletionOnSuccess(s.skip_completion_on_success())
	, m_buffer(buffer, byteCount)
{
}

bool cppcoro::net::socket_recv_operation_cancellable::try_start() noexcept
{
	DWORD numberOfBytesReceived = 0;
	DWORD flags = 0;
	int result = ::WSARecv(
		m_socketHandle,
		reinterpret_cast<WSABUF*>(&m_buffer),
		1, // buffer count
		&numberOfBytesReceived,
		&flags,
		get_overlapped(),
		nullptr);
	if (result == SOCKET_ERROR)
	{
		int errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			// Failed synchronously.
			m_errorCode = static_cast<DWORD>(errorCode);
			m_numberOfBytesTransferred = numberOfBytesReceived;
			return false;
		}
	}
	else if (m_skipCompletionOnSuccess)
	{
		// Completed synchronously, no completion event will be posted to the IOCP.
		m_errorCode = ERROR_SUCCESS;
		m_numberOfBytesTransferred = numberOfBytesReceived;
		return false;
	}

	// Operation will complete asynchronously.
	return true;
}

void cppcoro::net::socket_recv_operation_cancellable::cancel() noexcept
{
	(void)::CancelIoEx(
		reinterpret_cast<HANDLE>(m_socketHandle),
		get_overlapped());
}

#endif
