///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_NET_SOCKET_ACCEPT_OPERATION_HPP_INCLUDED
#define CPPCORO_NET_SOCKET_ACCEPT_OPERATION_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/cancellation_registration.hpp>
#include <cppcoro/cancellation_token.hpp>

#if CPPCORO_OS_WINNT
#include <cppcoro/detail/win32.hpp>
#include <cppcoro/detail/win32_overlapped_operation.hpp>
#elif CPPCORO_OS_LINUX
#include <cppcoro/detail/linux_uring_operation.hpp>
#endif

#include <cppcoro/detail/stdcoro.hpp>
#include <atomic>
#include <optional>

namespace cppcoro
{
	namespace net
	{
		class socket;

		class socket_accept_operation_impl
		{
		public:
			socket_accept_operation_impl(socket& listeningSocket, socket& acceptingSocket) noexcept
				: m_listeningSocket(listeningSocket)
				, m_acceptingSocket(acceptingSocket)
			{
#if CPPCORO_OS_LINUX
                detail::lnx::check_required_kernel(5, 5, "socket accept operation");
#endif
			}

			bool try_start(cppcoro::detail::io_operation_base& operation) noexcept;
			void cancel(cppcoro::detail::io_operation_base& operation) noexcept;
			void get_result(cppcoro::detail::io_operation_base& operation);

		private:
#if CPPCORO_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4324)  // Structure padded due to alignment
#endif

			socket& m_listeningSocket;
			socket& m_acceptingSocket;
			alignas(8) std::uint8_t m_addressBuffer[88];

#if CPPCORO_OS_LINUX
			socklen_t m_addressBufferLength = sizeof(m_addressBuffer);
#endif

#if CPPCORO_COMPILER_MSVC
#pragma warning(pop)
#endif
		};

		class socket_accept_operation
			: public cppcoro::detail::io_operation<socket_accept_operation>
		{
		public:
			socket_accept_operation(
#if CPPCORO_OS_LINUX
				io_service& ioService,
#endif
				socket& listeningSocket,
				socket& acceptingSocket) noexcept
				: cppcoro::detail::io_operation<socket_accept_operation>(
#if CPPCORO_OS_LINUX
				    ioService
#endif
                    )
				, m_impl(listeningSocket, acceptingSocket)
			{
			}

		private:
			friend cppcoro::detail::io_operation<socket_accept_operation>;

			bool try_start() noexcept { return m_impl.try_start(*this); }
			void get_result() { m_impl.get_result(*this); }

			socket_accept_operation_impl m_impl;
		};

		class socket_accept_operation_cancellable
			: public cppcoro::detail::io_operation_cancellable<socket_accept_operation_cancellable>
		{
		public:
			socket_accept_operation_cancellable(
#if CPPCORO_OS_LINUX
				io_service& ioService,
#endif
				socket& listeningSocket,
				socket& acceptingSocket,
				cancellation_token&& ct) noexcept
				: cppcoro::detail::io_operation_cancellable<socket_accept_operation_cancellable>(
#if CPPCORO_OS_LINUX
                    ioService,
#endif
					  std::move(ct))
				, m_impl(listeningSocket, acceptingSocket)
			{
			}

		private:
			friend cppcoro::detail::io_operation_cancellable<
				socket_accept_operation_cancellable>;

			bool try_start() noexcept { return m_impl.try_start(*this); }
			void cancel() noexcept { m_impl.cancel(*this); }
			void get_result() { m_impl.get_result(*this); }

			socket_accept_operation_impl m_impl;
		};
	}  // namespace net
}  // namespace cppcoro

#endif
