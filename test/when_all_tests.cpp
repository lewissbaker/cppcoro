///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/when_all.hpp>

#include <cppcoro/task.hpp>
#include <cppcoro/lazy_task.hpp>
#include <cppcoro/shared_task.hpp>
#include <cppcoro/shared_lazy_task.hpp>
#include <cppcoro/async_manual_reset_event.hpp>

#include "counted.hpp"

#include <functional>
#include <string>
#include <vector>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("when_all");

template<template<typename T> class TASK, typename T>
TASK<T> when_event_set_return(cppcoro::async_manual_reset_event& event, T value)
{
	co_await event;
	co_return std::move(value);
}

template<typename T>
cppcoro::task<T> start(cppcoro::lazy_task<T> t)
{
	co_return co_await std::move(t);
}

TEST_CASE("when_all() with no args completes immediately")
{
	CHECK([]() -> cppcoro::task<>
	{
		std::tuple<> result = co_await cppcoro::when_all();
	}().is_ready());
}

TEST_CASE("when_all() with one arg")
{
	bool started = false;
	auto f = [&](cppcoro::async_manual_reset_event& event) -> cppcoro::lazy_task<std::string>
	{
		started = true;
		co_await event;
		co_return "foo";
	};

	cppcoro::async_manual_reset_event event;

	auto whenAllTask = cppcoro::when_all(f(event));
	CHECK(!started);

	auto g = [&]() -> cppcoro::task<>
	{
		auto[s] = co_await whenAllTask;
		CHECK(s == "foo");
	};

	auto whenAllAwaiterTask = g();

	CHECK(started);
	CHECK(!whenAllAwaiterTask.is_ready());

	event.set();

	CHECK(whenAllAwaiterTask.is_ready());
}

TEST_CASE("when_all() with all task types")
{
	counted::reset_counts();

	auto run = [](cppcoro::async_manual_reset_event& event) -> cppcoro::task<>
	{
		using namespace std::string_literals;

		auto[a, b, c, d] = co_await cppcoro::when_all(
			when_event_set_return<cppcoro::task>(event, "foo"s),
			when_event_set_return<cppcoro::lazy_task>(event, 123),
			when_event_set_return<cppcoro::shared_task>(event, 1.0f),
			when_event_set_return<cppcoro::shared_lazy_task>(event, counted{}));

		CHECK(a == "foo");
		CHECK(b == 123);
		CHECK(c == 1.0f);
		CHECK(d.id == 0);
		CHECK(counted::active_count() == 1);
	};

	cppcoro::async_manual_reset_event event;
	auto t = run(event);

	CHECK(!t.is_ready());

	event.set();

	CHECK(t.is_ready());
}

TEST_CASE("when_all() throws if any task throws")
{
	struct X {};
	struct Y {};

	int startedCount = 0;
	auto makeTask = [&](int value) -> cppcoro::lazy_task<int>
	{
		++startedCount;
		if (value == 0) throw X{};
		else if (value == 1) throw Y{};
		else co_return value;
	};

	CHECK([&]() -> cppcoro::task<>
	{
		try
		{
			// This could either throw X or Y exception.
			// The exact exception that is thrown is not defined if multiple tasks throw an exception.
			// TODO: Consider throwing some kind of aggregate_exception that collects all of the exceptions together.
			co_await cppcoro::when_all(makeTask(0), makeTask(1), makeTask(2));
		}
		catch (const X&)
		{
		}
		catch (const Y&)
		{
		}
	}().is_ready());
}

TEST_CASE("when_all() with vector<task<>>")
{
	auto makeTask = [](cppcoro::async_manual_reset_event& event) -> cppcoro::task<>
	{
		co_await event;
	};

	cppcoro::async_manual_reset_event event1;
	cppcoro::async_manual_reset_event event2;

	auto run = [&]() -> cppcoro::task<>
	{
		std::vector<cppcoro::task<>> tasks;
		tasks.push_back(makeTask(event1));
		tasks.push_back(makeTask(event2));
		tasks.push_back(makeTask(event1));

		co_await cppcoro::when_all(std::move(tasks));
	};

	auto t = run();

	CHECK(!t.is_ready());

	event1.set();

	CHECK(!t.is_ready());

	event2.set();

	CHECK(t.is_ready());
}

