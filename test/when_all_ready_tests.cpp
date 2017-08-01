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

TEST_SUITE_BEGIN("when_all_ready");

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

TEST_CASE("when_all_ready() with no args")
{
	auto run = []() -> cppcoro::task<>
	{
		co_await cppcoro::when_all_ready();
	};
	CHECK(run().is_ready());
}

TEST_CASE("when_all_ready() with one lazy_task")
{
	bool started = false;
	auto f = [&](cppcoro::async_manual_reset_event& event) -> cppcoro::lazy_task<>
	{
		started = true;
		co_await event;
	};

	cppcoro::async_manual_reset_event event;
	auto whenAllTask = cppcoro::when_all_ready(f(event));
	CHECK(!started);

	auto g = [&]() -> cppcoro::task<>
	{
		auto& [t] = co_await whenAllTask;
		CHECK(t.is_ready());
	};

	auto whenAllAwaiterTask = g();

	CHECK(started);
	CHECK(!whenAllAwaiterTask.is_ready());

	event.set();

	CHECK(whenAllAwaiterTask.is_ready());
}

TEST_CASE("when_all_ready() with multiple lazy_task")
{
	auto makeTask = [&](bool& started, cppcoro::async_manual_reset_event& event) -> cppcoro::lazy_task<>
	{
		started = true;
		co_await event;
	};

	cppcoro::async_manual_reset_event event1;
	cppcoro::async_manual_reset_event event2;
	bool started1 = false;
	bool started2 = false;
	auto whenAllTask = cppcoro::when_all_ready(
		makeTask(started1, event1),
		makeTask(started2, event2));
	CHECK(!started1);
	CHECK(!started2);

	auto g = [&]() -> cppcoro::task<>
	{
		auto[t1, t2] = co_await std::move(whenAllTask);
		CHECK(t1.is_ready());
		CHECK(t2.is_ready());
	};

	auto whenAllAwaiterTask = g();

	CHECK(started1);
	CHECK(started2);
	CHECK(!whenAllAwaiterTask.is_ready());

	event2.set();

	CHECK(!whenAllAwaiterTask.is_ready());

	event1.set();

	CHECK(whenAllAwaiterTask.is_ready());
}

TEST_CASE("when_all_ready() with all task types")
{
	cppcoro::async_manual_reset_event event;
	auto t0 = when_event_set_return<cppcoro::task>(event, 0);
	auto t1 = when_event_set_return<cppcoro::lazy_task>(event, 1);
	auto t2 = when_event_set_return<cppcoro::shared_task>(event, 2);
	auto t3 = when_event_set_return<cppcoro::shared_lazy_task>(event, 3);

	auto allTask = start(cppcoro::when_all_ready(std::move(t0), std::move(t1), t2, t3));

	CHECK(!allTask.is_ready());

	event.set();

	CHECK(allTask.is_ready());

	CHECK([&]() -> cppcoro::task<>
	{
		auto [t0, t1, t2, t3] = co_await std::move(allTask);
		CHECK(co_await t0 == 0);
		CHECK(co_await t1 == 1);
		CHECK(co_await t2 == 2);
		CHECK(co_await t3 == 3);
	}().is_ready());
}

TEST_CASE("when_all_ready() with all task types passed by ref")
{
	cppcoro::async_manual_reset_event event;
	auto t0 = when_event_set_return<cppcoro::task>(event, 0);
	auto t1 = when_event_set_return<cppcoro::lazy_task>(event, 1);
	auto t2 = when_event_set_return<cppcoro::shared_task>(event, 2);
	auto t3 = when_event_set_return<cppcoro::shared_lazy_task>(event, 3);

	auto allTask = start(cppcoro::when_all_ready(
		std::ref(t0),
		std::ref(t1),
		std::ref(t2),
		std::ref(t3)));

	CHECK(!allTask.is_ready());

	event.set();

	CHECK(allTask.is_ready());

	CHECK([&]() -> cppcoro::task<>
	{
		auto[u0, u1, u2, u3] = co_await allTask;

		// Address of reference should be same as address of original task.
		CHECK(&u0.get() == &t0);
		CHECK(&u1.get() == &t1);
		CHECK(&u2.get() == &t2);
		CHECK(&u3.get() == &t3);

		CHECK(co_await t0 == 0);
		CHECK(co_await t1 == 1);
		CHECK(co_await t2 == 2);
		CHECK(co_await t3 == 3);
	}().is_ready());
}

