///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/net/socket.hpp>
#include <cppcoro/net/socket_send_to_operation.hpp>

#include "socket_helpers.hpp"

#if CPPCORO_OS_WINNT
#include <WinSock2.h>
#include <WS2tcpip.h>

bool cppcoro::net::socket_send_to_operation_impl::try_start(
	cppcoro::detail::io_operation_base& operation) noexcept
{
	// Need to read this flag before starting the operation, otherwise
	// it may be possible that the operation will complete immediately
	// on another thread and then destroy the socket before we get a
	// chance to read it.
	const bool skipCompletionOnSuccess = m_socket.skip_completion_on_success();

	SOCKADDR_STORAGE destinationAddress;
	const int destinationLength =
		detail::ip_endpoint_to_sockaddr(m_destination, std::ref(destinationAddress));

	DWORD numberOfBytesSent = 0;
	int result = ::WSASendTo(
		m_socket.native_handle(),
		reinterpret_cast<WSABUF*>(&m_buffer),
		1,  // buffer count
		&numberOfBytesSent,
		0,  // flags
		reinterpret_cast<const SOCKADDR*>(&destinationAddress),
		destinationLength,
		operation.get_overlapped(),
		nullptr);
	if (result == SOCKET_ERROR)
	{
		int errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			// Failed synchronously.
			operation.m_errorCode = static_cast<DWORD>(errorCode);
			operation.m_numberOfBytesTransferred = numberOfBytesSent;
			return false;
		}
	}
	else if (skipCompletionOnSuccess)
	{
		// Completed synchronously, no completion event will be posted to the IOCP.
		operation.m_errorCode = ERROR_SUCCESS;
		operation.m_numberOfBytesTransferred = numberOfBytesSent;
		return false;
	}

	// Operation will complete asynchronously.
	return true;
}

void cppcoro::net::socket_send_to_operation_impl::cancel(
	cppcoro::detail::io_operation_base& operation) noexcept
{
	(void)::CancelIoEx(
		reinterpret_cast<HANDLE>(m_socket.native_handle()), operation.get_overlapped());
}
#elif CPPCORO_OS_LINUX
bool cppcoro::net::socket_send_to_operation_impl::try_start(
	cppcoro::detail::io_operation_base& operation) noexcept
{
	const int destinationLength =
		detail::ip_endpoint_to_sockaddr(m_destination, std::ref(m_destinationStorage));
	return operation.try_start_sendto(
		m_socket.native_handle(),
		&m_destinationStorage,
		destinationLength,
		m_buffer.buffer,
		m_buffer.size);
}

void cppcoro::net::socket_send_to_operation_impl::cancel(
	cppcoro::detail::io_operation_base& operation) noexcept
{
    operation.cancel_io();
}

#endif