TEST_CASE("when_all() with vector<lazy_task<>>")
{
	int startedCount = 0;
	auto makeTask = [&](cppcoro::async_manual_reset_event& event) -> cppcoro::lazy_task<>
	{
		++startedCount;
		co_await event;
	};

	cppcoro::async_manual_reset_event event1;
	cppcoro::async_manual_reset_event event2;

	auto run = [&]() -> cppcoro::task<>
	{
		std::vector<cppcoro::lazy_task<>> tasks;
		tasks.push_back(makeTask(event1));
		tasks.push_back(makeTask(event2));
		tasks.push_back(makeTask(event1));

		auto allTask = cppcoro::when_all(std::move(tasks));

		CHECK(startedCount == 0);

		auto startedAllTask = start(std::move(allTask));

		CHECK(startedCount == 3);

		co_await startedAllTask;
	};

	auto t = run();

	CHECK(!t.is_ready());

	event1.set();

	CHECK(!t.is_ready());

	event2.set();

	CHECK(t.is_ready());
}

TEST_CASE("when_all() with vector<shared_task<>>")
{
	auto makeTask = [](cppcoro::async_manual_reset_event& event) -> cppcoro::shared_task<>
	{
		co_await event;
	};

	cppcoro::async_manual_reset_event event1;
	cppcoro::async_manual_reset_event event2;

	auto run = [&]() -> cppcoro::task<>
	{
		std::vector<cppcoro::shared_task<>> tasks;
		tasks.push_back(makeTask(event1));
		tasks.push_back(makeTask(event2));
		tasks.push_back(makeTask(event1));

		co_await cppcoro::when_all(std::move(tasks));
	};

	auto t = run();

	CHECK(!t.is_ready());

	event1.set();

	CHECK(!t.is_ready());

	event2.set();

	CHECK(t.is_ready());
}

TEST_CASE("when_all() with vector<shared_lazy_task<>>")
{
	int startedCount = 0;
	auto makeTask = [&](cppcoro::async_manual_reset_event& event) -> cppcoro::shared_lazy_task<>
	{
		++startedCount;
		co_await event;
	};

	cppcoro::async_manual_reset_event event1;
	cppcoro::async_manual_reset_event event2;

	auto run = [&]() -> cppcoro::task<>
	{
		std::vector<cppcoro::shared_lazy_task<>> tasks;
		tasks.push_back(makeTask(event1));
		tasks.push_back(makeTask(event2));
		tasks.push_back(makeTask(event1));

		auto allTask = cppcoro::when_all(std::move(tasks));

		CHECK(startedCount == 0);

		auto startedAllTask = start(std::move(allTask));

		CHECK(startedCount == 3);

		co_await startedAllTask;
	};

	auto t = run();

	CHECK(!t.is_ready());

	event1.set();

	CHECK(!t.is_ready());

	event2.set();

	CHECK(t.is_ready());
}

TEST_CASE("when_all() with vector<task<T>>")
{
	CHECK([]() -> cppcoro::task<>
	{
		cppcoro::async_manual_reset_event event1;
		cppcoro::async_manual_reset_event event2;
		std::vector<cppcoro::task<int>> tasks;
		tasks.emplace_back(when_event_set_return<cppcoro::task>(event1, 1));
		tasks.emplace_back(when_event_set_return<cppcoro::task>(event2, 2));
		auto whenAllTask = start(cppcoro::when_all(std::move(tasks)));
		CHECK(!whenAllTask.is_ready());
		event2.set();
		CHECK(!whenAllTask.is_ready());
		event1.set();
		CHECK(whenAllTask.is_ready());

		auto& values = co_await whenAllTask;
		REQUIRE(values.size() == 2);
		CHECK(values[0] == 1);
		CHECK(values[1] == 2);
	}().is_ready());
}

