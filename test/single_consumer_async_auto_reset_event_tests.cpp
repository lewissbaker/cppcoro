///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/single_consumer_async_auto_reset_event.hpp>

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

TEST_SUITE_BEGIN("single_consumer_async_auto_reset_event");

TEST_CASE("single waiter")
{
	cppcoro::single_consumer_async_auto_reset_event event;

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

#if CPPCORO_OS_WINNT

TEST_CASE_FIXTURE(io_service_fixture_with_threads<3>, "multi-threaded")
{
	cppcoro::sync_wait([&]() -> cppcoro::task<>
	{
		cppcoro::single_consumer_async_auto_reset_event valueChangedEvent;

		std::atomic<int> value;

		auto consumer = [&]() -> cppcoro::task<int>
		{
			while (value.load(std::memory_order_relaxed) < 10'000)
			{
				co_await valueChangedEvent;
			}

			co_return 0;
		};

		auto modifier = [&](int count) -> cppcoro::task<int>
		{
			co_await io_service().schedule();
			for (int i = 0; i < count; ++i)
			{
				value.fetch_add(1, std::memory_order_relaxed);
				valueChangedEvent.set();
			}
			co_return 0;
		};

		for (int i = 0; i < 1000; ++i)
		{
			value.store(0, std::memory_order_relaxed);

			// Really just checking that we don't deadlock here due to a missed wake-up.
			(void)co_await cppcoro::when_all(consumer(), modifier(5'000), modifier(5'000));
		}
	}());
}

#endif

TEST_SUITE_END();
