///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_NET_SOCKET_SEND_OPERATION_HPP_INCLUDED
#define CPPCORO_NET_SOCKET_SEND_OPERATION_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/cancellation_token.hpp>

#include <cstdint>

#if CPPCORO_OS_WINNT
# include <cppcoro/detail/win32.hpp>
# include <cppcoro/detail/win32_overlapped_operation.hpp>

namespace cppcoro::net
{
	class socket;

	class socket_send_operation
		: public cppcoro::detail::win32_overlapped_operation<socket_send_operation>
	{
	public:

		socket_send_operation(
			socket& s,
			const void* buffer,
			std::size_t byteCount) noexcept;

	private:

		friend class cppcoro::detail::win32_overlapped_operation<socket_send_operation>;

		bool try_start() noexcept;

		cppcoro::detail::win32::socket_t m_socketHandle;
		cppcoro::detail::win32::wsabuf m_buffer;
		bool m_skipCompletionOnSuccess;

	};

	class socket_send_operation_cancellable
		: public cppcoro::detail::win32_overlapped_operation_cancellable<socket_send_operation_cancellable>
	{
	public:

		socket_send_operation_cancellable(
			socket& s,
			const void* buffer,
			std::size_t byteCount,
			cancellation_token&& ct) noexcept;

	private:

		friend class cppcoro::detail::win32_overlapped_operation<socket_send_operation>;

		bool try_start() noexcept;
		void cancel() noexcept;

		cppcoro::detail::win32::socket_t m_socketHandle;
		cppcoro::detail::win32::wsabuf m_buffer;
		bool m_skipCompletionOnSuccess;

	};

}

#endif // CPPCORO_OS_WINNT

#endif
