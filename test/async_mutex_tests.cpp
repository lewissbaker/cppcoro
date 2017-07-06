///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/async_mutex.hpp>
#include <cppcoro/single_consumer_event.hpp>
#include <cppcoro/task.hpp>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("async_mutex");

TEST_CASE("try_lock")
{
	cppcoro::async_mutex mutex;

	CHECK(mutex.try_lock());

	CHECK_FALSE(mutex.try_lock());

	mutex.unlock();

	CHECK(mutex.try_lock());
}

TEST_CASE("multiple lockers")
{
	int value = 0;
	cppcoro::async_mutex mutex;
	cppcoro::single_consumer_event a;
	cppcoro::single_consumer_event b;
	cppcoro::single_consumer_event c;
	cppcoro::single_consumer_event d;

	auto f = [&](cppcoro::single_consumer_event& e) -> cppcoro::task<>
	{
		auto lock = co_await mutex.scoped_lock_async();
		co_await e;
		++value;
	};

	auto t1 = f(a);
	CHECK(!t1.is_ready());
	CHECK(value == 0);

	auto t2 = f(b);
	auto t3 = f(c);

	a.set();

	CHECK(value == 1);

	auto t4 = f(d);

	b.set();

	CHECK(value == 2);

	c.set();

	CHECK(value == 3);

	d.set();

	CHECK(value == 4);

	CHECK(t1.is_ready());
	CHECK(t2.is_ready());
	CHECK(t3.is_ready());
	CHECK(t4.is_ready());
}

TEST_SUITE_END();
