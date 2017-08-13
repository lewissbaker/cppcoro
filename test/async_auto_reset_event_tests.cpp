///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/async_auto_reset_event.hpp>

#include <cppcoro/config.hpp>
#include <cppcoro/lazy_task.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/when_all_ready.hpp>
#include <cppcoro/on_scope_exit.hpp>

#if CPPCORO_OS_WINNT
# include <cppcoro/io_service.hpp>
#endif

#include <thread>
#include <cassert>
#include <vector>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("async_auto_reset_event");

TEST_CASE("single waiter")
{
	cppcoro::async_auto_reset_event event;

	bool started = false;
	bool finished = false;
	auto run = [&]() -> cppcoro::lazy_task<>
	{
		started = true;
		co_await event;
		finished = true;
	};

	auto check = [&]() -> cppcoro::lazy_task<>
	{
		CHECK(started);
		CHECK(!finished);

		event.set();

		CHECK(finished);

		co_return;
	};

	cppcoro::sync_wait(cppcoro::when_all_ready(run(), check()));
}

TEST_CASE("multiple waiters")
{
	cppcoro::async_auto_reset_event event;

	
	auto run = [&](bool& flag) -> cppcoro::lazy_task<>
	{
		co_await event;
		flag = true;
	};

	bool completed1 = false;
	bool completed2 = false;

	auto check = [&]() -> cppcoro::lazy_task<>
	{
		CHECK(!completed1);
		CHECK(!completed2);

		event.set();

		CHECK(completed1);
		CHECK(!completed2);

		event.set();

		CHECK(completed2);

		co_return;
	};

	cppcoro::sync_wait(cppcoro::when_all_ready(
		run(completed1),
		run(completed2),
		check()));
}

#if CPPCORO_OS_WINNT

TEST_CASE("multi-threaded")
{
	cppcoro::io_service ioService;

	std::thread thread1{ [&] { ioService.process_events(); } };
	auto joinOnExit1 = cppcoro::on_scope_exit([&] { thread1.join(); });

	std::thread thread2{ [&] { ioService.process_events(); } };
	auto joinOnExit2 = cppcoro::on_scope_exit([&] { thread2.join(); });

	std::thread thread3{ [&] { ioService.process_events(); } };
	auto joinOnExit3 = cppcoro::on_scope_exit([&] { thread3.join(); });

	auto stopIoServiceOnExit = cppcoro::on_scope_exit([&] { ioService.stop(); });

	auto run = [&]() -> cppcoro::lazy_task<>
	{
		cppcoro::async_auto_reset_event event;

		int value = 0;

		auto startWaiter = [&]() -> cppcoro::lazy_task<>
		{
			co_await ioService.schedule();
			co_await event;
			++value;
			event.set();
		};

		auto startSignaller = [&]() -> cppcoro::lazy_task<>
		{
			co_await ioService.schedule();
			value = 5;
			event.set();
		};

		std::vector<cppcoro::lazy_task<>> tasks;

		tasks.emplace_back(startSignaller());

		for (int i = 0; i < 1000; ++i)
		{
			tasks.emplace_back(startWaiter());
		}

		co_await cppcoro::when_all(std::move(tasks));

		// NOTE: Can't use CHECK() here because it's not thread-safe
		assert(value == 1005);
	};

	std::vector<cppcoro::lazy_task<>> tasks;

	for (int i = 0; i < 1000; ++i)
	{
		tasks.emplace_back(run());
	}

	cppcoro::sync_wait(cppcoro::when_all(std::move(tasks)));
}

#endif

TEST_SUITE_END();
