///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/io_service.hpp>
#include <cppcoro/task.hpp>
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
	auto startTask = [&](cppcoro::io_service& ioService) -> cppcoro::task<>
	{
		cppcoro::io_work_scope ioScope(ioService);
		reachedPointA = true;
		co_await ioService.schedule();
		reachedPointB = true;
	};

	{
		auto t = startTask(service);

		CHECK_FALSE(t.is_ready());
		CHECK(reachedPointA);
		CHECK_FALSE(reachedPointB);

		service.process_pending_events();

		CHECK(reachedPointB);
		CHECK(t.is_ready());

		CHECK(service.is_stop_requested());
	}
}

TEST_CASE("multiple I/O threads servicing events")
{
	cppcoro::io_service ioService;

	std::thread t1{ [&] { ioService.process_events(); } };
	auto waitForT1 = cppcoro::on_scope_exit([&] { t1.join(); });

	std::thread t2{ [&] { ioService.process_events(); } };
	auto waitForT2 = cppcoro::on_scope_exit([&] { t2.join(); });

	auto runOnIoThread = [&]() -> cppcoro::task<>
	{
		cppcoro::io_work_scope ioScope(ioService);
		co_await ioService.schedule();
	};

	std::vector<cppcoro::task<>> tasks;
	{
		cppcoro::io_work_scope ioScope(ioService);
		for (int i = 0; i < 1000; ++i)
		{
			tasks.emplace_back(runOnIoThread());
		}
	}

	waitForT1.call_now();
	waitForT2.call_now();

	// Thread won't have exited until all tasks complete
	// and io_context objects destruct.

	for (auto& task : tasks)
	{
		CHECK(task.is_ready());
	}
}

TEST_CASE("Multiple concurrent timers")
{
	cppcoro::io_service ioService;

	auto start = [&](std::chrono::milliseconds duration)
		-> cppcoro::task<std::chrono::high_resolution_clock::duration>
	{
		cppcoro::io_work_scope scope(ioService);

		auto start = std::chrono::high_resolution_clock::now();

		co_await ioService.schedule_after(duration);

		auto end = std::chrono::high_resolution_clock::now();

		co_return end - start;
	};

	using namespace std::literals::chrono_literals;
	auto t1 = start(100ms);
	auto t2 = start(120ms);
	auto t3 = start(50ms);

	ioService.process_events();

	REQUIRE(t1.is_ready());
	REQUIRE(t2.is_ready());
	REQUIRE(t3.is_ready());

	auto test = [&]() -> cppcoro::task<>
	{
		using namespace std::chrono;
		auto time1 = duration_cast<microseconds>(co_await t1);
		auto time2 = duration_cast<microseconds>(co_await t2);
		auto time3 = duration_cast<microseconds>(co_await t3);

		MESSAGE("Waiting 100ms took " << time1.count() << "us");
		MESSAGE("Waiting 120ms took " << time2.count() << "us");
		MESSAGE("Waiting 50ms took " << time3.count() << "us");

		CHECK(time1 >= 100ms);
		CHECK(time2 >= 120ms);
		CHECK(time3 >= 50ms);
	};

	test();
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


	auto test = [&]() -> cppcoro::task<>
	{
		cppcoro::io_work_scope scope(ioService);


		{
			cppcoro::cancellation_source source;
			auto t = longWait(source.token());
			co_await ioService.schedule_after(1ms);
			source.request_cancellation();
			CHECK_THROWS_AS(co_await t, cppcoro::operation_cancelled);
		}

		// Check that a second timer cancellation is also promptly responded-to.
		{
			cppcoro::cancellation_source source;
			auto t = longWait(source.token());
			co_await ioService.schedule_after(1ms);
			source.request_cancellation();
			CHECK_THROWS_AS(co_await t, cppcoro::operation_cancelled);
		}
	};

	auto t = test();

	ioService.process_events();

	REQUIRE(t.is_ready());
}

TEST_CASE("Many concurrent timers")
{
	cppcoro::io_service ioService;

	std::thread workerThread{ [&] { ioService.process_events(); } };
	auto joinOnExit = cppcoro::on_scope_exit([&] { workerThread.join(); });

	auto startTimer = [&]() -> cppcoro::task<>
	{
		using namespace std::literals::chrono_literals;
		co_await ioService.schedule_after(50ms);
	};

	constexpr std::uint32_t taskCount = 10'000;

	auto runManyTimers = [&]() -> cppcoro::task<>
	{
		cppcoro::io_work_scope scope(ioService);

		std::vector<cppcoro::task<>> tasks;

		tasks.reserve(taskCount);

		for (std::uint32_t i = 0; i < taskCount; ++i)
		{
			tasks.emplace_back(startTimer());
		}

		for (auto& t : tasks)
		{
			co_await t;
		}
	};

	auto start = std::chrono::high_resolution_clock::now();

	auto t = runManyTimers();

	joinOnExit.call_now();

	auto end = std::chrono::high_resolution_clock::now();

	MESSAGE(
		"Waiting for " << taskCount << " x 50ms timers took "
		<< std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
		<< "ms");
}

TEST_SUITE_END();
