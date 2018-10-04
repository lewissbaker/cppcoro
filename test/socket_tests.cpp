///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/io_service.hpp>
#include <cppcoro/net/socket.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/cancellation_token.hpp>
#include <cppcoro/async_scope.hpp>

#include "doctest/doctest.h"

using namespace cppcoro;
using namespace cppcoro::net;

TEST_SUITE_BEGIN("socket");

TEST_CASE("create TCP/IPv4")
{
	io_service ioSvc;
	auto socket = socket::create_tcpv4(ioSvc);
}

TEST_CASE("create TCP/IPv6")
{
	io_service ioSvc;
	auto socket = socket::create_tcpv6(ioSvc);
}

TEST_CASE("create UDP/IPv4")
{
	io_service ioSvc;
	auto socket = socket::create_udpv4(ioSvc);
}

TEST_CASE("create UDP/IPv6")
{
	io_service ioSvc;
	auto socket = socket::create_udpv6(ioSvc);
}

TEST_CASE("TCP/IPv4 connect/disconnect")
{
	io_service ioSvc;

	ip_endpoint serverAddress;

	task<int> serverTask;

	auto server = [&](socket listeningSocket) -> task<int>
	{
		auto s = socket::create_tcpv4(ioSvc);
		co_await listeningSocket.accept(s);
		co_await s.disconnect();
		co_return 0;
	};

	{
		auto serverSocket = socket::create_tcpv4(ioSvc);
		serverSocket.bind(ipv4_endpoint{ ipv4_address::loopback(), 0 });
		serverSocket.listen(3);
		serverAddress = serverSocket.local_endpoint();
		serverTask = server(std::move(serverSocket));
	}

	auto client = [&]() -> task<int>
	{
		auto s = socket::create_tcpv4(ioSvc);
		s.bind(ipv4_endpoint{ ipv4_address::loopback(), 0 });
		co_await s.connect(serverAddress);
		co_await s.disconnect();
		co_return 0;
	};

	task<int> clientTask = client();

	(void)sync_wait(when_all(
		[&]() -> task<int>
		{
			auto stopOnExit = on_scope_exit([&] { ioSvc.stop(); });
			(void)co_await when_all(std::move(serverTask), std::move(clientTask));
			co_return 0;
		}(),
		[&]() -> task<int>
		{
			ioSvc.process_events();
			co_return 0;
		}()));
}

TEST_CASE("send/recv TCP/IPv4")
{
	io_service ioSvc;

	auto listeningSocket = socket::create_tcpv4(ioSvc);

	listeningSocket.bind(ipv4_endpoint{ ipv4_address::loopback(), 0 });
	listeningSocket.listen(3);

	auto echoServer = [&]() -> task<int>
	{
		auto acceptingSocket = socket::create_tcpv4(ioSvc);

		co_await listeningSocket.accept(acceptingSocket);

		std::uint8_t buffer[64];
		std::size_t bytesReceived;
		do
		{
			bytesReceived = co_await acceptingSocket.recv(buffer, sizeof(buffer));
			if (bytesReceived > 0)
			{
				std::size_t bytesSent = 0;
				do
				{
					bytesSent += co_await acceptingSocket.send(
						buffer + bytesSent,
						bytesReceived - bytesSent);
				} while (bytesSent < bytesReceived);
			}
		} while (bytesReceived > 0);

		acceptingSocket.close_send();

		co_await acceptingSocket.disconnect();

		co_return 0;
	};

	auto echoClient = [&]() -> task<int>
	{
		auto connectingSocket = socket::create_tcpv4(ioSvc);

		connectingSocket.bind(ipv4_endpoint{});

		co_await connectingSocket.connect(listeningSocket.local_endpoint());

		auto receive = [&]() -> task<int>
		{
			std::uint8_t buffer[100];
			std::uint64_t totalBytesReceived = 0;
			std::size_t bytesReceived;
			do
			{
				bytesReceived = co_await connectingSocket.recv(buffer, sizeof(buffer));
				for (std::size_t i = 0; i < bytesReceived; ++i)
				{
					std::uint64_t byteIndex = totalBytesReceived + i;
					std::uint8_t expectedByte = 'a' + (byteIndex % 26);
					CHECK(buffer[i] == expectedByte);
				}

				totalBytesReceived += bytesReceived;
			} while (bytesReceived > 0);

			CHECK(totalBytesReceived == 1000);

			co_return 0;
		};

		auto send = [&]() -> task<int>
		{
			std::uint8_t buffer[100];
			for (std::uint64_t i = 0; i < 1000; i += sizeof(buffer))
			{
				for (std::size_t j = 0; j < sizeof(buffer); ++j)
				{
					buffer[j] = 'a' + ((i + j) % 26);
				}

				std::size_t bytesSent = 0;
				do
				{
					bytesSent += co_await connectingSocket.send(buffer + bytesSent, sizeof(buffer) - bytesSent);
				} while (bytesSent < sizeof(buffer));
			}

			connectingSocket.close_send();

			co_return 0;
		};

		co_await when_all(send(), receive());

		co_await connectingSocket.disconnect();

		co_return 0;
	};

	(void)sync_wait(when_all(
		[&]() -> task<int>
		{
			auto stopOnExit = on_scope_exit([&] { ioSvc.stop(); });
			(void)co_await when_all(echoClient(), echoServer());
			co_return 0;
		}(),
		[&]() -> task<int>
		{
			ioSvc.process_events();
			co_return 0;
		}()));
}

