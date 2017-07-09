///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/task.hpp>
#include <cppcoro/single_consumer_event.hpp>

#include "counted.hpp"

#include <string>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("task");

TEST_CASE("default constructed task")
{
	cppcoro::task<> t;

	CHECK(t.is_ready());

	SUBCASE("throws broken_promise when awaited")
	{
		auto op = t.operator co_await();
		CHECK(op.await_ready());
		CHECK_THROWS_AS(op.await_resume(), const cppcoro::broken_promise&);
	}
}

TEST_CASE("co_await synchronously completing task")
{
	auto doNothingAsync = []() -> cppcoro::task<>
	{
		co_return;
	};

	auto task = doNothingAsync();

	CHECK(task.is_ready());

	bool ok = false;
	auto test = [&]() -> cppcoro::task<>
	{
		co_await task;
		ok = true;
	};

	test();

	CHECK(ok);
}

TEST_CASE("task of move-only type by value")
{
	// unique_ptr is move-only type.
	auto getIntPtrAsync = []() -> cppcoro::task<std::unique_ptr<int>>
	{
		co_return std::make_unique<int>(123);
	};

	SUBCASE("co_await temporary")
	{
		auto test = [&]() -> cppcoro::task<>
		{
			auto intPtr = co_await getIntPtrAsync();
			REQUIRE(intPtr);
			CHECK(*intPtr == 123);
		};

		test();
	}

	SUBCASE("co_await lvalue reference")
	{
		auto test = [&]() -> cppcoro::task<>
		{
			// co_await yields l-value reference if task is l-value
			auto intPtrTask = getIntPtrAsync();
			auto& intPtr = co_await intPtrTask;
			REQUIRE(intPtr);
			CHECK(*intPtr == 123);
		};

		test();
	}

	SUBCASE("co_await rvalue reference")
	{
		auto test = [&]() -> cppcoro::task<>
		{
			// Returns r-value reference if task is r-value
			auto intPtrTask = getIntPtrAsync();
			auto intPtr = co_await std::move(intPtrTask);
			REQUIRE(intPtr);
			CHECK(*intPtr == 123);
		};

		test();
	}
}

TEST_CASE("task of reference type")
{
	int value = 0;
	auto getRefAsync = [&]() -> cppcoro::task<int&>
	{
		co_return value;
	};

	auto test = [&]() -> cppcoro::task<>
	{
		// Await r-value task results in l-value reference
		decltype(auto) result = co_await getRefAsync();
		CHECK(&result == &value);

		// Await l-value task results in l-value reference
		auto getRefTask = getRefAsync();
		decltype(auto) result2 = co_await getRefTask;
		CHECK(&result2 == &value);
	};

	auto task = test();
	CHECK(task.is_ready());
}