TEST_CASE("when_all_ready() with std::vector<lazy_task<T>>")
{
	cppcoro::async_manual_reset_event event;

	std::uint32_t startedCount = 0;

	auto makeTask = [&]() -> cppcoro::lazy_task<>
	{
		++startedCount;
		co_await event;
	};

	std::vector<cppcoro::lazy_task<>> tasks;
	for (std::uint32_t i = 0; i < 10; ++i)
	{
		tasks.emplace_back(makeTask());
	}

	cppcoro::lazy_task<std::vector<cppcoro::lazy_task<>>> allTask =
		cppcoro::when_all_ready(std::move(tasks));

	// Shouldn't have started any tasks yet.
	CHECK(startedCount == 0u);

	auto startedAllTask = start(std::move(allTask));

	CHECK(startedCount == 10u);

	CHECK(!startedAllTask.is_ready());

	event.set();

	CHECK(startedAllTask.is_ready());

	CHECK([&]() -> cppcoro::task<>
	{
		auto tasks = co_await std::move(startedAllTask);
		CHECK(tasks.size() == 10u);

		for (auto& t : tasks)
		{
			CHECK(t.is_ready());
		}
	}().is_ready());
}

TEST_CASE("when_all_ready() with std::vector<shared_lazy_task<T>>")
{
	cppcoro::async_manual_reset_event event;

	std::uint32_t startedCount = 0;

	auto makeTask = [&]() -> cppcoro::shared_lazy_task<>
	{
		++startedCount;
		co_await event;
	};

	std::vector<cppcoro::shared_lazy_task<>> tasks;
	for (std::uint32_t i = 0; i < 10; ++i)
	{
		tasks.emplace_back(makeTask());
	}

	cppcoro::lazy_task<std::vector<cppcoro::shared_lazy_task<>>> allTask =
		cppcoro::when_all_ready(std::move(tasks));

	// Shouldn't have started any tasks yet as allTask is a lazy_task
	CHECK(startedCount == 0u);

	auto startedAllTask = start(std::move(allTask));

	CHECK(startedCount == 10u);

	CHECK(!startedAllTask.is_ready());

	event.set();

	CHECK(startedAllTask.is_ready());

	CHECK([&]() -> cppcoro::task<>
	{
		auto tasks = co_await std::move(startedAllTask);
		CHECK(tasks.size() == 10u);

		for (auto& t : tasks)
		{
			CHECK(t.is_ready());
		}
	}().is_ready());
}

TEST_CASE("when_all_ready() with std::vector<task<T>>")
{
	cppcoro::async_manual_reset_event event;

	auto makeTask = [&]() -> cppcoro::task<>
	{
		co_await event;
	};

	std::vector<cppcoro::task<>> tasks;
	tasks.emplace_back(makeTask());
	tasks.emplace_back(makeTask());
	tasks.emplace_back(makeTask());

	auto startedAllTask = start(cppcoro::when_all_ready(std::move(tasks)));

	CHECK(!startedAllTask.is_ready());

	event.set();

	CHECK(startedAllTask.is_ready());

	CHECK([&]() -> cppcoro::task<>
	{
		auto tasks = co_await std::move(startedAllTask);

		CHECK(tasks.size() == 3u);

		for (auto& t : tasks)
		{
			CHECK(t.is_ready());
		}
	}().is_ready());
}

TEST_CASE("when_all_ready() with std::vector<shared_task<T>>")
{
	auto makeTask = [](cppcoro::async_manual_reset_event& event) -> cppcoro::shared_task<>
	{
		co_await event;
	};

	cppcoro::async_manual_reset_event event1;
	cppcoro::async_manual_reset_event event2;
	cppcoro::async_manual_reset_event event3;

	std::vector<cppcoro::shared_task<>> tasks = { makeTask(event1), makeTask(event2), makeTask(event3) };

	// We can pass the vector by copy into when_all_ready as we're using shared_task
	// which support copy-construction.
	auto startedAllTask = start(cppcoro::when_all_ready(tasks));

	CHECK(!startedAllTask.is_ready());

	event2.set();
	CHECK(!startedAllTask.is_ready());
	CHECK(tasks[1].is_ready());

	event3.set();
	CHECK(!startedAllTask.is_ready());
	CHECK(tasks[2].is_ready());

	event1.set();
	CHECK(startedAllTask.is_ready());
	CHECK(tasks[0].is_ready());

	CHECK([&]() -> cppcoro::task<>
	{
		auto& resultTasks = co_await startedAllTask;

		CHECK(resultTasks.size() == 3u);

		for (auto& t : tasks)
		{
			CHECK(t.is_ready());
		}
	}().is_ready());
}

TEST_CASE("when_all_ready() doesn't rethrow exceptions")
{
	auto makeTask = [](bool throwException) -> cppcoro::lazy_task<int>
	{
		if (throwException)
		{
			throw std::exception{};
		}
		else
		{
			co_return 123;
		}
	};

	CHECK([&]() -> cppcoro::task<>
	{
		try
		{
			auto[t0, t1] = co_await cppcoro::when_all_ready(makeTask(true), makeTask(false));

			CHECK_THROWS_AS(co_await t0, const std::exception&);
			CHECK(co_await t1 == 123);
		}
		catch (...)
		{
			FAIL("Shouldn't throw");
		}
	}().is_ready());
}

TEST_SUITE_END();
