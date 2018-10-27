///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/async_auto_reset_event.hpp>

#include <cppcoro/config.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/when_all_ready.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/static_thread_pool.hpp>

#include <thread>
#include <cassert>
#include <vector>

#include <ostream>
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

	auto check = [&]() -> cppcoro::task<>
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

	
	auto run = [&](bool& flag) -> cppcoro::task<>
	{
		co_await event;
		flag = true;
	};

	bool completed1 = false;
	bool completed2 = false;

	auto check = [&]() -> cppcoro::task<>
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

TEST_CASE("multi-threaded")
{
	cppcoro::static_thread_pool tp{ 3 };

	auto run = [&]() -> cppcoro::task<>
	{
		cppcoro::async_auto_reset_event event;

		int value = 0;

		auto startWaiter = [&]() -> cppcoro::task<>
		{
			co_await tp.schedule();
			co_await event;
			++value;
			event.set();
		};

		auto startSignaller = [&]() -> cppcoro::task<>
		{
			co_await tp.schedule();
			value = 5;
			event.set();
		};

		std::vector<cppcoro::task<>> tasks;

		tasks.emplace_back(startSignaller());

		for (int i = 0; i < 1000; ++i)
		{
			tasks.emplace_back(startWaiter());
		}

		co_await cppcoro::when_all(std::move(tasks));

		// NOTE: Can't use CHECK() here because it's not thread-safe
		assert(value == 1005);
	};

	std::vector<cppcoro::task<>> tasks;

	for (int i = 0; i < 1000; ++i)
	{
		tasks.emplace_back(run());
	}

	cppcoro::sync_wait(cppcoro::when_all(std::move(tasks)));
}

TEST_SUITE_END();
