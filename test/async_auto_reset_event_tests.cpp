///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/async_auto_reset_event.hpp>

#include <cppcoro/task.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/on_scope_exit.hpp>

#include <thread>
#include <cassert>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("async_auto_reset_event");

TEST_CASE("single waiter")
{
	cppcoro::async_auto_reset_event event;

	bool started = false;
	bool finished = false;
	auto run = [&]() -> cppcoro::task<>
	{
		started = true;
		co_await event;
		finished = true;
	};

	auto t = run();

	CHECK(started);
	CHECK(!finished);

	event.set();

	CHECK(finished);
}

TEST_CASE("multiple waiters")
{
	cppcoro::async_auto_reset_event event;

	auto run = [&]() -> cppcoro::task<>
	{
		co_await event;
	};

	auto t1 = run();
	auto t2 = run();

	CHECK(!t1.is_ready());
	CHECK(!t2.is_ready());

	event.set();

	CHECK(t1.is_ready());
	CHECK(!t2.is_ready());

	event.set();

	CHECK(t1.is_ready());
	CHECK(t2.is_ready());
}

TEST_CASE("multi-threaded")
{
	cppcoro::io_service ioService;

	std::thread thread1{ [&] { ioService.process_events(); } };
	auto joinOnExit1 = cppcoro::on_scope_exit([&] { thread1.join(); });

	std::thread thread2{ [&] { ioService.process_events(); } };
	auto joinOnExit2 = cppcoro::on_scope_exit([&] { thread2.join(); });

	std::thread thread3{ [&] { ioService.process_events(); } };
	auto joinOnExit3 = cppcoro::on_scope_exit([&] { thread3.join(); });

	auto run = [&]() -> cppcoro::task<>
	{
		cppcoro::async_auto_reset_event event;

		int value = 0;

		auto startWaiter = [&]() -> cppcoro::task<>
		{
			co_await ioService.schedule();
			co_await event;
			++value;
			event.set();
		};

		auto startSignaller = [&]() -> cppcoro::task<>
		{
			co_await ioService.schedule();
			value = 5;
			event.set();
		};

		std::vector<cppcoro::task<>> waiters;

		for (int i = 0; i < 1000; ++i)
		{
			waiters.emplace_back(startWaiter());
		}

		co_await startSignaller();

		for (auto& waiter : waiters)
		{
			co_await waiter;
		}

		// NOTE: Can't use CHECK() here because it's not thread-safe
		assert(value == 1005);
	};

	auto runMany = [&]() -> cppcoro::task<>
	{
		std::vector<cppcoro::task<>> tasks;

		for (int i = 0; i < 1000; ++i)
		{
			tasks.emplace_back(run());
		}

		for (auto& t : tasks)
		{
			co_await t;
		}

		ioService.stop();
	};

	auto t = runMany();

	joinOnExit1.call_now();
	joinOnExit2.call_now();
	joinOnExit3.call_now();
}

TEST_SUITE_END();
