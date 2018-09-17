///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_NET_SOCKET_HPP_INCLUDED
#define CPPCORO_NET_SOCKET_HPP_INCLUDED

#include <cppcoro/config.hpp>

#include <cppcoro/net/ip_endpoint.hpp>
#include <cppcoro/net/socket_accept_operation.hpp>
#include <cppcoro/net/socket_connect_operation.hpp>
#include <cppcoro/net/socket_disconnect_operation.hpp>
#include <cppcoro/net/socket_recv_operation.hpp>
#include <cppcoro/net/socket_recv_from_operation.hpp>
#include <cppcoro/net/socket_send_operation.hpp>
#include <cppcoro/net/socket_send_to_operation.hpp>

#include <cppcoro/cancellation_token.hpp>

#if CPPCORO_OS_WINNT
# include <cppcoro/detail/win32.hpp>
#endif

namespace cppcoro
{
	class io_service;

	namespace net
	{
		class socket
		{
		public:

			/// Create a socket that can be used to communicate using TCP/IPv4 protocol.
			///
			/// \param ioSvc
			/// The I/O service the socket will use for dispatching I/O completion events.
			///
			/// \return
			/// The newly created socket.
			///
			/// \throws std::system_error
			/// If the socket could not be created for some reason.
			static socket create_tcpv4(io_service& ioSvc);

			/// Create a socket that can be used to communicate using TCP/IPv6 protocol.
			///
			/// \param ioSvc
			/// The I/O service the socket will use for dispatching I/O completion events.
			///
			/// \return
			/// The newly created socket.
			///
			/// \throws std::system_error
			/// If the socket could not be created for some reason.
			static socket create_tcpv6(io_service& ioSvc);

			/// Create a socket that can be used to communicate using UDP/IPv4 protocol.
			///
			/// \param ioSvc
			/// The I/O service the socket will use for dispatching I/O completion events.
			///
			/// \return
			/// The newly created socket.
			///
			/// \throws std::system_error
			/// If the socket could not be created for some reason.
			static socket create_udpv4(io_service& ioSvc);

			/// Create a socket that can be used to communicate using UDP/IPv6 protocol.
			///
			/// \param ioSvc
			/// The I/O service the socket will use for dispatching I/O completion events.
			///
			/// \return
			/// The newly created socket.
			///
			/// \throws std::system_error
			/// If the socket could not be created for some reason.
			static socket create_udpv6(io_service& ioSvc);

			socket(socket&& other) noexcept;

			/// Closes the socket, releasing any associated resources.
			///
			/// If the socket still has an open connection then the connection will be
			/// reset. The destructor will not block waiting for queueud data to be sent.
			/// If you need to ensure that queued data is delivered then you must call
			/// disconnect() and wait until the disconnect operation completes.
			~socket();

			socket& operator=(socket&& other) noexcept;

#if CPPCORO_OS_WINNT
			/// Get the Win32 socket handle assocaited with this socket.
			cppcoro::detail::win32::socket_t native_handle() noexcept { return m_handle; }

			/// Query whether I/O operations that complete synchronously will skip posting
			/// an I/O completion event to the I/O completion port.
			///
			/// The operation class implementations can use this to determine whether or not
			/// it should immediately resume the coroutine on the current thread upon an
			/// operation completing synchronously or whether it should suspend the coroutine
			/// and wait until the I/O completion event is dispatched to an I/O thread.
			bool skip_completion_on_success() noexcept { return m_skipCompletionOnSuccess; }
#endif

			/// Get the address and port of the local end-point.
			///
			/// If the socket is not bound then this will be the unspecified end-point
			/// of the socket's associated address-family.
			const ip_endpoint& local_endpoint() const noexcept { return m_localEndPoint; }

			/// Get the address and port of the remote end-point.
			///
			/// If the socket is not in the connected state then this will be the unspecified
			/// end-point of the socket's associated address-family.
			const ip_endpoint& remote_endpoint() const noexcept { return m_remoteEndPoint; }

			/// Bind the local end of this socket to the specified local end-point.
			///
			/// \param localEndPoint
			/// The end-point to bind to.
			/// This can be either an unspecified address (in which case it binds to all available
			/// interfaces) and/or an unspecified port (in which case a random port is allocated).
			///
			/// \throws std::system_error
			/// If the socket could not be bound for some reason.
			void bind(const ip_endpoint& localEndPoint);

