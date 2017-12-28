///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_NET_SOCKET_DISCONNECT_OPERATION_HPP_INCLUDED
#define CPPCORO_NET_SOCKET_DISCONNECT_OPERATION_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/cancellation_token.hpp>

#if CPPCORO_OS_WINNT
# include <cppcoro/detail/win32.hpp>
# include <cppcoro/detail/win32_overlapped_operation.hpp>

namespace cppcoro
{
	namespace net
	{
		class socket;

		class socket_disconnect_operation
			: public cppcoro::detail::win32_overlapped_operation<socket_disconnect_operation>
		{
		public:

			socket_disconnect_operation(socket& s) noexcept
				: m_socket(s)
			{}

		private:

			friend class cppcoro::detail::win32_overlapped_operation<socket_disconnect_operation>;

			bool try_start() noexcept;
			void get_result();

			socket& m_socket;

		};

		class socket_disconnect_operation_cancellable
			: public cppcoro::detail::win32_overlapped_operation_cancellable<socket_disconnect_operation_cancellable>
		{
		public:

			socket_disconnect_operation_cancellable(socket& s, cancellation_token&& ct) noexcept
				: cppcoro::detail::win32_overlapped_operation_cancellable<socket_disconnect_operation_cancellable>(std::move(ct))
				, m_socket(s)
			{}

		private:

			friend class cppcoro::detail::win32_overlapped_operation<socket_disconnect_operation>;

			bool try_start() noexcept;
			void cancel() noexcept;
			void get_result();

			socket& m_socket;

		};
	}
}

#endif // CPPCORO_OS_WINNT

#endif
