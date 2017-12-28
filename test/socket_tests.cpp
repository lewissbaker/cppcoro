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

TEST_SUITE_END();
