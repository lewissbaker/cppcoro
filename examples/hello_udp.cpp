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
	auto server_endpoint = net::ip_endpoint::from_string(args.empty() ? "127.0.0.1:4242" : args.at(0));
	std::cout << "listening at '" << server_endpoint->to_string() << "'\n";
	cancellation_source canceller;
	auto server = [&]() -> task<> {
		auto _ = on_scope_exit([&] { ios.stop(); });
		try
		{
			auto sock = net::socket::create_udpv4(ios);
			sock.bind(*server_endpoint);
			std::string data;
			data.resize(256);
			try {
                auto [rc, from] = co_await sock.recv_from(data.data(), data.size(), canceller.token());
                assert(0);
			} catch (operation_cancelled &) {
			    std::cout << "Cancelled\n";
            }
			auto [rc, from] = co_await sock.recv_from(data.data(), data.size());
            std::cout << "datagram from '" << from.to_string() << "': " << data << '\n';
		}
		catch (std::system_error &err)
		{
			std::cout << err.what() << '\n';
		}
	};
	auto client = [&]() -> task<> {
	    using namespace std::chrono_literals;
	    // cancel first server recv
        canceller.request_cancellation();
        co_await ios.schedule_after(1s);
		auto sock = net::socket::create_udpv4(ios);
		std::string_view data = "Hello";
		co_await ios.schedule();
		try
		{
			co_await sock.send_to(*server_endpoint, data.data(), data.size());
		}
		catch (std::system_error &err)
		{
			std::cout << err.what() << '\n';
		}
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
