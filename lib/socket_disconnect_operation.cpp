///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/net/socket_disconnect_operation.hpp>
#include <cppcoro/net/socket.hpp>

#include "socket_helpers.hpp"

#include <system_error>

#if CPPCORO_OS_WINNT
# include <WinSock2.h>
# include <WS2tcpip.h>
# include <MSWSock.h>
# include <Windows.h>

bool cppcoro::net::socket_disconnect_operation::try_start() noexcept
{
	// Lookup the address of the DisconnectEx function pointer for this socket.
	LPFN_DISCONNECTEX disconnectExPtr;
	{
		GUID disconnectExGuid = WSAID_DISCONNECTEX;
		DWORD byteCount = 0;
		const int result = ::WSAIoctl(
			m_socket.native_handle(),
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			static_cast<void*>(&disconnectExGuid),
			sizeof(disconnectExGuid),
			static_cast<void*>(&disconnectExPtr),
			sizeof(disconnectExPtr),
			&byteCount,
			nullptr,
			nullptr);
		if (result == SOCKET_ERROR)
		{
			m_errorCode = static_cast<DWORD>(::WSAGetLastError());
			return false;
		}
	}

	// Need to add TF_REUSE_SOCKET to these flags if we want to allow reusing
	// a socket for subsequent connections once the disconnect operation
	// completes.
	const DWORD flags = 0;

	const BOOL ok = disconnectExPtr(
		m_socket.native_handle(),
		get_overlapped(),
		flags,
		0);
	if (!ok)
	{
		const int errorCode = ::WSAGetLastError();
		if (errorCode != ERROR_IO_PENDING)
		{
			// Failed synchronously.
			m_errorCode = static_cast<DWORD>(errorCode);
			return false;
		}
	}
	else if (m_socket.m_skipCompletionOnSuccess)
	{
		// Successfully completed synchronously and no completion event
		// will be posted to an I/O thread so we can return without suspending.
		m_errorCode = ERROR_SUCCESS;
		return false;
	}

	return true;
}

void cppcoro::net::socket_disconnect_operation::get_result()
{
	if (m_errorCode != ERROR_SUCCESS)
	{
		throw std::system_error(
			static_cast<int>(m_errorCode),
			std::system_category(),
			"Disconnect operation failed: DisconnectEx");
	}
}

bool cppcoro::net::socket_disconnect_operation_cancellable::try_start() noexcept
{
	// Lookup the address of the DisconnectEx function pointer for this socket.
	LPFN_DISCONNECTEX disconnectExPtr;
	{
		GUID disconnectExGuid = WSAID_DISCONNECTEX;
		DWORD byteCount = 0;
		const int result = ::WSAIoctl(
			m_socket.native_handle(),
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			static_cast<void*>(&disconnectExGuid),
			sizeof(disconnectExGuid),
			static_cast<void*>(&disconnectExPtr),
			sizeof(disconnectExPtr),
			&byteCount,
			nullptr,
			nullptr);
		if (result == SOCKET_ERROR)
		{
			m_errorCode = static_cast<DWORD>(::WSAGetLastError());
			return false;
		}
	}

	// Need to add TF_REUSE_SOCKET to these flags if we want to allow reusing
	// a socket for subsequent connections once the disconnect operation
	// completes.
	const DWORD flags = 0;

	const BOOL ok = disconnectExPtr(
		m_socket.native_handle(),
		get_overlapped(),
		flags,
		0);
	if (!ok)
	{
		const int errorCode = ::WSAGetLastError();
		if (errorCode != ERROR_IO_PENDING)
		{
			// Failed synchronously.
			m_errorCode = static_cast<DWORD>(errorCode);
			return false;
		}
	}
	else if (m_socket.m_skipCompletionOnSuccess)
	{
		// Successfully completed synchronously and no completion event
		// will be posted to an I/O thread so we can return without suspending.
		m_errorCode = ERROR_SUCCESS;
		return false;
	}

	return true;
}

void cppcoro::net::socket_disconnect_operation_cancellable::cancel() noexcept
{
	(void)::CancelIoEx(
		reinterpret_cast<HANDLE>(m_socket.native_handle()),
		get_overlapped());
}

void cppcoro::net::socket_disconnect_operation_cancellable::get_result()
{
	if (m_errorCode != ERROR_SUCCESS)
	{
		throw std::system_error(
			static_cast<int>(m_errorCode),
			std::system_category(),
			"Disconnect operation failed: DisconnectEx");
	}
}

#endif
