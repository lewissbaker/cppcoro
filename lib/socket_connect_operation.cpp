///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/net/socket_connect_operation.hpp>
#include <cppcoro/net/socket.hpp>

#include <cppcoro/operation_cancelled.hpp>

#include "socket_helpers.hpp"

#include <cassert>
#include <system_error>

#if CPPCORO_OS_WINNT
# include <WinSock2.h>
# include <WS2tcpip.h>
# include <MSWSock.h>
# include <Windows.h>

bool cppcoro::net::socket_connect_operation_impl::try_start(
	cppcoro::detail::win32_overlapped_operation_base& operation) noexcept
{
	// Lookup the address of the ConnectEx function pointer for this socket.
	LPFN_CONNECTEX connectExPtr;
	{
		GUID connectExGuid = WSAID_CONNECTEX;
		DWORD byteCount = 0;
		int result = ::WSAIoctl(
			m_socket.native_handle(),
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			static_cast<void*>(&connectExGuid),
			sizeof(connectExGuid),
			static_cast<void*>(&connectExPtr),
			sizeof(connectExPtr),
			&byteCount,
			nullptr,
			nullptr);
		if (result == SOCKET_ERROR)
		{
			operation.m_errorCode = ::WSAGetLastError();
			return false;
		}
	}

	// Need to read this flag before starting the operation, otherwise
	// it may be possible that the operation will complete immediately
	// on another thread and then destroy the socket before we get a
	// chance to read it.
	const bool skipCompletionOnSuccess = m_socket.skip_completion_on_success();

	SOCKADDR_STORAGE remoteSockaddrStorage;
	const int sockaddrNameLength = cppcoro::net::detail::ip_endpoint_to_sockaddr(
		m_remoteEndPoint,
		std::ref(remoteSockaddrStorage));

	DWORD bytesSent = 0;
	const BOOL ok = connectExPtr(
		m_socket.native_handle(),
		reinterpret_cast<const SOCKADDR*>(&remoteSockaddrStorage),
		sockaddrNameLength,
		nullptr, // send buffer
		0, // size of send buffer
		&bytesSent,
		operation.get_overlapped());
	if (!ok)
	{
		const int errorCode = ::WSAGetLastError();
		if (errorCode != ERROR_IO_PENDING)
		{
			// Failed synchronously.
			operation.m_errorCode = static_cast<DWORD>(errorCode);
			return false;
		}
	}
	else if (skipCompletionOnSuccess)
	{
		// Successfully completed synchronously and no completion event
		// will be posted to an I/O thread so we can return without suspending.
		operation.m_errorCode = ERROR_SUCCESS;
		return false;
	}

	return true;
}

void cppcoro::net::socket_connect_operation_impl::cancel(
	cppcoro::detail::win32_overlapped_operation_base& operation) noexcept
{
	(void)::CancelIoEx(
		reinterpret_cast<HANDLE>(m_socket.native_handle()),
		operation.get_overlapped());
}

void cppcoro::net::socket_connect_operation_impl::get_result(
	cppcoro::detail::win32_overlapped_operation_base& operation)
{
	if (operation.m_errorCode != ERROR_SUCCESS)
	{
		if (operation.m_errorCode == ERROR_OPERATION_ABORTED)
		{
			throw operation_cancelled{};
		}

		throw std::system_error{
			static_cast<int>(operation.m_errorCode),
			std::system_category(),
			"Connect operation failed: ConnectEx"
		};
	}

	// We need to call setsockopt() to update the socket state with information
	// about the connection now that it has been successfully connected.
	{
		const int result = ::setsockopt(
			m_socket.native_handle(),
			SOL_SOCKET,
			SO_UPDATE_CONNECT_CONTEXT,
			nullptr,
			0);
		if (result == SOCKET_ERROR)
		{
			// This shouldn't fail, but just in case it does we fall back to
			// setting the remote address as specified in the call to Connect().
			//
			// Don't really want to throw an exception here since the connection
			// has actually been established.
			m_socket.m_remoteEndPoint = m_remoteEndPoint;
			return;
		}
	}

	{
		SOCKADDR_STORAGE localSockaddr;
		int nameLength = sizeof(localSockaddr);
		const int result = ::getsockname(
			m_socket.native_handle(),
			reinterpret_cast<SOCKADDR*>(&localSockaddr),
			&nameLength);
		if (result == 0)
		{
			m_socket.m_localEndPoint = cppcoro::net::detail::sockaddr_to_ip_endpoint(
				*reinterpret_cast<const SOCKADDR*>(&localSockaddr));
		}
		else
		{
			// Failed to get the updated local-end-point
			// Just leave m_localEndPoint set to whatever bind() left it as.
			//
			// TODO: Should we be throwing an exception here instead?
		}
	}

	{
		SOCKADDR_STORAGE remoteSockaddr;
		int nameLength = sizeof(remoteSockaddr);
		const int result = ::getpeername(
			m_socket.native_handle(),
			reinterpret_cast<SOCKADDR*>(&remoteSockaddr),
			&nameLength);
		if (result == 0)
		{
			m_socket.m_remoteEndPoint = cppcoro::net::detail::sockaddr_to_ip_endpoint(
				*reinterpret_cast<const SOCKADDR*>(&remoteSockaddr));
		}
		else
		{
			// Failed to get the actual remote end-point so just fall back to
			// remembering the actual end-point that was passed to connect().
			//
			// TODO: Should we be throwing an exception here instead?
			m_socket.m_remoteEndPoint = m_remoteEndPoint;
		}
	}
}

#endif
