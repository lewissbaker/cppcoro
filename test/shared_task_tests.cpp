///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/shared_task.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all_ready.hpp>
#include <cppcoro/single_consumer_event.hpp>
#include <cppcoro/fmap.hpp>

#include "counted.hpp"

#include <ostream>
#include <string>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("shared_task");

TEST_CASE("awaiting default-constructed task throws broken_promise")
{
	cppcoro::sync_wait([]() -> cppcoro::task<>
	{
		CHECK_THROWS_AS(co_await cppcoro::shared_task<>{}, const cppcoro::broken_promise&);
	}());
}

TEST_CASE("coroutine doesn't start executing until awaited")
{
	bool startedExecuting = false;
	auto f = [&]() -> cppcoro::shared_task<>
	{
		startedExecuting = true;
		co_return;
	};

	auto t = f();

	CHECK(!t.is_ready());
	CHECK(!startedExecuting);

	cppcoro::sync_wait([](cppcoro::shared_task<> t) -> cppcoro::task<>
	{
		co_await t;
	}(t));

	CHECK(t.is_ready());
	CHECK(startedExecuting);
}

TEST_CASE("result is destroyed when last reference is destroyed")
{
	counted::reset_counts();

	{
		auto t = []() -> cppcoro::shared_task<counted>
		{
			co_return counted{};
		}();

		CHECK(counted::active_count() == 0);

		cppcoro::sync_wait(t);

		CHECK(counted::active_count() == 1);
	}

	CHECK(counted::active_count() == 0);
}

TEST_CASE("multiple awaiters")
{
	cppcoro::single_consumer_event event;
	bool startedExecution = false;
	auto produce = [&]() -> cppcoro::shared_task<int>
	{
		startedExecution = true;
		co_await event;
		co_return 1;
	};

	auto consume = [](cppcoro::shared_task<int> t) -> cppcoro::task<>
	{
		int result = co_await t;
		CHECK(result == 1);
	};

	auto sharedTask = produce();

	cppcoro::sync_wait(cppcoro::when_all_ready(
		consume(sharedTask),
		consume(sharedTask),
		consume(sharedTask),
		[&]() -> cppcoro::task<>
		{
			event.set();
			CHECK(sharedTask.is_ready());
			co_return;
		}()));

	CHECK(sharedTask.is_ready());
}

TEST_CASE("waiting on shared_task in loop doesn't cause stack-overflow")
{
	// This test checks that awaiting a shared_task that completes
	// synchronously doesn't recursively resume the awaiter inside the
	// call to start executing the task. If it were to do this then we'd
	// expect that this test would result in failure due to stack-overflow.

	auto completesSynchronously = []() -> cppcoro::shared_task<int>
	{
		co_return 1;
	};

	cppcoro::sync_wait([&]() -> cppcoro::task<>
	{
		int result = 0;
		for (int i = 0; i < 1'000'000; ++i)
		{
			result += co_await completesSynchronously();
		}
		CHECK(result == 1'000'000);
	}());
}

TEST_CASE("make_shared_task")
{
	bool startedExecution = false;

	auto f = [&]() -> cppcoro::task<std::string>
	{
		startedExecution = false;
		co_return "test";
	};

	auto t = f();

	cppcoro::shared_task<std::string> sharedT =
		cppcoro::make_shared_task(std::move(t));

	CHECK(!sharedT.is_ready());
	CHECK(!startedExecution);

	auto consume = [](cppcoro::shared_task<std::string> t) -> cppcoro::task<>
	{
		auto x = co_await std::move(t);
		CHECK(x == "test");
	};

	cppcoro::sync_wait(cppcoro::when_all_ready(
		consume(sharedT),
		consume(sharedT)));
}

TEST_CASE("make_shared_task of void"
	* doctest::description{ "Tests that workaround for 'co_return <void-expr>' bug is operational if required" })
{
	bool startedExecution = false;

	auto f = [&]() -> cppcoro::task<>
	{
		startedExecution = true;
		co_return;
	};

	auto t = f();

	cppcoro::shared_task<> sharedT = cppcoro::make_shared_task(std::move(t));

	CHECK(!sharedT.is_ready());
	CHECK(!startedExecution);

	auto consume = [](cppcoro::shared_task<> t) -> cppcoro::task<>
	{
		co_await t;
	};

	auto c1 = consume(sharedT);
	cppcoro::sync_wait(c1);

	CHECK(startedExecution);

	auto c2 = consume(sharedT);
	cppcoro::sync_wait(c2);

	CHECK(c1.is_ready());
	CHECK(c2.is_ready());
}

TEST_CASE("shared_task<void> fmap operator")
{
	cppcoro::single_consumer_event event;
	int value = 0;

	auto setNumber = [&]() -> cppcoro::shared_task<>
	{
		co_await event;
		value = 123;
	};

	cppcoro::sync_wait(cppcoro::when_all_ready(
		[&]() -> cppcoro::task<>
	{
		auto numericStringTask =
			setNumber()
			| cppcoro::fmap([&]() { return std::to_string(value); });

		CHECK(co_await numericStringTask == "123");
	}(),
		[&]() -> cppcoro::task<>
	{
		CHECK(value == 0);
		event.set();
		CHECK(value == 123);
		co_return;
	}()));
}

TEST_CASE("shared_task<T> fmap operator")
{
	cppcoro::single_consumer_event event;

	auto getNumber = [&]() -> cppcoro::shared_task<int>
	{
		co_await event;
		co_return 123;
	};

	cppcoro::sync_wait(cppcoro::when_all_ready(
		[&]() -> cppcoro::task<>
	{
		auto numericStringTask =
			getNumber()
			| cppcoro::fmap([](int x) { return std::to_string(x); });

		CHECK(co_await numericStringTask == "123");
	}(),
		[&]() -> cppcoro::task<>
	{
		event.set();
		co_return;
	}()));
}

TEST_SUITE_END();
