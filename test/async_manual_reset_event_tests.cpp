///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/async_manual_reset_event.hpp>

#include <cppcoro/task.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all_ready.hpp>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("async_manual_reset_event");

TEST_CASE("default constructor initially not set")
{
	cppcoro::async_manual_reset_event event;
	CHECK(!event.is_set());
}

TEST_CASE("construct event initially set")
{
	cppcoro::async_manual_reset_event event{ true };
	CHECK(event.is_set());
}

TEST_CASE("set and reset")
{
	cppcoro::async_manual_reset_event event;
	CHECK(!event.is_set());
	event.set();
	CHECK(event.is_set());
	event.set();
	CHECK(event.is_set());
	event.reset();
	CHECK(!event.is_set());
	event.reset();
	CHECK(!event.is_set());
	event.set();
	CHECK(event.is_set());
}

TEST_CASE("await not set event")
{
	cppcoro::async_manual_reset_event event;

	auto createWaiter = [&](bool& flag) -> cppcoro::task<>
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

		event.reset();

		CHECK(!completed1);
		CHECK(!completed2);

		event.set();

		CHECK(completed1);
		CHECK(completed2);

		co_return;
	};

	cppcoro::sync_wait(cppcoro::when_all_ready(
		createWaiter(completed1),
		createWaiter(completed2),
		check()));
}

TEST_CASE("awaiting already set event doesn't suspend")
{
	cppcoro::async_manual_reset_event event{ true };

	auto createWaiter = [&]() -> cppcoro::task<>
	{
		co_await event;
	};

	// Should complete without blocking.
	cppcoro::sync_wait(cppcoro::when_all_ready(
		createWaiter(),
		createWaiter()));
}

TEST_SUITE_END();
