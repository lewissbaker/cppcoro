///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/io_service.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/when_all_ready.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/operation_cancelled.hpp>
#include <cppcoro/cancellation_source.hpp>

#include "io_service_fixture.hpp"

#include <thread>
#include <vector>

#include <ostream>
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
	auto startTask = [&](cppcoro::io_service& ioService) -> cppcoro::task<>
	{
		reachedPointA = true;
		co_await ioService.schedule();
		reachedPointB = true;
	};

	cppcoro::sync_wait(cppcoro::when_all_ready(
		startTask(service),
		[&]() -> cppcoro::task<>
		{
			CHECK(reachedPointA);
			CHECK_FALSE(reachedPointB);

			service.process_pending_events();

			CHECK(reachedPointB);

			co_return;
		}()));
}

TEST_CASE_FIXTURE(io_service_fixture_with_threads<2>, "multiple I/O threads servicing events")
{
	std::atomic<int> completedCount = 0;

	auto runOnIoThread = [&]() -> cppcoro::task<>
	{
		co_await io_service().schedule();
		++completedCount;
	};

	std::vector<cppcoro::task<>> tasks;
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
		-> cppcoro::task<std::chrono::high_resolution_clock::duration>
	{
		auto start = std::chrono::high_resolution_clock::now();

		co_await ioService.schedule_after(duration);

		auto end = std::chrono::high_resolution_clock::now();

		co_return end - start;
	};

	auto test = [&]() -> cppcoro::task<>
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
		[&]() -> cppcoro::task<>
		{
			auto stopIoOnExit = cppcoro::on_scope_exit([&] { ioService.stop(); });
			co_await test();
		}(),
		[&]() -> cppcoro::task<>
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

	auto longWait = [&](cppcoro::cancellation_token ct) -> cppcoro::task<>
	{
		co_await ioService.schedule_after(20'000ms, ct);
	};

	auto cancelAfter = [&](cppcoro::cancellation_source source, auto duration) -> cppcoro::task<>
	{
		co_await ioService.schedule_after(duration);
		source.request_cancellation();
	};

	auto test = [&]() -> cppcoro::task<>
	{
		cppcoro::cancellation_source source;
		co_await cppcoro::when_all_ready(
			[&](cppcoro::cancellation_token ct) -> cppcoro::task<>
		{
			CHECK_THROWS_AS(co_await longWait(std::move(ct)), const cppcoro::operation_cancelled&);
		}(source.token()),
			cancelAfter(source, 1ms));
	};

	auto testTwice = [&]() -> cppcoro::task<>
	{
		co_await test();
		co_await test();
	};

	auto stopIoServiceAfter = [&](cppcoro::task<> task) -> cppcoro::task<>
	{
		co_await task.when_ready();
		ioService.stop();
		co_return co_await task.when_ready();
	};

	cppcoro::sync_wait(cppcoro::when_all_ready(
		stopIoServiceAfter(testTwice()),
		[&]() -> cppcoro::task<>
		{
			ioService.process_events();
			co_return;
		}()));
}

TEST_CASE_FIXTURE(io_service_fixture_with_threads<1>, "Many concurrent timers")
{
	auto startTimer = [&]() -> cppcoro::task<>
	{
		using namespace std::literals::chrono_literals;
		co_await io_service().schedule_after(50ms);
	};

	constexpr std::uint32_t taskCount = 10'000;

	auto runManyTimers = [&]() -> cppcoro::task<>
	{
		std::vector<cppcoro::task<>> tasks;

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
