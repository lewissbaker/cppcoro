///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_NET_SOCKET_RECV_FROM_OPERATION_HPP_INCLUDED
#define CPPCORO_NET_SOCKET_RECV_FROM_OPERATION_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/cancellation_token.hpp>
#include <cppcoro/net/ip_endpoint.hpp>

#include <cstdint>
#include <tuple>

#if CPPCORO_OS_WINNT
#include <cppcoro/detail/win32.hpp>
#include <cppcoro/detail/win32_overlapped_operation.hpp>
#elif CPPCORO_OS_LINUX
#include <cppcoro/detail/linux_uring_operation.hpp>
#endif

namespace cppcoro::net
{
	class socket;

	class socket_recv_from_operation_impl
	{
	public:
		socket_recv_from_operation_impl(
			socket& socket, void* buffer, std::size_t byteCount) noexcept
			: m_socket(socket)
			, m_buffer(buffer, byteCount)
		{
		}

		bool try_start(cppcoro::detail::io_operation_base& operation) noexcept;
		void cancel(cppcoro::detail::io_operation_base& operation) noexcept;
		std::tuple<std::size_t, ip_endpoint>
		get_result(cppcoro::detail::io_operation_base& operation);

	private:
		socket& m_socket;
		cppcoro::detail::sock_buf m_buffer;

		static constexpr std::size_t sockaddrStorageAlignment = 4;

		// Storage suitable for either SOCKADDR_IN or SOCKADDR_IN6
		alignas(sockaddrStorageAlignment) std::uint8_t m_sourceSockaddrStorage[28];
		int m_sourceSockaddrLength;
	};

	class socket_recv_from_operation
		: public cppcoro::detail::io_operation<socket_recv_from_operation>
	{
	public:
		socket_recv_from_operation(
#if CPPCORO_OS_LINUX
			io_service& ioService,
#endif
			socket& socket,
			void* buffer,
			std::size_t byteCount) noexcept
			: cppcoro::detail::io_operation<socket_recv_from_operation>(
#if CPPCORO_OS_LINUX
				  ioService
#endif
				  )
			, m_impl(socket, buffer, byteCount)
		{
		}

	private:
		friend cppcoro::detail::io_operation<socket_recv_from_operation>;

		bool try_start() noexcept { return m_impl.try_start(*this); }
		decltype(auto) get_result() { return m_impl.get_result(*this); }

		socket_recv_from_operation_impl m_impl;
	};

	class socket_recv_from_operation_cancellable
		: public cppcoro::detail::io_operation_cancellable<socket_recv_from_operation_cancellable>
	{
	public:
		socket_recv_from_operation_cancellable(
#if CPPCORO_OS_LINUX
			io_service& ioService,
#endif
			socket& socket,
			void* buffer,
			std::size_t byteCount,
			cancellation_token&& ct) noexcept
			: cppcoro::detail::io_operation_cancellable<socket_recv_from_operation_cancellable>(
#if CPPCORO_OS_LINUX
				  ioService,
#endif
				  std::move(ct))
			, m_impl(socket, buffer, byteCount)
		{
		}

	private:
		friend cppcoro::detail::io_operation_cancellable<socket_recv_from_operation_cancellable>;

		bool try_start() noexcept { return m_impl.try_start(*this); }
		void cancel() noexcept { m_impl.cancel(*this); }
		decltype(auto) get_result() { return m_impl.get_result(*this); }

		socket_recv_from_operation_impl m_impl;
	};

}  // namespace cppcoro::net

#endif
