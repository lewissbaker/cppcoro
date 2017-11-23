///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/async_mutex.hpp>
#include <cppcoro/single_consumer_event.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all_ready.hpp>
#include <cppcoro/sync_wait.hpp>

#include <ostream>
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

#if 0
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

	auto check = [&]() -> cppcoro::task<>
	{
		CHECK(value == 0);

		a.set();

		CHECK(value == 1);

		auto check2 = [&]() -> cppcoro::task<>
		{
			b.set();

			CHECK(value == 2);

			c.set();

			CHECK(value == 3);

			d.set();

			CHECK(value == 4);

			co_return;
		};

		// Now that we've queued some waiters and released one waiter this will
		// have acquired the list of pending waiters in the local cache.
		// We'll now queue up another one before releasing any more waiters
		// to test the code-path that looks at the newly queued waiter list
		// when the cache of waiters is exhausted.
		(void)co_await cppcoro::when_all_ready(f(d), check2());
	};

	cppcoro::sync_wait(cppcoro::when_all_ready(
		f(a),
		f(b),
		f(c),
		check()));

	CHECK(value == 4);
}
#endif

TEST_SUITE_END();
