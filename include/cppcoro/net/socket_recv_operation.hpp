///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_NET_SOCKET_RECV_OPERATION_HPP_INCLUDED
#define CPPCORO_NET_SOCKET_RECV_OPERATION_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/cancellation_token.hpp>

#include <cstdint>

#if CPPCORO_OS_WINNT
# include <cppcoro/detail/win32.hpp>
# include <cppcoro/detail/win32_overlapped_operation.hpp>

namespace cppcoro::net
{
	class socket;

	class socket_recv_operation
		: public cppcoro::detail::win32_overlapped_operation<socket_recv_operation>
	{
	public:

		socket_recv_operation(
			socket& s,
			void* buffer,
			std::size_t byteCount) noexcept;

	private:

		friend class cppcoro::detail::win32_overlapped_operation<socket_recv_operation>;

		bool try_start() noexcept;

		cppcoro::detail::win32::socket_t m_socketHandle;
		cppcoro::detail::win32::wsabuf m_buffer;
		bool m_skipCompletionOnSuccess;

	};

	class socket_recv_operation_cancellable
		: public cppcoro::detail::win32_overlapped_operation_cancellable<socket_recv_operation_cancellable>
	{
	public:

		socket_recv_operation_cancellable(
			socket& s,
			void* buffer,
			std::size_t byteCount,
			cancellation_token&& ct) noexcept;

	private:

		friend class cppcoro::detail::win32_overlapped_operation<socket_recv_operation>;

		bool try_start() noexcept;
		void cancel() noexcept;

		cppcoro::detail::win32::socket_t m_socketHandle;
		cppcoro::detail::win32::wsabuf m_buffer;
		bool m_skipCompletionOnSuccess;

	};

}

#endif // CPPCORO_OS_WINNT

#endif
