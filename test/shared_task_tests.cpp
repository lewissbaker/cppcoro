///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/shared_task.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/single_consumer_event.hpp>

#include "counted.hpp"

#include <string>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("shared_task");

TEST_CASE("default constructed shared_task")
{
	SUBCASE("is_ready")
	{
		cppcoro::shared_task<> t;
		CHECK(t.is_ready());

		cppcoro::shared_task<> tCopy = t;
		CHECK(t.is_ready());
	}

	SUBCASE("awaiting throws broken_promise")
	{
		auto task = []() -> cppcoro::task<>
		{
			CHECK_THROWS_AS(co_await cppcoro::shared_task<>{}, const cppcoro::broken_promise&);
		}();

		CHECK(task.is_ready());
	}
}

TEST_CASE("multiple waiters")
{
	cppcoro::single_consumer_event event;

	auto sharedTask = [](cppcoro::single_consumer_event& event) -> cppcoro::shared_task<>
	{
		co_await event;
	}(event);

	CHECK(!sharedTask.is_ready());

	auto consumeTask = [](cppcoro::shared_task<> task) -> cppcoro::task<>
	{
		co_await task;
	};

	auto t1 = consumeTask(sharedTask);
	auto t2 = consumeTask(sharedTask);

	CHECK(!t1.is_ready());
	CHECK(!t2.is_ready());

	event.set();

	CHECK(sharedTask.is_ready());
	CHECK(t1.is_ready());
	CHECK(t2.is_ready());

	auto t3 = consumeTask(sharedTask);

	CHECK(t3.is_ready());
}

TEST_CASE("unhandled exception is rethrown")
{
	class X {};

	auto throwingTask = []() -> cppcoro::shared_task<>
	{
		co_await std::experimental::suspend_never{};
		throw X{};
	};

	[&]() -> cppcoro::task<>
	{
		auto t = throwingTask();
		CHECK(t.is_ready());
		CHECK_THROWS_AS(co_await t, const X&);
	}();
}

TEST_CASE("result is destroyed when last reference is destroyed")
{
	counted::reset_counts();

	{
		cppcoro::shared_task<counted> tCopy;

		{
			auto t = []() -> cppcoro::shared_task<counted>
			{
				co_return counted{};
			}();

			CHECK(t.is_ready());

			tCopy = t;

			CHECK(tCopy.is_ready());
		}

		{
			cppcoro::shared_task<counted> tCopy2 = tCopy;

			CHECK(tCopy2.is_ready());
		}

		CHECK(counted::active_count() == 1);
	}

	CHECK(counted::active_count() == 0);
}

TEST_CASE("assigning result from shared_task doesn't move result")
{
	auto f = []() -> cppcoro::shared_task<std::string>
	{
		co_return "string that is longer than short-string optimisation";
	};

	auto t = f();

	auto g = [](cppcoro::shared_task<std::string> t) -> cppcoro::task<>
	{
		auto x = co_await t;
		CHECK(x == "string that is longer than short-string optimisation");

		auto y = co_await std::move(t);
		CHECK(y == "string that is longer than short-string optimisation");
	};

	g(t);
	g(t);
}

TEST_CASE("shared_task of reference type")
{
	const std::string value = "some string value";

	auto f = [&]() -> cppcoro::shared_task<const std::string&>
	{
		co_return value;
	};

	[&]() -> cppcoro::task<>
	{
		auto& result = co_await f();
		CHECK(&result == &value);
	}();
}

TEST_CASE("shared_task returning rvalue reference moves into promise")
{
	counted::reset_counts();

	auto f = []() -> cppcoro::shared_task<counted>
	{
		co_return counted{};
	};

	CHECK(counted::active_count() == 0);

	{
		auto t = f();
		CHECK(counted::default_construction_count == 1);
		CHECK(counted::copy_construction_count == 0);
		CHECK(counted::move_construction_count == 1);
		CHECK(counted::destruction_count == 1);
		CHECK(counted::active_count() == 1);

		// Moving task doesn't move/copy result.
		auto t2 = std::move(t);
		CHECK(counted::default_construction_count == 1);
		CHECK(counted::copy_construction_count == 0);
		CHECK(counted::move_construction_count == 1);
		CHECK(counted::destruction_count == 1);
		CHECK(counted::active_count() == 1);
	}

	CHECK(counted::active_count() == 0);
}

TEST_CASE("equality comparison")
{
	auto f = []() -> cppcoro::shared_task<>
	{
		co_return;
	};

	cppcoro::shared_task<> t0;
	cppcoro::shared_task<> t1 = t0;
	cppcoro::shared_task<> t2 = f();
	cppcoro::shared_task<> t3 = t2;
	cppcoro::shared_task<> t4 = f();
	CHECK(t0 == t0);
	CHECK(t0 == t1);
	CHECK(t0 != t2);
	CHECK(t0 != t3);
	CHECK(t0 != t4);
	CHECK(t2 == t2);
	CHECK(t2 == t3);
	CHECK(t2 != t4);
}

TEST_CASE("make_shared_task")
{
	cppcoro::single_consumer_event event;

	auto f = [&]() -> cppcoro::task<std::string>
	{
		co_await event;
		co_return "foo";
	};

	auto t = cppcoro::make_shared_task(f());

	auto consumer = [](cppcoro::shared_task<std::string> task) -> cppcoro::task<>
	{
		CHECK(co_await task == "foo");
	};

	auto consumerTask0 = consumer(t);
	auto consumerTask1 = consumer(t);

	CHECK(!consumerTask0.is_ready());
	CHECK(!consumerTask1.is_ready());

	event.set();

	CHECK(consumerTask0.is_ready());
	CHECK(consumerTask1.is_ready());
}

TEST_CASE("make_shared_task of void-returning task"
          * doctest::description{"checks that workaround for MSVC bug is getting picked up."
                                 "MSVC 2017.1 was failing to evaluate <expr> in 'co_return <expr>' if expression was void."})
{
	cppcoro::single_consumer_event event;

	auto f = [&]() -> cppcoro::task<>
	{
		co_await event;
	};

	auto t = cppcoro::make_shared_task(f());

	CHECK(!t.is_ready());

	auto consumer = [](cppcoro::shared_task<> task) -> cppcoro::task<>
	{
		co_await task;
	};

	auto consumerTask0 = consumer(t);
	auto consumerTask1 = consumer(t);

	CHECK(!consumerTask0.is_ready());
	CHECK(!consumerTask1.is_ready());

	event.set();

	CHECK(consumerTask0.is_ready());
	CHECK(consumerTask1.is_ready());
}

TEST_SUITE_END();