TEST_CASE("when_all() with vector<lazy_task<T>>")
{
	CHECK([]() -> cppcoro::task<>
	{
		cppcoro::async_manual_reset_event event1;
		cppcoro::async_manual_reset_event event2;

		std::vector<cppcoro::lazy_task<int>> tasks;
		tasks.emplace_back(when_event_set_return<cppcoro::lazy_task>(event1, 1));
		tasks.emplace_back(when_event_set_return<cppcoro::lazy_task>(event2, 2));

		auto whenAllTask = start(cppcoro::when_all(std::move(tasks)));
		CHECK(!whenAllTask.is_ready());
		event2.set();
		CHECK(!whenAllTask.is_ready());
		event1.set();
		CHECK(whenAllTask.is_ready());

		std::vector<int>& values = co_await whenAllTask;
		REQUIRE(values.size() == 2);
		CHECK(values[0] == 1);
		CHECK(values[1] == 2);
	}().is_ready());
}

TEST_CASE("when_all() with vector<shared_task<T>>")
{
	CHECK([]() -> cppcoro::task<>
	{
		cppcoro::async_manual_reset_event event1;
		cppcoro::async_manual_reset_event event2;
		
		std::vector<cppcoro::shared_task<int>> tasks;
		tasks.emplace_back(when_event_set_return<cppcoro::shared_task>(event1, 1));
		tasks.emplace_back(when_event_set_return<cppcoro::shared_task>(event2, 2));

		auto whenAllTask = start(cppcoro::when_all(std::move(tasks)));
		CHECK(!whenAllTask.is_ready());
		event2.set();
		CHECK(!whenAllTask.is_ready());
		event1.set();
		CHECK(whenAllTask.is_ready());

		auto& values = co_await whenAllTask;
		REQUIRE(values.size() == 2);
		CHECK(values[0] == 1);
		CHECK(values[1] == 2);
	}().is_ready());
}

TEST_CASE("when_all() with vector<shared_lazy_task<T>>")
{
	CHECK([]() -> cppcoro::task<>
	{
		cppcoro::async_manual_reset_event event1;
		cppcoro::async_manual_reset_event event2;

		std::vector<cppcoro::shared_lazy_task<int>> tasks;
		tasks.emplace_back(when_event_set_return<cppcoro::shared_lazy_task>(event1, 1));
		tasks.emplace_back(when_event_set_return<cppcoro::shared_lazy_task>(event2, 2));

		auto whenAllTask = start(cppcoro::when_all(std::move(tasks)));
		CHECK(!whenAllTask.is_ready());
		event2.set();
		CHECK(!whenAllTask.is_ready());
		event1.set();
		CHECK(whenAllTask.is_ready());

		std::vector<int>& values = co_await whenAllTask;
		REQUIRE(values.size() == 2);
		CHECK(values[0] == 1);
		CHECK(values[1] == 2);
	}().is_ready());
}

TEST_CASE("when_all() with vector<task<T&>>")
{
	[]() -> cppcoro::task<>
	{
		cppcoro::async_manual_reset_event event1;
		cppcoro::async_manual_reset_event event2;

		int value1 = 1;
		int value2 = 2;

		auto makeTask = [](cppcoro::async_manual_reset_event& event, int& value) -> cppcoro::task<int&>
		{
			co_await event;
			co_return value;
		};

		std::vector<cppcoro::task<int&>> tasks;
		tasks.emplace_back(makeTask(event1, value1));
		tasks.emplace_back(makeTask(event2, value2));

		auto whenAllTask = start(cppcoro::when_all(std::move(tasks)));
		CHECK(!whenAllTask.is_ready());
		event2.set();
		CHECK(!whenAllTask.is_ready());
		event1.set();
		CHECK(whenAllTask.is_ready());

		std::vector<std::reference_wrapper<int>>& values = co_await whenAllTask;
		REQUIRE(values.size() == 2);
		CHECK(&values[0].get() == &value1);
		CHECK(&values[1].get() == &value2);
	}();
}