#if !CPPCORO_COMPILER_MSVC || CPPCORO_COMPILER_MSVC >= 191626926 || !CPPCORO_CPU_X86
// HACK: Don't compile this function under MSVC x86.
// It results in an ICE under VS 2017.8 / 2017.9 Preview 1.
// Seems to be fixed in 2017.9 Preview 3

TEST_CASE("send/recv TCP/IPv4 many connections")
{
	io_service ioSvc;

	auto listeningSocket = socket::create_tcpv4(ioSvc);

	listeningSocket.bind(ipv4_endpoint{ ipv4_address::loopback(), 0 });
	listeningSocket.listen(20);

	cancellation_source canceller;

	auto handleConnection = [](socket s) -> task<void>
	{
		std::uint8_t buffer[64];
		std::size_t bytesReceived;
		do
		{
			bytesReceived = co_await s.recv(buffer, sizeof(buffer));
			if (bytesReceived > 0)
			{
				std::size_t bytesSent = 0;
				do
				{
					bytesSent += co_await s.send(
						buffer + bytesSent,
						bytesReceived - bytesSent);
				} while (bytesSent < bytesReceived);
			}
		} while (bytesReceived > 0);

		s.close_send();

		co_await s.disconnect();
	};

	auto echoServer = [&](cancellation_token ct) -> task<>
	{
		async_scope connectionScope;

		std::exception_ptr ex;
		try
		{
			while (true) {
				auto acceptingSocket = socket::create_tcpv4(ioSvc);
				co_await listeningSocket.accept(acceptingSocket, ct);
				connectionScope.spawn(
					handleConnection(std::move(acceptingSocket)));
			}
		}
		catch (const cppcoro::operation_cancelled&)
		{
		}
		catch (...)
		{
			ex = std::current_exception();
		}

		co_await connectionScope.join();

		if (ex)
		{
			std::rethrow_exception(ex);
		}
	};

	auto echoClient = [&]() -> task<>
	{
		auto connectingSocket = socket::create_tcpv4(ioSvc);

		connectingSocket.bind(ipv4_endpoint{});

		co_await connectingSocket.connect(listeningSocket.local_endpoint());

		auto receive = [&]() -> task<>
		{
			std::uint8_t buffer[100];
			std::uint64_t totalBytesReceived = 0;
			std::size_t bytesReceived;
			do
			{
				bytesReceived = co_await connectingSocket.recv(buffer, sizeof(buffer));
				for (std::size_t i = 0; i < bytesReceived; ++i)
				{
					std::uint64_t byteIndex = totalBytesReceived + i;
					std::uint8_t expectedByte = 'a' + (byteIndex % 26);
					CHECK(buffer[i] == expectedByte);
				}

				totalBytesReceived += bytesReceived;
			} while (bytesReceived > 0);

			CHECK(totalBytesReceived == 1000);
		};

		auto send = [&]() -> task<>
		{
			std::uint8_t buffer[100];
			for (std::uint64_t i = 0; i < 1000; i += sizeof(buffer))
			{
				for (std::size_t j = 0; j < sizeof(buffer); ++j)
				{
					buffer[j] = 'a' + ((i + j) % 26);
				}

				std::size_t bytesSent = 0;
				do
				{
					bytesSent += co_await connectingSocket.send(buffer + bytesSent, sizeof(buffer) - bytesSent);
				} while (bytesSent < sizeof(buffer));
			}

			connectingSocket.close_send();
		};

		co_await when_all(send(), receive());

		co_await connectingSocket.disconnect();
	};

	auto manyEchoClients = [&](int count) -> task<void>
	{
		auto shutdownServerOnExit = on_scope_exit([&]
		{
			canceller.request_cancellation();
		});

		std::vector<task<>> clientTasks;
		clientTasks.reserve(count);

		for (int i = 0; i < count; ++i)
		{
			clientTasks.emplace_back(echoClient());
		}

		co_await when_all(std::move(clientTasks));
	};

	(void)sync_wait(when_all(
		[&]() -> task<>
		{
			auto stopOnExit = on_scope_exit([&] { ioSvc.stop(); });
			(void)co_await when_all(
				manyEchoClients(20),
				echoServer(canceller.token()));
		}(),
		[&]() -> task<>
		{
			ioSvc.process_events();
			co_return;
		}()));
}

