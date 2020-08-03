#include <cppcoro/io_service.hpp>
#include <cppcoro/net/socket.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/cancellation_source.hpp>

#include <iostream>

int main(int argc, char **argv)
{
	std::vector<std::string_view> args{argv + 1, argv + argc};
	using namespace cppcoro;
	io_service ios;
	auto server_endpoint = net::ip_endpoint::from_string(args.empty() ? "127.0.0.1:2424" : args.at(0));
	std::cout << "listening at '" << server_endpoint->to_string() << "'\n";
    cancellation_source canceller;
	auto server = [&]() -> task<> {
		try
		{
			auto sock = net::socket::create_tcpv4(ios);
			sock.bind(*server_endpoint);
			std::string data;
			data.resize(256);
			sock.listen();
            auto client_sock = net::socket::create_tcpv4(ios);
            co_await sock.accept(client_sock);
			co_await client_sock.recv(data.data(), data.size());
            std::cout << "datagram from '" << client_sock.remote_endpoint().to_string() << "': " << data << '\n';
            co_await client_sock.send(data.data(), data.size());
		}
		catch (std::system_error &err)
		{
			std::cout << err.what() << '\n';
            canceller.request_cancellation();
			throw err;
		}
        co_return;
	};
	auto client = [&]() -> task<> {
        auto _ = on_scope_exit([&] { ios.stop(); });
		auto sock = net::socket::create_tcpv4(ios);
		std::string_view data = "Hello";
		co_await sock.connect(*server_endpoint);
		try
		{
			co_await sock.send(data.data(), data.size());
			std::string back;
			back.resize(data.size());
            co_await sock.recv(back.data(), back.size());
            assert(back == data);
		}
		catch (std::system_error &err)
		{
			std::cout << err.what() << '\n';
		}
		co_return;
	};
	(void)sync_wait(when_all(
		server(),
		client(),
		[&]() -> task<> {
			ios.process_events();
			co_return;
		}()));
	return 0;
}
