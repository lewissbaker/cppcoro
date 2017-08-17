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

#if CPPCORO_OS_WINNT
# include <cppcoro/io_service.hpp>
# include "io_service_fixture.hpp"
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

#if CPPCORO_OS_WINNT

TEST_CASE_FIXTURE(io_service_fixture_with_threads<3>, "multi-threaded")
{
	auto run = [&]() -> cppcoro::task<>
	{
		cppcoro::async_auto_reset_event event;

		int value = 0;

		auto startWaiter = [&]() -> cppcoro::task<>
		{
			co_await io_service().schedule();
			co_await event;
			++value;
			event.set();
		};

		auto startSignaller = [&]() -> cppcoro::task<>
		{
			co_await io_service().schedule();
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

#endif

TEST_SUITE_END();
