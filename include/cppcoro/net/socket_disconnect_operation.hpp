///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_NET_SOCKET_DISCONNECT_OPERATION_HPP_INCLUDED
#define CPPCORO_NET_SOCKET_DISCONNECT_OPERATION_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/cancellation_token.hpp>

#if CPPCORO_OS_WINNT
#include <cppcoro/detail/win32.hpp>
#include <cppcoro/detail/win32_overlapped_operation.hpp>
#elif CPPCORO_OS_LINUX
#include <cppcoro/detail/linux_uring_operation.hpp>
#endif

namespace cppcoro
{
	namespace net
	{
		class socket;

		class socket_disconnect_operation_impl
		{
		public:
			socket_disconnect_operation_impl(socket& socket) noexcept
				: m_socket(socket)
			{
			}

			bool try_start(cppcoro::detail::io_operation_base& operation) noexcept;
			void cancel(cppcoro::detail::io_operation_base& operation) noexcept;
			void get_result(cppcoro::detail::io_operation_base& operation);

		private:
			socket& m_socket;
		};

		class socket_disconnect_operation
			: public cppcoro::detail::io_operation<socket_disconnect_operation>
		{
		public:
			socket_disconnect_operation(
#if CPPCORO_OS_LINUX
				io_service& ioService,
#endif
				socket& socket) noexcept
				: cppcoro::detail::io_operation<socket_disconnect_operation>(
#if CPPCORO_OS_LINUX
					  ioService
#endif
					  )
				, m_impl(socket)
			{
			}

		private:
			friend cppcoro::detail::io_operation<socket_disconnect_operation>;

			bool try_start() noexcept { return m_impl.try_start(*this); }
			void get_result() { m_impl.get_result(*this); }

			socket_disconnect_operation_impl m_impl;
		};

		class socket_disconnect_operation_cancellable
			: public cppcoro::detail::io_operation_cancellable<
				  socket_disconnect_operation_cancellable>
		{
		public:
			socket_disconnect_operation_cancellable(
#if CPPCORO_OS_LINUX
				io_service& ioService,
#endif
				socket& socket,
				cancellation_token&& ct) noexcept
				: cppcoro::detail::io_operation_cancellable<
					  socket_disconnect_operation_cancellable>(
#if CPPCORO_OS_LINUX
					  ioService,
#endif
					  std::move(ct))
				, m_impl(socket)
			{
			}

		private:
			friend cppcoro::detail::io_operation_cancellable<
				socket_disconnect_operation_cancellable>;

			bool try_start() noexcept { return m_impl.try_start(*this); }
			void cancel() noexcept { m_impl.cancel(*this); }
			void get_result() { m_impl.get_result(*this); }

			socket_disconnect_operation_impl m_impl;
		};
	}  // namespace net
}  // namespace cppcoro

#endif
