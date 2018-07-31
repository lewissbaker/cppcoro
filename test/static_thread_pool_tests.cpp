///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/static_thread_pool.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>

#include <vector>
#include <thread>
#include <cassert>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("static_thread_pool");

TEST_CASE("construct/destruct")
{
	cppcoro::static_thread_pool threadPool;
	CHECK(threadPool.thread_count() == std::thread::hardware_concurrency());
}

TEST_CASE("construct/destruct to specific thread count")
{
	cppcoro::static_thread_pool threadPool{ 5 };
	CHECK(threadPool.thread_count() == 5);
}

TEST_CASE("run one task")
{
	cppcoro::static_thread_pool threadPool{ 2 };

	auto initiatingThreadId = std::this_thread::get_id();

	cppcoro::sync_wait([&]() -> cppcoro::task<void>
	{
		co_await threadPool.schedule();
		if (std::this_thread::get_id() == initiatingThreadId)
		{
			FAIL("schedule() did not switch threads");
		}
	}());
}

TEST_CASE("launch many tasks remotely")
{
	cppcoro::static_thread_pool threadPool;

	auto makeTask = [&]() -> cppcoro::task<>
	{
		co_await threadPool.schedule();
	};

	std::vector<cppcoro::task<>> tasks;
	for (std::uint32_t i = 0; i < 100; ++i)
	{
		tasks.push_back(makeTask());
	}

	cppcoro::sync_wait(cppcoro::when_all(std::move(tasks)));
}

cppcoro::task<std::uint64_t> sum_of_squares(
	std::uint32_t start,
	std::uint32_t end,
	cppcoro::static_thread_pool& tp)
{
	co_await tp.schedule();

	auto count = end - start;
	if (count > 1000)
	{
		auto half = start + count / 2;
		auto[a, b] = co_await cppcoro::when_all(
			sum_of_squares(start, half, tp),
			sum_of_squares(half, end, tp));
		co_return a + b;
	}
	else
	{
		std::uint64_t sum = 0;
		for (std::uint64_t x = start; x < end; ++x)
		{
			sum += x * x;
		}
		co_return sum;
	}
}

TEST_CASE("launch sub-task with many sub-tasks")
{
	cppcoro::static_thread_pool tp;
	auto result = cppcoro::sync_wait(sum_of_squares(0, 1'000'000'000, tp));

	std::uint64_t sum = 0;
	for (std::uint64_t i = 0; i < 1'000'000'000; ++i)
	{
		sum += i * i;
	}

	CHECK(result == sum);
}

TEST_SUITE_END();