#endif

TEST_CASE("udp send_to/recv_from")
{
	io_service ioSvc;

	auto server = [&](socket serverSocket) -> task<int>
	{
		std::uint8_t buffer[100];

		auto[bytesReceived, remoteEndPoint] = co_await serverSocket.recv_from(buffer, 100);
		CHECK(bytesReceived == 50);

		// Send an ACK response.
		{
			const std::uint8_t response[1] = { 0 };
			co_await serverSocket.send_to(remoteEndPoint, &response, 1);
		}

		// Second message received won't fit within buffer.
		try
		{
			std::tie(bytesReceived, remoteEndPoint) = co_await serverSocket.recv_from(buffer, 100);
			FAIL("Should have thrown");
		}
		catch (const std::system_error&)
		{
			// TODO: Map this situation to some kind of error_condition value.
			// The win32 ERROR_MORE_DATA error code doesn't seem to map to any of the standard std::errc values.
			//
			// CHECK(ex.code() == ???);
			//
			// Possibly also need to switch to returning a std::error_code directly rather than
			// throwing a std::system_error for this case.
		}

		// Send an NACK response.
		{
			const std::uint8_t response[1] = { 1 };
			co_await serverSocket.send_to(remoteEndPoint, response, 1);
		}

		co_return 0;
	};

	ip_endpoint serverAddress;

	task<int> serverTask;

	{
		auto serverSocket = socket::create_udpv4(ioSvc);
		serverSocket.bind(ipv4_endpoint{ ipv4_address::loopback(), 0 });
		serverAddress = serverSocket.local_endpoint();
		serverTask = server(std::move(serverSocket));
	}

	auto client = [&]() -> task<int>
	{
		auto socket = socket::create_udpv4(ioSvc);

		// don't need to bind(), should be implicitly bound on first send_to().

		// Send first message of 50 bytes
		{
			std::uint8_t buffer[50] = { 0 };
			co_await socket.send_to(serverAddress, buffer, 50);
		}

		// Receive ACK message
		{
			std::uint8_t buffer[1];
			auto[bytesReceived, ackAddress] = co_await socket.recv_from(buffer, 1);
			CHECK(bytesReceived == 1);
			CHECK(buffer[0] == 0);
			CHECK(ackAddress == serverAddress);
		}

		// Send second message of 128 bytes
		{
			std::uint8_t buffer[128] = { 0 };
			co_await socket.send_to(serverAddress, buffer, 128);
		}

		// Receive NACK message
		{
			std::uint8_t buffer[1];
			auto[bytesReceived, ackAddress] = co_await socket.recv_from(buffer, 1);
			CHECK(bytesReceived == 1);
			CHECK(buffer[0] == 1);
			CHECK(ackAddress == serverAddress);
		}

		co_return 0;
	};

	(void)sync_wait(when_all(
		[&]() -> task<int>
		{
			auto stopOnExit = on_scope_exit([&] { ioSvc.stop(); });
			(void)co_await when_all(std::move(serverTask), client());
			co_return 0;
		}(),
		[&]() -> task<int>
		{
			ioSvc.process_events();
			co_return 0;
		}()));
}

TEST_SUITE_END();