TEST_CASE("when_all() with vector<lazy_task<T&>>")
{
	CHECK([]() -> cppcoro::task<>
	{
		cppcoro::async_manual_reset_event event1;
		cppcoro::async_manual_reset_event event2;

		int value1 = 1;
		int value2 = 2;

		auto makeTask = [](cppcoro::async_manual_reset_event& event, int& value) -> cppcoro::lazy_task<int&>
		{
			co_await event;
			co_return value;
		};

		std::vector<cppcoro::lazy_task<int&>> tasks;
		tasks.emplace_back(makeTask(event1, value1));
		tasks.emplace_back(makeTask(event2, value2));

		auto whenAllTask = start(cppcoro::when_all(std::move(tasks)));
		CHECK(!whenAllTask.is_ready());
		event2.set();
		CHECK(!whenAllTask.is_ready());
		event1.set();
		CHECK(whenAllTask.is_ready());

		std::vector<std::reference_wrapper<int>>& values = co_await whenAllTask;
		REQUIRE(values.size() == 2);
		CHECK(&values[0].get() == &value1);
		CHECK(&values[1].get() == &value2);
	}().is_ready());
}

TEST_CASE("when_all() with vector<shared_task<T&>>")
{
	CHECK([]() -> cppcoro::task<>
	{
		cppcoro::async_manual_reset_event event1;
		cppcoro::async_manual_reset_event event2;

		int value1 = 1;
		int value2 = 2;

		auto makeTask = [](cppcoro::async_manual_reset_event& event, int& value) -> cppcoro::shared_task<int&>
		{
			co_await event;
			co_return value;
		};

		std::vector<cppcoro::shared_task<int&>> tasks;
		tasks.emplace_back(makeTask(event1, value1));
		tasks.emplace_back(makeTask(event2, value2));

		auto whenAllTask = start(cppcoro::when_all(std::move(tasks)));
		CHECK(!whenAllTask.is_ready());
		event2.set();
		CHECK(!whenAllTask.is_ready());
		event1.set();
		CHECK(whenAllTask.is_ready());

		std::vector<std::reference_wrapper<int>>& values = co_await whenAllTask;
		REQUIRE(values.size() == 2);
		CHECK(&values[0].get() == &value1);
		CHECK(&values[1].get() == &value2);
	}().is_ready());
}

TEST_CASE("when_all() with vector<shared_lazy_task<T&>>")
{
	CHECK([]() -> cppcoro::task<>
	{
		cppcoro::async_manual_reset_event event1;
		cppcoro::async_manual_reset_event event2;

		int value1 = 1;
		int value2 = 2;

		auto makeTask = [](cppcoro::async_manual_reset_event& event, int& value) -> cppcoro::shared_lazy_task<int&>
		{
			co_await event;
			co_return value;
		};

		std::vector<cppcoro::shared_lazy_task<int&>> tasks;
		tasks.emplace_back(makeTask(event1, value1));
		tasks.emplace_back(makeTask(event2, value2));

		auto whenAllTask = start(cppcoro::when_all(std::move(tasks)));
		CHECK(!whenAllTask.is_ready());
		event2.set();
		CHECK(!whenAllTask.is_ready());
		event1.set();
		CHECK(whenAllTask.is_ready());

		std::vector<std::reference_wrapper<int>>& values = co_await whenAllTask;
		REQUIRE(values.size() == 2);
		CHECK(&values[0].get() == &value1);
		CHECK(&values[1].get() == &value2);
	}().is_ready());
}

TEST_SUITE_END();
