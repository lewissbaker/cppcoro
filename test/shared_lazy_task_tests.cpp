///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/shared_lazy_task.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/single_consumer_event.hpp>

#include "counted.hpp"

#include <string>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("shared_lazy_task");

TEST_CASE("awaiting default-constructed task throws broken_promise")
{
	auto t = []() -> cppcoro::task<>
	{
		CHECK_THROWS_AS(co_await cppcoro::shared_lazy_task<>{}, const cppcoro::broken_promise&);
	}();

	CHECK(t.is_ready());
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

	auto waiter = [](cppcoro::shared_lazy_task<> t) -> cppcoro::task<>
	{
		co_await t;
	}(t);

	CHECK(waiter.is_ready());
	CHECK(t.is_ready());
	CHECK(startedExecuting);
}

static constexpr bool is_msvc_2015_x86_optimised =
#if defined(_MSC_VER) && _MSC_VER < 1910 && defined(CPPCORO_RELEASE_OPTIMISED)
	true;
#else
	false;
#endif

// Skip running under MSVC 2015 x86 opt due to compiler bug which causes this test
// to crash with an access violation inside shared_lazy_task_promise_base::try_await().
TEST_CASE("result is destroyed when last reference is destroyed"
	* doctest::skip{ is_msvc_2015_x86_optimised })
{
	counted::reset_counts();

	{
		auto t = []() -> cppcoro::shared_lazy_task<counted>
		{
			co_return counted{};
		}();

		CHECK(counted::active_count() == 0);

		[](cppcoro::shared_lazy_task<counted> t) -> cppcoro::task<>
		{
			co_await t;
		}(t);

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

	auto consume = [](cppcoro::shared_lazy_task<int> t) -> cppcoro::task<>
	{
		int result = co_await t;
		CHECK(result == 1);
	};

	auto sharedTask = produce();

	CHECK(!sharedTask.is_ready());
	CHECK(!startedExecution);

	auto t1 = consume(sharedTask);

	CHECK(!t1.is_ready());
	CHECK(startedExecution);
	CHECK(!sharedTask.is_ready());

	auto t2 = consume(sharedTask);

	CHECK(!t2.is_ready());
	CHECK(!sharedTask.is_ready());

	auto t3 = consume(sharedTask);

	event.set();

	CHECK(sharedTask.is_ready());
	CHECK(t1.is_ready());
	CHECK(t2.is_ready());
	CHECK(t3.is_ready());
}

// Skip running under MSVC 2015 x86 opt due to compiler bug which causes this test
// to crash with an access violation inside shared_lazy_task_promise_base::try_await().
TEST_CASE("waiting on shared_lazy_task in loop doesn't cause stack-overflow"
	* doctest::skip{ is_msvc_2015_x86_optimised })
{
	// This test checks that awaiting a shared_lazy_task that completes
	// synchronously doesn't recursively resume the awaiter inside the
	// call to start executing the task. If it were to do this then we'd
	// expect that this test would result in failure due to stack-overflow.

	auto completesSynchronously = []() -> cppcoro::shared_lazy_task<int>
	{
		co_return 1;
	};

	auto run = [&]() -> cppcoro::task<>
	{
		int result = 0;
		for (int i = 0; i < 1'000'000; ++i)
		{
			result += co_await completesSynchronously();
		}
		CHECK(result == 1'000'000);
	};

	auto t = run();
	CHECK(t.is_ready());
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

	auto consume = [](cppcoro::shared_lazy_task<std::string> t) -> cppcoro::task<>
	{
		auto x = co_await std::move(t);
		CHECK(x == "test");
	};

	auto c1 = consume(sharedT);
	auto c2 = consume(sharedT);

	CHECK(c1.is_ready());
	CHECK(c2.is_ready());
}

TEST_CASE("make_shared_task of void"
	* doctest::description{ "Tests that workaround for MSVC 2017.1 bug is operational" })
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

	auto consume = [](cppcoro::shared_lazy_task<> t) -> cppcoro::task<>
	{
		co_await t;
	};

	auto c1 = consume(sharedT);
	CHECK(startedExecution);

	auto c2 = consume(sharedT);

	CHECK(c1.is_ready());
	CHECK(c2.is_ready());
}

TEST_SUITE_END();
