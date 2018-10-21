///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/net/socket_accept_operation.hpp>
#include <cppcoro/net/socket.hpp>

#include "socket_helpers.hpp"

#include <system_error>

#if CPPCORO_OS_WINNT
# include <WinSock2.h>
# include <WS2tcpip.h>
# include <MSWSock.h>
# include <Windows.h>

// TODO: Eliminate duplication of implementation between socket_accept_operation
// and socket_accept_operation_cancellable.

bool cppcoro::net::socket_accept_operation_impl::try_start(
	cppcoro::detail::win32_overlapped_operation_base& operation) noexcept
{
	static_assert(
		(sizeof(m_addressBuffer) / 2) >= (16 + sizeof(SOCKADDR_IN)) &&
		(sizeof(m_addressBuffer) / 2) >= (16 + sizeof(SOCKADDR_IN6)),
		"AcceptEx requires address buffer to be at least 16 bytes more than largest address.");

	// Need to read this flag before starting the operation, otherwise
	// it may be possible that the operation will complete immediately
	// on another thread and then destroy the socket before we get a
	// chance to read it.
	const bool skipCompletionOnSuccess = m_listeningSocket.skip_completion_on_success();

	DWORD bytesReceived = 0;
	BOOL ok = ::AcceptEx(
		m_listeningSocket.native_handle(),
		m_acceptingSocket.native_handle(),
		m_addressBuffer,
		0,
		sizeof(m_addressBuffer) / 2,
		sizeof(m_addressBuffer) / 2,
		&bytesReceived,
		operation.get_overlapped());
	if (!ok)
	{
		int errorCode = ::WSAGetLastError();
		if (errorCode != ERROR_IO_PENDING)
		{
			operation.m_errorCode = static_cast<DWORD>(errorCode);
			return false;
		}
	}
	else if (skipCompletionOnSuccess)
	{
		operation.m_errorCode = ERROR_SUCCESS;
		return false;
	}

	return true;
}

void cppcoro::net::socket_accept_operation_impl::cancel(
	cppcoro::detail::win32_overlapped_operation_base& operation) noexcept
{
	(void)::CancelIoEx(
		reinterpret_cast<HANDLE>(m_listeningSocket.native_handle()),
		operation.get_overlapped());
}

void cppcoro::net::socket_accept_operation_impl::get_result(
	cppcoro::detail::win32_overlapped_operation_base& operation)
{
	if (operation.m_errorCode != ERROR_SUCCESS)
	{
		throw std::system_error{
			static_cast<int>(operation.m_errorCode),
			std::system_category(),
			"Accepting a connection failed: AcceptEx"
		};
	}

	sockaddr* localSockaddr = nullptr;
	sockaddr* remoteSockaddr = nullptr;

	INT localSockaddrLength;
	INT remoteSockaddrLength;

	::GetAcceptExSockaddrs(
		m_addressBuffer,
		0,
		sizeof(m_addressBuffer) / 2,
		sizeof(m_addressBuffer) / 2,
		&localSockaddr,
		&localSockaddrLength,
		&remoteSockaddr,
		&remoteSockaddrLength);

	m_acceptingSocket.m_localEndPoint =
		detail::sockaddr_to_ip_endpoint(*localSockaddr);

	m_acceptingSocket.m_remoteEndPoint =
		detail::sockaddr_to_ip_endpoint(*remoteSockaddr);

	{
		// Need to set SO_UPDATE_ACCEPT_CONTEXT after the accept completes
		// to ensure that ::shutdown() and ::setsockopt() calls work on the
		// accepted socket.
		SOCKET listenSocket = m_listeningSocket.native_handle();
		const int result = ::setsockopt(
			m_acceptingSocket.native_handle(),
			SOL_SOCKET,
			SO_UPDATE_ACCEPT_CONTEXT,
			(const char*)&listenSocket,
			sizeof(SOCKET));
		if (result == SOCKET_ERROR)
		{
			const int errorCode = ::WSAGetLastError();
			throw std::system_error{
				errorCode,
				std::system_category(),
				"Socket accept operation failed: setsockopt(SO_UPDATE_ACCEPT_CONTEXT)"
			};
		}
	}
}

#endif
