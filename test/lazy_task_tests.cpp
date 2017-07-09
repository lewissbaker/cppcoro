///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/lazy_task.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/single_consumer_event.hpp>

#include "counted.hpp"

#include <string>
#include <type_traits>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("lazy_task");

TEST_CASE("lazy_task doesn't start until awaited")
{
	bool started = false;
	auto func = [&]() -> cppcoro::lazy_task<>
	{
		started = true;
		co_return;
	};

	auto t = func();
	CHECK(!started);

	auto consumer = [&]() -> cppcoro::task<>
	{
		co_await t;
	};

	auto consumerTask = consumer();

	CHECK(started);
}

TEST_CASE("awaiting default-constructed lazy_task throws broken_promise")
{
	[&]() -> cppcoro::task<>
	{
		cppcoro::lazy_task<> t;
		CHECK_THROWS_AS(co_await t, const cppcoro::broken_promise&);
	}();
}

TEST_CASE("awaiting lazy_task that completes asynchronously")
{
	bool reachedBeforeEvent = false;
	bool reachedAfterEvent = false;
	cppcoro::single_consumer_event event;
	auto f = [&]() -> cppcoro::lazy_task<>
	{
		reachedBeforeEvent = true;
		co_await event;
		reachedAfterEvent = true;
	};

	auto t = f();

	CHECK(!t.is_ready());
	CHECK(!reachedBeforeEvent);

	auto t2 = [](cppcoro::lazy_task<>& t) -> cppcoro::task<>
	{
		co_await t;
	}(t);

	CHECK(!t2.is_ready());

	event.set();

	CHECK(t.is_ready());
	CHECK(t2.is_ready());
	CHECK(reachedAfterEvent);
}

TEST_CASE("destroying lazy_task that was never awaited destroys captured args")
{
	counted::reset_counts();

	auto f = [](counted c) -> cppcoro::lazy_task<counted>
	{
		co_return c;
	};

	CHECK(counted::active_count() == 0);

	{
		auto t = f(counted{});
		CHECK(counted::active_count() == 1);
	}

	CHECK(counted::active_count() == 0);
}

TEST_CASE("lazy_task destructor destroys result")
{
	counted::reset_counts();

	auto f = []() -> cppcoro::lazy_task<counted>
	{
		co_return counted{};
	};

	{
		auto t = f();
		CHECK(counted::active_count() == 0);

		[](cppcoro::lazy_task<counted>& t) -> cppcoro::task<>
		{
			co_await t;
			CHECK(t.is_ready());
			CHECK(counted::active_count() == 1);
		}(t);

		CHECK(counted::active_count() == 1);
	}

	CHECK(counted::active_count() == 0);
}

TEST_CASE("lazy_task of reference type")
{
	int value = 3;
	auto f = [&]() -> cppcoro::lazy_task<int&>
	{
		co_return value;
	};

	auto g = [&]() -> cppcoro::task<>
	{
		SUBCASE("awaiting rvalue task")
		{
			decltype(auto) result = co_await f();
			static_assert(
				std::is_same<decltype(result), int&>::value,
				"co_await r-value reference of lazy_task<int&> should result in an int&");
			CHECK(&result == &value);
		}

		SUBCASE("awaiting lvalue task")
		{
			auto t = f();
			decltype(auto) result = co_await t;
			static_assert(
				std::is_same<decltype(result), int&>::value,
				"co_await l-value reference of lazy_task<int&> should result in an int&");
			CHECK(&result == &value);
		}
	};

	auto t = g();
	CHECK(t.is_ready());
}

TEST_CASE("passing parameter by value to lazy_task coroutine calls move-constructor exactly once")
{
	counted::reset_counts();

	auto f = [](counted arg) -> cppcoro::lazy_task<>
	{
		co_return;
	};

	counted c;

	CHECK(counted::active_count() == 1);
	CHECK(counted::default_construction_count == 1);
	CHECK(counted::copy_construction_count == 0);
	CHECK(counted::move_construction_count == 0);
	CHECK(counted::destruction_count == 0);

	{
		auto t = f(c);

		// Should have called copy-constructor to pass a copy of 'c' into f by value.
		CHECK(counted::copy_construction_count == 1);

		// Inside f it should have move-constructed parameter into coroutine frame variable
		WARN_MESSAGE(counted::move_construction_count == 1,
			"Known bug in MSVC 2017.1, not critical if it performs multiple moves");

		// Active counts should be the instance 'c' and the instance captured in coroutine frame of 't'.
		CHECK(counted::active_count() == 2);
	}

	CHECK(counted::active_count() == 1);
}

TEST_CASE("lazy_task<void> fmap pipe operator")
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

TEST_CASE("lazy_task<int> fmap pipe operator")
{
	using cppcoro::task;
	using cppcoro::lazy_task;
	using cppcoro::fmap;

	auto one = [&]() -> lazy_task<int>
	{
		co_return 1;
	};

	SUBCASE("r-value fmap / r-value lambda")
	{
		lazy_task<int> t = one() | fmap([delta = 1](auto i) { return i + delta; });
		CHECK(!t.is_ready());
		[&]() -> task<>
		{
			CHECK(co_await t == 2);
		}();
	}

	SUBCASE("r-value fmap / l-value lambda")
	{
		using namespace std::string_literals;

		lazy_task<std::string> t;

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

		[&]() -> task<>
		{
			CHECK(co_await t == "pfx1");
		}();
	}

	SUBCASE("l-value fmap / r-value lambda")
	{
		using namespace std::string_literals;

		lazy_task<std::string> t;

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

		[&]() -> task<>
		{
			CHECK(co_await t == "a really really long prefix that prevents small string optimisation1");
		}();
	}

	SUBCASE("l-value fmap / l-value lambda")
	{
		using namespace std::string_literals;

		lazy_task<std::string> t;

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
	using cppcoro::lazy_task;

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

	auto asyncString = [](std::string s) -> lazy_task<std::string>
	{
		co_return std::move(s);
	};

	auto t = asyncString("base"s) | prepend("pre_"s) | append("_post"s);

	CHECK(!t.is_ready());

	[&]() -> task<>
	{
		CHECK(co_await t == "pre_base_post");
	}();
}

TEST_SUITE_END();
