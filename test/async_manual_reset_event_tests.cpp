///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/async_manual_reset_event.hpp>

#include <cppcoro/task.hpp>

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

	auto createWaiter = [&]() -> cppcoro::task<>
	{
		co_await event;
	};

	auto t1 = createWaiter();
	auto t2 = createWaiter();

	CHECK(!t1.is_ready());
	CHECK(!t2.is_ready());

	event.reset();

	CHECK(!t1.is_ready());
	CHECK(!t2.is_ready());

	event.set();

	CHECK(t1.is_ready());
	CHECK(t2.is_ready());
}

TEST_CASE("awaiting already set event doesn't suspend")
{
	cppcoro::async_manual_reset_event event{ true };

	auto createWaiter = [&]() -> cppcoro::task<>
	{
		co_await event;
	};

	auto t1 = createWaiter();

	CHECK(t1.is_ready());

	auto t2 = createWaiter();

	CHECK(t2.is_ready());
}

TEST_SUITE_END();
