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

	class socket_send_operation_impl
	{
	public:

		socket_send_operation_impl(
			socket& s,
			const void* buffer,
			std::size_t byteCount) noexcept
			: m_socket(s)
			, m_buffer(const_cast<void*>(buffer), byteCount)
		{}

		bool try_start(cppcoro::detail::win32_overlapped_operation_base& operation) noexcept;
		void cancel(cppcoro::detail::win32_overlapped_operation_base& operation) noexcept;

	private:

		socket& m_socket;
		cppcoro::detail::win32::wsabuf m_buffer;

	};

	class socket_send_operation
		: public cppcoro::detail::win32_overlapped_operation<socket_send_operation>
	{
	public:

		socket_send_operation(
			socket& s,
			const void* buffer,
			std::size_t byteCount) noexcept
			: m_impl(s, buffer, byteCount)
		{}

	private:

		friend class cppcoro::detail::win32_overlapped_operation<socket_send_operation>;

		bool try_start() noexcept { return m_impl.try_start(*this); }

		socket_send_operation_impl m_impl;

	};

	class socket_send_operation_cancellable
		: public cppcoro::detail::win32_overlapped_operation_cancellable<socket_send_operation_cancellable>
	{
	public:

		socket_send_operation_cancellable(
			socket& s,
			const void* buffer,
			std::size_t byteCount,
			cancellation_token&& ct) noexcept
			: cppcoro::detail::win32_overlapped_operation_cancellable<socket_send_operation_cancellable>(std::move(ct))
			, m_impl(s, buffer, byteCount)
		{}

	private:

		friend class cppcoro::detail::win32_overlapped_operation_cancellable<socket_send_operation_cancellable>;

		bool try_start() noexcept { return m_impl.try_start(*this); }
		void cancel() noexcept { return m_impl.cancel(*this); }

		socket_send_operation_impl m_impl;

	};

}

#endif // CPPCORO_OS_WINNT

#endif
