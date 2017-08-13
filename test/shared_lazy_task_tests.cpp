///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/shared_lazy_task.hpp>
#include <cppcoro/lazy_task.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all_ready.hpp>
#include <cppcoro/single_consumer_event.hpp>

#include "counted.hpp"

#include <string>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("shared_lazy_task");

TEST_CASE("awaiting default-constructed task throws broken_promise")
{
	cppcoro::sync_wait([]() -> cppcoro::lazy_task<>
	{
		CHECK_THROWS_AS(co_await cppcoro::shared_lazy_task<>{}, const cppcoro::broken_promise&);
	}());
}

TEST_CASE("coroutine doesn't start executing until awaited")
{
	bool startedExecuting = false;
	auto f = [&]() -> cppcoro::shared_lazy_task<>
	{
		startedExecuting = true;
		co_return;
	};

	auto t = f();

	CHECK(!t.is_ready());
	CHECK(!startedExecuting);

	cppcoro::sync_wait([](cppcoro::shared_lazy_task<> t) -> cppcoro::lazy_task<>
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
		auto t = []() -> cppcoro::shared_lazy_task<counted>
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
	auto produce = [&]() -> cppcoro::shared_lazy_task<int>
	{
		startedExecution = true;
		co_await event;
		co_return 1;
	};

	auto consume = [](cppcoro::shared_lazy_task<int> t) -> cppcoro::lazy_task<>
	{
		int result = co_await t;
		CHECK(result == 1);
	};

	auto sharedTask = produce();

	cppcoro::sync_wait(cppcoro::when_all_ready(
		consume(sharedTask),
		consume(sharedTask),
		consume(sharedTask),
		[&]() -> cppcoro::lazy_task<>
		{
			event.set();
			CHECK(sharedTask.is_ready());
			co_return;
		}()));

	CHECK(sharedTask.is_ready());
}

TEST_CASE("waiting on shared_lazy_task in loop doesn't cause stack-overflow")
{
	// This test checks that awaiting a shared_lazy_task that completes
	// synchronously doesn't recursively resume the awaiter inside the
	// call to start executing the task. If it were to do this then we'd
	// expect that this test would result in failure due to stack-overflow.

	auto completesSynchronously = []() -> cppcoro::shared_lazy_task<int>
	{
		co_return 1;
	};

	cppcoro::sync_wait([&]() -> cppcoro::lazy_task<>
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

	auto f = [&]() -> cppcoro::lazy_task<std::string>
	{
		startedExecution = false;
		co_return "test";
	};

	auto t = f();

	cppcoro::shared_lazy_task<std::string> sharedT =
		cppcoro::make_shared_task(std::move(t));

	CHECK(!sharedT.is_ready());
	CHECK(!startedExecution);

	auto consume = [](cppcoro::shared_lazy_task<std::string> t) -> cppcoro::lazy_task<>
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

	auto f = [&]() -> cppcoro::lazy_task<>
	{
		startedExecution = true;
		co_return;
	};

	auto t = f();

	cppcoro::shared_lazy_task<> sharedT = cppcoro::make_shared_task(std::move(t));

	CHECK(!sharedT.is_ready());
	CHECK(!startedExecution);

	auto consume = [](cppcoro::shared_lazy_task<> t) -> cppcoro::lazy_task<>
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

TEST_SUITE_END();
