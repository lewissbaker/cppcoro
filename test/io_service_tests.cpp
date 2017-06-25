///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/io_service.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/on_scope_exit.hpp>

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

TEST_SUITE_END();
