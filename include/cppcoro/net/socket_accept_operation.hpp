///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_NET_SOCKET_ACCEPT_OPERATION_HPP_INCLUDED
#define CPPCORO_NET_SOCKET_ACCEPT_OPERATION_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/cancellation_token.hpp>
#include <cppcoro/cancellation_registration.hpp>

#if CPPCORO_OS_WINNT
# include <cppcoro/detail/win32.hpp>
# include <cppcoro/detail/win32_overlapped_operation.hpp>

# include <atomic>
# include <optional>
# include <experimental/coroutine>

namespace cppcoro
{
	namespace net
	{
		class socket;

		class socket_accept_operation
			: public cppcoro::detail::win32_overlapped_operation<socket_accept_operation>
		{
		public:

			socket_accept_operation(
				socket& listeningSocket,
				socket& acceptingSocket) noexcept
				: cppcoro::detail::win32_overlapped_operation<socket_accept_operation>()
				, m_listeningSocket(listeningSocket)
				, m_acceptingSocket(acceptingSocket)
			{}

		private:

			friend class cppcoro::detail::win32_overlapped_operation<socket_accept_operation>;

			bool try_start() noexcept;
			void get_result();

#if CPPCORO_COMPILER_MSVC
# pragma warning(push)
# pragma warning(disable : 4324) // Structure padded due to alignment
#endif

			socket& m_listeningSocket;
			socket& m_acceptingSocket;
			alignas(8) std::uint8_t m_addressBuffer[88];

#if CPPCORO_COMPILER_MSVC
# pragma warning(pop)
#endif
		};

		class socket_accept_operation_cancellable
			: public cppcoro::detail::win32_overlapped_operation_cancellable<socket_accept_operation_cancellable>
		{
		public:

			socket_accept_operation_cancellable(
				socket& listeningSocket,
				socket& acceptingSocket,
				cancellation_token&& ct) noexcept
				: cppcoro::detail::win32_overlapped_operation_cancellable<socket_accept_operation_cancellable>(std::move(ct))
				, m_listeningSocket(listeningSocket)
				, m_acceptingSocket(acceptingSocket)
			{}

		private:

			friend class cppcoro::detail::win32_overlapped_operation<socket_accept_operation>;

			bool try_start() noexcept;
			void cancel() noexcept;
			void get_result();

#if CPPCORO_COMPILER_MSVC
# pragma warning(push)
# pragma warning(disable : 4324) // Structure padded due to alignment
#endif

			socket& m_listeningSocket;
			socket& m_acceptingSocket;
			alignas(8) std::uint8_t m_addressBuffer[88];

#if CPPCORO_COMPILER_MSVC
# pragma warning(pop)
#endif

		};
	}
}

#endif // CPPCORO_OS_WINNT

#endif
