#include <cppcoro/async_generator.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>

#include <cppcoro/io_service.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/operation_cancelled.hpp>

#include <iostream>

int main(int argc, char **argv) {
    using namespace std::chrono_literals;
    using namespace cppcoro;
    io_service ios;

    cancellation_source canceller;

    (void) sync_wait(when_all(
        [&]() -> task<> {
            auto _ = on_scope_exit([&] { ios.stop(); });

            try {
                co_await ios.schedule_after(1s, canceller.token());
            } catch (operation_cancelled &) {
                std::cout << "cancelled\n";
            }
        }(),
        [&]() -> task<> {
            canceller.request_cancellation();
            co_return;
        }(),
        [&]() -> task<> {
            ios.process_events();
            co_return;
        }()));
    return 0;
}
