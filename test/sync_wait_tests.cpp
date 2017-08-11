///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/sync_wait.hpp>

#include <cppcoro/io_service.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/lazy_task.hpp>
#include <cppcoro/shared_task.hpp>
#include <cppcoro/shared_lazy_task.hpp>
#include <cppcoro/on_scope_exit.hpp>

#include <string>
#include <thread>
#include <type_traits>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("sync_wait");

static_assert(std::is_same<
	decltype(cppcoro::sync_wait(std::declval<cppcoro::lazy_task<std::string>>())),
	std::string&&>::value);
static_assert(std::is_same<
	decltype(cppcoro::sync_wait(std::declval<cppcoro::lazy_task<std::string>&>())),
	std::string&>::value);

TEST_CASE("sync_wait(task<T>)")
{
	auto makeTask = []() -> cppcoro::task<std::string>
	{
		co_return "foo";
	};

	auto task = makeTask();
	CHECK(cppcoro::sync_wait(task) == "foo");

	CHECK(cppcoro::sync_wait(makeTask()) == "foo");
}

TEST_CASE("sync_wait(lazy_task<T>)")
{
	auto makeTask = []() -> cppcoro::lazy_task<std::string>
	{
		co_return "foo";
	};

	auto task = makeTask();
	CHECK(cppcoro::sync_wait(task) == "foo");

	CHECK(cppcoro::sync_wait(makeTask()) == "foo");
}

TEST_CASE("sync_wait(shared_lazy_task<T>)")
{
	auto makeTask = []() -> cppcoro::shared_lazy_task<std::string>
	{
		co_return "foo";
	};

	auto task = makeTask();

	CHECK(cppcoro::sync_wait(task) == "foo");
	CHECK(cppcoro::sync_wait(makeTask()) == "foo");
}

TEST_CASE("sync_wait(shared_task<T>)")
{
	auto makeTask = []() -> cppcoro::shared_task<std::string>
	{
		co_return "foo";
	};

	auto task = makeTask();

	CHECK(cppcoro::sync_wait(task) == "foo");
	CHECK(cppcoro::sync_wait(makeTask()) == "foo");
}

TEST_CASE("multiple threads")
{
	// We are creating a new task and starting it inside the sync_wait().
	// The task will reschedule itself for resumption on an I/O thread
	// which will sometimes complete before this thread calls event.wait()
	// inside sync_wait(). Thus we're roughly testing the thread-safety of
	// sync_wait().

	cppcoro::io_service ioService;
	std::thread thread{ [&] { ioService.process_events(); } };
	auto joinThreadOnExit = cppcoro::on_scope_exit([&] { thread.join(); });
	auto stopIoServiceOnExit = cppcoro::on_scope_exit([&] { ioService.stop(); });

	{
		int value = 0;
		auto createLazyTask = [&]() -> cppcoro::lazy_task<int>
		{
			co_await ioService.schedule();
			co_return value++;
		};

		for (int i = 0; i < 10'000; ++i)
		{
			CHECK(cppcoro::sync_wait(createLazyTask()) == i);
		}
	}

	{
		int value = 0;
		auto createTask = [&]() -> cppcoro::task<int>
		{
			co_await ioService.schedule();
			co_return value++;
		};

		for (int i = 0; i < 10'000; ++i)
		{
			CHECK(cppcoro::sync_wait(createTask()) == i);
		}
	}
}

TEST_SUITE_END();
