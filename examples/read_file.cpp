#include <cppcoro/async_generator.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>

#include <cppcoro/io_service.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/read_only_file.hpp>
#include <cppcoro/read_write_file.hpp>
#include <cppcoro/cancellation_source.hpp>

#include <iostream>

int main(int argc, char **argv) {
    using namespace std::chrono_literals;
    using namespace cppcoro;
    io_service ios;
    std::string check;
    std::string content{"Hello world"};

    cancellation_source canceller;

    canceller.request_cancellation();

    (void) sync_wait(when_all(
            [&]() -> task<> {
                auto _ = on_scope_exit([&] { ios.stop(); });

                std::string tmp;
                auto this_file = read_only_file::open(ios, __FILE__);
                tmp.resize(this_file.size());
                try {
                    // first attempt will probably terminate successfully before cancel has been requested
                    co_await this_file.read(0, tmp.data(), tmp.size(), canceller.token());
                    assert(false);
                } catch (operation_cancelled &) {
                    std::cout << "Cancelled\n";
                }

                auto f = read_write_file::open(ios, "./test.txt", file_open_mode::create_always);

                check.resize(content.size());

                co_await f.write(0, content.data(), content.size());
                co_await f.read(0, check.data(), check.size());

                assert(check == content);

                std::cout << "got: " << check << '\n';
            }(),
            [&]() -> task<> {
                ios.process_events();
                co_return;
            }()));
    return 0;
}