			/// Put the socket into a passive listening state that will start acknowledging
			/// and queueing up new connections ready to be accepted by a call to 'accept()'.
			///
			/// The backlog of connections ready to be accepted will be set to some default
			/// suitable large value, depending on the network provider. If you need more
			/// control over the size of the queue then use the overload of listen()
			/// that accepts a 'backlog' parameter.
			///
			/// \throws std::system_error
			/// If the socket could not be placed into a listening mode.
			void listen();

			/// Put the socket into a passive listening state that will start acknowledging
			/// and queueing up new connections ready to be accepted by a call to 'accept()'.
			///
			/// \param backlog
			/// The maximum number of pending connections to allow in the queue of ready-to-accept
			/// connections.
			///
			/// \throws std::system_error
			/// If the socket could not be placed into a listening mode.
			void listen(std::uint32_t backlog);

			/// Connect the socket to the specified remote end-point.
			///
			/// The socket must be in a bound but unconnected state prior to this call.
			///
			/// \param remoteEndPoint
			/// The IP address and port-number to connect to.
			///
			/// \return
			/// An awaitable object that must be co_await'ed to perform the async connect
			/// operation. The result of the co_await expression is type void.
			[[nodiscard]]
			socket_connect_operation connect(const ip_endpoint& remoteEndPoint) noexcept;

			/// Connect to the specified remote end-point.
			///
			/// \param remoteEndPoint
			/// The IP address and port of the remote end-point to connect to.
			///
			/// \param ct
			/// A cancellation token that can be used to communicate a request to
			/// later cancel the operation. If the operation is successfully
			/// cancelled then it will complete by throwing a cppcoro::operation_cancelled
			/// exception.
			///
			/// \return
			/// An awaitable object that will start the connect operation when co_await'ed
			/// and will suspend the coroutine, resuming it when the operation completes.
			/// The result of the co_await expression has type 'void'.
			[[nodiscard]]
			socket_connect_operation_cancellable connect(
				const ip_endpoint& remoteEndPoint,
				cancellation_token ct) noexcept;

			[[nodiscard]]
			socket_accept_operation accept(socket& acceptingSocket) noexcept;
			[[nodiscard]]
			socket_accept_operation_cancellable accept(
				socket& acceptingSocket,
				cancellation_token ct) noexcept;

			[[nodiscard]]
			socket_disconnect_operation disconnect() noexcept;
			[[nodiscard]]
			socket_disconnect_operation_cancellable disconnect(cancellation_token ct) noexcept;

			[[nodiscard]]
			socket_send_operation send(
				const void* buffer,
				std::size_t size) noexcept;
			[[nodiscard]]
			socket_send_operation_cancellable send(
				const void* buffer,
				std::size_t size,
				cancellation_token ct) noexcept;

			[[nodiscard]]
			socket_recv_operation recv(
				void* buffer,
				std::size_t size) noexcept;
			[[nodiscard]]
			socket_recv_operation_cancellable recv(
				void* buffer,
				std::size_t size,
				cancellation_token ct) noexcept;

			[[nodiscard]]
			socket_recv_from_operation recv_from(
				void* buffer,
				std::size_t size) noexcept;
			[[nodiscard]]
			socket_recv_from_operation_cancellable recv_from(
				void* buffer,
				std::size_t size,
				cancellation_token ct) noexcept;

			[[nodiscard]]
			socket_send_to_operation send_to(
				const ip_endpoint& destination,
				const void* buffer,
				std::size_t size) noexcept;
			[[nodiscard]]
			socket_send_to_operation_cancellable send_to(
				const ip_endpoint& destination,
				const void* buffer,
				std::size_t size,
				cancellation_token ct) noexcept;

			void close_send();
			void close_recv();

		private:

			friend class socket_accept_operation_impl;
			friend class socket_connect_operation_impl;

#if CPPCORO_OS_WINNT
			explicit socket(
				cppcoro::detail::win32::socket_t handle,
				bool skipCompletionOnSuccess) noexcept;
#endif

#if CPPCORO_OS_WINNT
			cppcoro::detail::win32::socket_t m_handle;
			bool m_skipCompletionOnSuccess;
#endif

			ip_endpoint m_localEndPoint;
			ip_endpoint m_remoteEndPoint;

		};
	}
}

#endif
