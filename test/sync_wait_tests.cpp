///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/sync_wait.hpp>

#include <cppcoro/config.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/shared_task.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/static_thread_pool.hpp>

#include <string>
#include <type_traits>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("sync_wait");

static_assert(std::is_same<
	decltype(cppcoro::sync_wait(std::declval<cppcoro::task<std::string>>())),
	std::string&&>::value);
static_assert(std::is_same<
	decltype(cppcoro::sync_wait(std::declval<cppcoro::task<std::string>&>())),
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
	// The task will reschedule itself for resumption on a thread-pool thread
	// which will sometimes complete before this thread calls event.wait()
	// inside sync_wait(). Thus we're roughly testing the thread-safety of
	// sync_wait().
	cppcoro::static_thread_pool tp{ 1 };

	int value = 0;
	auto createLazyTask = [&]() -> cppcoro::task<int>
	{
		co_await tp.schedule();
		co_return value++;
	};

	for (int i = 0; i < 10'000; ++i)
	{
		CHECK(cppcoro::sync_wait(createLazyTask()) == i);
	}
}

TEST_SUITE_END();
