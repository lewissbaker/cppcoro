///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_NET_SOCKET_CONNECT_OPERATION_HPP_INCLUDED
#define CPPCORO_NET_SOCKET_CONNECT_OPERATION_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/cancellation_token.hpp>
#include <cppcoro/net/ip_endpoint.hpp>

#if CPPCORO_OS_WINNT
# include <cppcoro/detail/win32.hpp>
# include <cppcoro/detail/win32_overlapped_operation.hpp>

namespace cppcoro
{
	namespace net
	{
		class socket;

		class socket_connect_operation
			: public cppcoro::detail::win32_overlapped_operation<socket_connect_operation>
		{
		public:

			socket_connect_operation(
				socket& socket,
				const ip_endpoint& remoteEndPoint) noexcept
				: cppcoro::detail::win32_overlapped_operation<socket_connect_operation>()
				, m_socket(socket)
				, m_remoteEndPoint(remoteEndPoint)
			{}

		private:

			friend class cppcoro::detail::win32_overlapped_operation<socket_connect_operation>;

			bool try_start() noexcept;
			void get_result();

			socket& m_socket;
			ip_endpoint m_remoteEndPoint;

		};

		class socket_connect_operation_cancellable
			: public cppcoro::detail::win32_overlapped_operation_cancellable<socket_connect_operation_cancellable>
		{
		public:

			socket_connect_operation_cancellable(
				socket& socket,
				const ip_endpoint& remoteEndPoint,
				cancellation_token&& ct) noexcept
				: cppcoro::detail::win32_overlapped_operation_cancellable<socket_connect_operation_cancellable>(std::move(ct))
				, m_socket(socket)
				, m_remoteEndPoint(remoteEndPoint)
			{}

		private:

			friend class cppcoro::detail::win32_overlapped_operation<socket_connect_operation>;

			bool try_start() noexcept;
			void cancel() noexcept;
			void get_result();

			socket& m_socket;
			ip_endpoint m_remoteEndPoint;

		};
	}
}

#endif // CPPCORO_OS_WINNT

#endif
