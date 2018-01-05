///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/when_all.hpp>

#include <cppcoro/task.hpp>
#include <cppcoro/shared_task.hpp>
#include <cppcoro/sync_wait.hpp>
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

TEST_CASE("when_all_ready() with no args")
{
	[[maybe_unused]] std::tuple<> result = cppcoro::sync_wait(cppcoro::when_all_ready());
}

TEST_CASE("when_all_ready() with one task")
{
	bool started = false;
	auto f = [&](cppcoro::async_manual_reset_event& event) -> cppcoro::task<>
	{
		started = true;
		co_await event;
	};

	cppcoro::async_manual_reset_event event;
	auto whenAllTask = cppcoro::when_all_ready(f(event));
	CHECK(!started);

	cppcoro::sync_wait(cppcoro::when_all_ready(
		[&]() -> cppcoro::task<>
	{
		auto&[t] = co_await whenAllTask;
		CHECK(t.is_ready());
	}(),
		[&]() -> cppcoro::task<>
	{
		CHECK(started);
		event.set();
		CHECK(whenAllTask.is_ready());
		co_return;
	}()));
}

TEST_CASE("when_all_ready() with multiple task")
{
	auto makeTask = [&](bool& started, cppcoro::async_manual_reset_event& event) -> cppcoro::task<>
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

	bool whenAllTaskFinished = false;

	cppcoro::sync_wait(cppcoro::when_all_ready(
		[&]() -> cppcoro::task<>
	{
		auto[t1, t2] = co_await std::move(whenAllTask);
		whenAllTaskFinished = true;
		CHECK(t1.is_ready());
		CHECK(t2.is_ready());
	}(),
		[&]() -> cppcoro::task<>
	{
		CHECK(started1);
		CHECK(started2);

		event2.set();

		CHECK(!whenAllTaskFinished);

		event1.set();

		CHECK(whenAllTaskFinished);

		co_return;
	}()));
}

TEST_CASE("when_all_ready() with all task types")
{
	cppcoro::async_manual_reset_event event;
	auto t0 = when_event_set_return<cppcoro::task>(event, 1);
	auto t1 = when_event_set_return<cppcoro::shared_task>(event, 2);

	auto allTask = cppcoro::when_all_ready(std::move(t0), t1);

	cppcoro::sync_wait(cppcoro::when_all_ready(
		[&]() -> cppcoro::task<>
	{
		auto [r0, r1] = co_await std::move(allTask);

		CHECK(r0.is_ready());
		CHECK(r1.is_ready());

		CHECK(co_await r0 == 1);
		CHECK(co_await r1 == 2);
	}(),
		[&]() -> cppcoro::task<>
	{
		event.set();
		co_return;
	}()));
}

TEST_CASE("when_all_ready() with all task types passed by ref")
{
	cppcoro::async_manual_reset_event event;
	auto t0 = when_event_set_return<cppcoro::task>(event, 1);
	auto t1 = when_event_set_return<cppcoro::shared_task>(event, 2);

	auto allTask = cppcoro::when_all_ready(
		std::ref(t0),
		std::ref(t1));

	cppcoro::sync_wait(cppcoro::when_all_ready(
		[&]() -> cppcoro::task<>
	{
		auto[u0, u1] = co_await allTask;

		// Address of reference should be same as address of original task.
		CHECK(&u0.get() == &t0);
		CHECK(&u1.get() == &t1);

		CHECK(co_await t0 == 1);
		CHECK(co_await t1 == 2);
	}(),
		[&]() -> cppcoro::task<>
	{
		event.set();
		co_return;
	}()));

	CHECK(allTask.is_ready());
}

TEST_CASE("when_all_ready() with std::vector<task<T>>")
{
	cppcoro::async_manual_reset_event event;

	std::uint32_t startedCount = 0;
	std::uint32_t finishedCount = 0;

	auto makeTask = [&]() -> cppcoro::task<>
	{
		++startedCount;
		co_await event;
		++finishedCount;
	};

	std::vector<cppcoro::task<>> tasks;
	for (std::uint32_t i = 0; i < 10; ++i)
	{
		tasks.emplace_back(makeTask());
	}

	cppcoro::task<std::vector<cppcoro::task<>>> allTask =
		cppcoro::when_all_ready(std::move(tasks));

	// Shouldn't have started any tasks yet.
	CHECK(startedCount == 0u);

	cppcoro::sync_wait(cppcoro::when_all_ready(
		[&]() -> cppcoro::task<>
	{
		auto resultTasks = co_await std::move(allTask);
		CHECK(resultTasks .size() == 10u);

		for (auto& t : resultTasks)
		{
			CHECK(t.is_ready());
		}
	}(),
		[&]() -> cppcoro::task<>
	{
		CHECK(startedCount == 10u);
		CHECK(finishedCount == 0u);

		event.set();

		CHECK(finishedCount == 10u);

		co_return;
	}()));
}

TEST_CASE("when_all_ready() with std::vector<shared_task<T>>")
{
	cppcoro::async_manual_reset_event event;

	std::uint32_t startedCount = 0;
	std::uint32_t finishedCount = 0;

	auto makeTask = [&]() -> cppcoro::shared_task<>
	{
		++startedCount;
		co_await event;
		++finishedCount;
	};

	std::vector<cppcoro::shared_task<>> tasks;
	for (std::uint32_t i = 0; i < 10; ++i)
	{
		tasks.emplace_back(makeTask());
	}

	cppcoro::task<std::vector<cppcoro::shared_task<>>> allTask =
		cppcoro::when_all_ready(std::move(tasks));

	// Shouldn't have started any tasks yet.
	CHECK(startedCount == 0u);

	cppcoro::sync_wait(cppcoro::when_all_ready(
		[&]() -> cppcoro::task<>
	{
		auto resultTasks = co_await std::move(allTask);
		CHECK(resultTasks.size() == 10u);

		for (auto& t : resultTasks)
		{
			CHECK(t.is_ready());
		}
	}(),
		[&]() -> cppcoro::task<>
	{
		CHECK(startedCount == 10u);
		CHECK(finishedCount == 0u);

		event.set();

		CHECK(finishedCount == 10u);

		co_return;
	}()));
}

TEST_CASE("when_all_ready() doesn't rethrow exceptions")
{
	auto makeTask = [](bool throwException) -> cppcoro::task<int>
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

	cppcoro::sync_wait([&]() -> cppcoro::task<>
	{
		try
		{
			auto[t0, t1] = co_await cppcoro::when_all_ready(makeTask(true), makeTask(false));

			// You can obtain the exceptions by re-awaiting the returned tasks.
			CHECK_THROWS_AS((void)co_await t0, const std::exception&);
			CHECK(co_await t1 == 123);
		}
		catch (...)
		{
			FAIL("Shouldn't throw");
		}
	}());
}

TEST_SUITE_END();
