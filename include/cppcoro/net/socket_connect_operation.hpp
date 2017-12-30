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

		class socket_connect_operation_impl
		{
		public:

			socket_connect_operation_impl(
				socket& socket,
				const ip_endpoint& remoteEndPoint) noexcept
				: m_socket(socket)
				, m_remoteEndPoint(remoteEndPoint)
			{}

			bool try_start(cppcoro::detail::win32_overlapped_operation_base& operation) noexcept;
			void cancel(cppcoro::detail::win32_overlapped_operation_base& operation) noexcept;
			void get_result(cppcoro::detail::win32_overlapped_operation_base& operation);

		private:

			socket& m_socket;
			ip_endpoint m_remoteEndPoint;

		};

		class socket_connect_operation
			: public cppcoro::detail::win32_overlapped_operation<socket_connect_operation>
		{
		public:

			socket_connect_operation(
				socket& socket,
				const ip_endpoint& remoteEndPoint) noexcept
				: m_impl(socket, remoteEndPoint)
			{}

		private:

			friend class cppcoro::detail::win32_overlapped_operation<socket_connect_operation>;

			bool try_start() noexcept { return m_impl.try_start(*this); }
			decltype(auto) get_result() { return m_impl.get_result(*this); }

			socket_connect_operation_impl m_impl;

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
				, m_impl(socket, remoteEndPoint)
			{}

		private:

			friend class cppcoro::detail::win32_overlapped_operation_cancellable<socket_connect_operation_cancellable>;

			bool try_start() noexcept { return m_impl.try_start(*this); }
			void cancel() noexcept { m_impl.cancel(*this); }
			void get_result() { m_impl.get_result(*this); }

			socket_connect_operation_impl m_impl;

		};
	}
}

#endif // CPPCORO_OS_WINNT

#endif
