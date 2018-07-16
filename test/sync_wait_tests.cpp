///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/sync_wait.hpp>

#include <cppcoro/config.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/shared_task.hpp>
#include <cppcoro/on_scope_exit.hpp>

#ifdef CPPCORO_IO_ENABLED
# include <cppcoro/io_service.hpp>
# include "io_service_fixture.hpp"
#endif

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

#ifdef CPPCORO_IO_ENABLED

TEST_CASE_FIXTURE(io_service_fixture_with_threads<1>, "multiple threads")
{
	// We are creating a new task and starting it inside the sync_wait().
	// The task will reschedule itself for resumption on an I/O thread
	// which will sometimes complete before this thread calls event.wait()
	// inside sync_wait(). Thus we're roughly testing the thread-safety of
	// sync_wait().

	int value = 0;
	auto createLazyTask = [&]() -> cppcoro::task<int>
	{
		co_await io_service().schedule();
		co_return value++;
	};

	for (int i = 0; i < 10'000; ++i)
	{
		CHECK(cppcoro::sync_wait(createLazyTask()) == i);
	}
}

#endif

TEST_SUITE_END();