TEST_CASE("task of value-type moves into promise if passed rvalue reference")
{
	counted::reset_counts();

	auto f = []() -> cppcoro::task<counted>
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

TEST_CASE("task of value-type copies into promise if passed lvalue reference")
{
	counted::reset_counts();

	auto f = []() -> cppcoro::task<counted>
	{
		counted temp;

		// Should be calling copy-constructor here since <promise>.return_value()
		// is being passed an l-value reference.
		co_return temp;
	};

	CHECK(counted::active_count() == 0);

	{
		auto t = f();
		CHECK(counted::default_construction_count == 1);
		CHECK(counted::copy_construction_count == 1);
		CHECK(counted::move_construction_count == 0);
		CHECK(counted::destruction_count == 1);
		CHECK(counted::active_count() == 1);

		// Moving the task doesn't move/copy the result
		auto t2 = std::move(t);
		CHECK(counted::default_construction_count == 1);
		CHECK(counted::copy_construction_count == 1);
		CHECK(counted::move_construction_count == 0);
		CHECK(counted::destruction_count == 1);
		CHECK(counted::active_count() == 1);
	}

	CHECK(counted::active_count() == 0);
}

TEST_CASE("co_await chain of async completions")
{
	cppcoro::single_consumer_event event;
	bool reachedPointA = false;
	bool reachedPointB = false;
	auto async1 = [&]() -> cppcoro::task<int>
	{
		reachedPointA = true;
		co_await event;
		reachedPointB = true;
		co_return 1;
	};

	bool reachedPointC = false;
	bool reachedPointD = false;
	auto async2 = [&]() -> cppcoro::task<int>
	{
		reachedPointC = true;
		int result = co_await async1();
		reachedPointD = true;
		co_return result;
	};

	auto task = async2();

	CHECK(!task.is_ready());
	CHECK(reachedPointA);
	CHECK(!reachedPointB);
	CHECK(reachedPointC);
	CHECK(!reachedPointD);

	event.set();

	CHECK(task.is_ready());
	CHECK(reachedPointB);
	CHECK(reachedPointD);

	[](cppcoro::task<int> t) -> cppcoro::task<>
	{
		int value = co_await t;
		CHECK(value == 1);
	}(std::move(task));
}

TEST_CASE("awaiting default-constructed task throws broken_promise")
{
	[]() -> cppcoro::task<>
	{
		cppcoro::task<> broken;
		CHECK_THROWS_AS(co_await broken, const cppcoro::broken_promise&);
	}();
}

TEST_CASE("awaiting task that completes with exception")
{
	class X {};

	auto run = [](bool doThrow = true) -> cppcoro::task<>
	{
		if (doThrow) throw X{};
		co_return;
	};

	auto t = run();
	CHECK(t.is_ready());

	auto consumeT = [&]() -> cppcoro::task<>
	{
		SUBCASE("co_await task rethrows exception")
		{
			CHECK_THROWS_AS(co_await t, const X&);
		}

		SUBCASE("co_await task.when_ready() doesn't rethrow exception")
		{
			CHECK_NOTHROW(co_await t.when_ready());
		}
	};

	auto consumer = consumeT();
	CHECK(consumer.is_ready());
}

TEST_CASE("task<void> fmap pipe operator")
{
	using cppcoro::fmap;

	cppcoro::single_consumer_event event;

	auto f = [&]() -> cppcoro::task<>
	{
		co_await event;
		co_return;
	};

	auto t = f() | fmap([] { return 123; });

	CHECK(!t.is_ready());

	event.set();

	REQUIRE(t.is_ready());

	[&]() -> cppcoro::task<>
	{
		CHECK(co_await t == 123);
	}();
}

TEST_CASE("task<int> fmap pipe operator")
{
	using cppcoro::task;
	using cppcoro::fmap;

	cppcoro::single_consumer_event event;

	auto one = [&]() -> task<int>
	{
		co_await event;
		co_return 1;
	};

	SUBCASE("r-value fmap / r-value lambda")
	{
		task<int> t = one() | fmap([delta=1](auto i) { return i + delta; });
		CHECK(!t.is_ready());
		event.set();
		CHECK(t.is_ready());
		[&]() -> task<>
		{
			CHECK(co_await t == 2);
		}();
	}

	event.reset();

	SUBCASE("r-value fmap / l-value lambda")
	{
		using namespace std::string_literals;

		task<std::string> t;

		{
			auto f = [prefix = "pfx"s](int x)
			{
				return prefix + std::to_string(x);
			};

			// Want to make sure that the resulting task has taken
			// a copy of the lambda passed to fmap().
			t = one() | fmap(f);
		}

		CHECK(!t.is_ready());

		event.set();

		REQUIRE(t.is_ready());

		[&]() -> task<>
		{
			CHECK(co_await t == "pfx1");
		}();
	}

	event.reset();

	SUBCASE("l-value fmap / r-value lambda")
	{
		using namespace std::string_literals;

		task<std::string> t;

		{
			auto addprefix = fmap([prefix = "a really really long prefix that prevents small string optimisation"s](int x)
			{
				return prefix + std::to_string(x);
			});

			// Want to make sure that the resulting task has taken
			// a copy of the lambda passed to fmap().
			t = one() | addprefix;
		}

		CHECK(!t.is_ready());

		event.set();

		REQUIRE(t.is_ready());

		[&]() -> task<>
		{
			CHECK(co_await t == "a really really long prefix that prevents small string optimisation1");
		}();
	}

	event.reset();

	SUBCASE("l-value fmap / l-value lambda")
	{
		using namespace std::string_literals;
		
		task<std::string> t;

		{
			auto lambda = [prefix = "a really really long prefix that prevents small string optimisation"s](int x)
			{
				return prefix + std::to_string(x);
			};

			auto addprefix = fmap(lambda);

			// Want to make sure that the resulting task has taken
			// a copy of the lambda passed to fmap().
			t = one() | addprefix;
		}

		CHECK(!t.is_ready());

		event.set();

		REQUIRE(t.is_ready());

		[&]() -> task<>
		{
			CHECK(co_await t == "a really really long prefix that prevents small string optimisation1");
		}();
	}
}

TEST_CASE("chained fmap pipe operations")
{
	using namespace std::string_literals;
	using cppcoro::task;

	auto prepend = [](std::string s)
	{
		using cppcoro::fmap;
		return fmap([s = std::move(s)](const std::string& value) { return s + value; });
	};

	auto append = [](std::string s)
	{
		using cppcoro::fmap;
		return fmap([s = std::move(s)](const std::string& value){ return value + s; });
	};

	auto asyncString = [](std::string s) -> task<std::string>
	{
		co_return std::move(s);
	};

	auto t = asyncString("base"s) | prepend("pre_"s) | append("_post"s);

	REQUIRE(t.is_ready());

	[&]() -> task<>
	{
		CHECK(co_await t == "pre_base_post");
	}();
}

TEST_SUITE_END();
