///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/io_service.hpp>
#include <cppcoro/lazy_task.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/when_all_ready.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/operation_cancelled.hpp>
#include <cppcoro/cancellation_source.hpp>

#include <thread>
#include <vector>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("io_service");

TEST_CASE("default construct")
{
	cppcoro::io_service service;
	CHECK_FALSE(service.is_stop_requested());
}

TEST_CASE("construct with concurrency hint")
{
	cppcoro::io_service service{ 3 };
	CHECK_FALSE(service.is_stop_requested());
}

TEST_CASE("process_one_pending_event returns immediately when no events")
{
	cppcoro::io_service service;
	CHECK(service.process_one_pending_event() == 0);
	CHECK(service.process_pending_events() == 0);
}

TEST_CASE("schedule coroutine")
{
	cppcoro::io_service service;

	bool reachedPointA = false;
	bool reachedPointB = false;
	auto startTask = [&](cppcoro::io_service& ioService) -> cppcoro::lazy_task<>
	{
		reachedPointA = true;
		co_await ioService.schedule();
		reachedPointB = true;
	};

	cppcoro::sync_wait(cppcoro::when_all_ready(
		startTask(service),
		[&]() -> cppcoro::lazy_task<>
		{
			CHECK(reachedPointA);
			CHECK_FALSE(reachedPointB);

			service.process_pending_events();

			CHECK(reachedPointB);

			co_return;
		}()));
}

TEST_CASE("multiple I/O threads servicing events")
{
	cppcoro::io_service ioService;

	std::thread t1{ [&] { ioService.process_events(); } };
	auto waitForT1 = cppcoro::on_scope_exit([&] { t1.join(); });
	auto stopIoServiceOnExit1 = cppcoro::on_scope_exit([&] { ioService.stop();  });

	std::thread t2{ [&] { ioService.process_events(); } };
	auto waitForT2 = cppcoro::on_scope_exit([&] { t2.join(); });
	auto stopIoServiceOnExit2 = cppcoro::on_scope_exit([&] { ioService.stop();  });

	std::atomic<int> completedCount = 0;

	auto runOnIoThread = [&]() -> cppcoro::lazy_task<>
	{
		co_await ioService.schedule();
		++completedCount;
	};

	std::vector<cppcoro::lazy_task<>> tasks;
	{
		for (int i = 0; i < 1000; ++i)
		{
			tasks.emplace_back(runOnIoThread());
		}
	}

	cppcoro::sync_wait(cppcoro::when_all(std::move(tasks)));

	CHECK(completedCount == 1000);
}

TEST_CASE("Multiple concurrent timers")
{
	cppcoro::io_service ioService;

	auto startTimer = [&](std::chrono::milliseconds duration)
		-> cppcoro::lazy_task<std::chrono::high_resolution_clock::duration>
	{
		auto start = std::chrono::high_resolution_clock::now();

		co_await ioService.schedule_after(duration);

		auto end = std::chrono::high_resolution_clock::now();

		co_return end - start;
	};

	auto test = [&]() -> cppcoro::lazy_task<>
	{
		using namespace std::chrono;
		using namespace std::chrono_literals;

		auto[time1, time2, time3] = co_await cppcoro::when_all(
			startTimer(100ms),
			startTimer(120ms),
			startTimer(50ms));

		MESSAGE("Waiting 100ms took " << duration_cast<microseconds>(time1).count() << "us");
		MESSAGE("Waiting 120ms took " << duration_cast<microseconds>(time2).count() << "us");
		MESSAGE("Waiting 50ms took " << duration_cast<microseconds>(time3).count() << "us");

		CHECK(time1 >= 100ms);
		CHECK(time2 >= 120ms);
		CHECK(time3 >= 50ms);
	};

	cppcoro::sync_wait(cppcoro::when_all_ready(
		[&]() -> cppcoro::lazy_task<>
		{
			auto stopIoOnExit = cppcoro::on_scope_exit([&] { ioService.stop(); });
			co_await test();
		}(),
		[&]() -> cppcoro::lazy_task<>
		{
			ioService.process_events();
			co_return;
		}()));
}

TEST_CASE("Timer cancellation"
	* doctest::timeout{ 5.0 })
{
	using namespace std::literals::chrono_literals;

	cppcoro::io_service ioService;

	auto longWait = [&](cppcoro::cancellation_token ct) -> cppcoro::lazy_task<>
	{
		co_await ioService.schedule_after(20'000ms, ct);
	};

	auto cancelAfter = [&](cppcoro::cancellation_source source, auto duration) -> cppcoro::lazy_task<>
	{
		co_await ioService.schedule_after(duration);
		source.request_cancellation();
	};

	auto test = [&]() -> cppcoro::lazy_task<>
	{
		cppcoro::cancellation_source source;
		co_await cppcoro::when_all_ready(
			[&](cppcoro::cancellation_token ct) -> cppcoro::lazy_task<>
		{
			CHECK_THROWS_AS(co_await longWait(std::move(ct)), const cppcoro::operation_cancelled&);
		}(source.token()),
			cancelAfter(source, 1ms));
	};

	auto testTwice = [&]() -> cppcoro::lazy_task<>
	{
		co_await test();
		co_await test();
	};

	auto stopIoServiceAfter = [&](cppcoro::lazy_task<> task) -> cppcoro::lazy_task<>
	{
		co_await task.when_ready();
		ioService.stop();
		co_return co_await task.when_ready();
	};

	cppcoro::sync_wait(cppcoro::when_all_ready(
		stopIoServiceAfter(testTwice()),
		[&]() -> cppcoro::lazy_task<>
		{
			ioService.process_events();
			co_return;
		}()));
}

TEST_CASE("Many concurrent timers")
{
	cppcoro::io_service ioService;

	std::thread workerThread{ [&] { ioService.process_events(); } };
	auto joinOnExit = cppcoro::on_scope_exit([&] { workerThread.join(); });
	auto stopIoServiceOnExit = cppcoro::on_scope_exit([&] { ioService.stop(); });

	auto startTimer = [&]() -> cppcoro::lazy_task<>
	{
		using namespace std::literals::chrono_literals;
		co_await ioService.schedule_after(50ms);
	};

	constexpr std::uint32_t taskCount = 10'000;

	auto runManyTimers = [&]() -> cppcoro::lazy_task<>
	{
		std::vector<cppcoro::lazy_task<>> tasks;

		tasks.reserve(taskCount);

		for (std::uint32_t i = 0; i < taskCount; ++i)
		{
			tasks.emplace_back(startTimer());
		}

		co_await cppcoro::when_all(std::move(tasks));
	};

	auto start = std::chrono::high_resolution_clock::now();

	cppcoro::sync_wait(runManyTimers());

	auto end = std::chrono::high_resolution_clock::now();

	MESSAGE(
		"Waiting for " << taskCount << " x 50ms timers took "
		<< std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
		<< "ms");
}

TEST_SUITE_END();
