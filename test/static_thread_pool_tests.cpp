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
#include <chrono>
#include <iostream>
#include <numeric>

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
	using namespace std::chrono_literals;

	constexpr std::uint64_t limit = 1'000'000'000;

	cppcoro::static_thread_pool tp;

	// Wait for the thread-pool thread to start up.
	std::this_thread::sleep_for(1ms);

	auto start = std::chrono::high_resolution_clock::now();

	auto result = cppcoro::sync_wait(sum_of_squares(0, limit , tp));

	auto end = std::chrono::high_resolution_clock::now();

	std::uint64_t sum = 0;
	for (std::uint64_t i = 0; i < limit; ++i)
	{
		sum += i * i;
	}

	auto end2 = std::chrono::high_resolution_clock::now();

	auto toNs = [](auto time)
	{
		return std::chrono::duration_cast<std::chrono::nanoseconds>(time).count();
	};

	std::cout
		<< "multi-threaded version took " << toNs(end - start) << "ns\n"
		<< "single-threaded version took " << toNs(end2 - end) << "ns" << std::endl;

	CHECK(result == sum);
}

struct fork_join_operation
{
	std::atomic<std::size_t> m_count;
	std::coroutine_handle<> m_coro;

	fork_join_operation() : m_count(1) {}

	void begin_work() noexcept
	{
		m_count.fetch_add(1, std::memory_order_relaxed);
	}

	void end_work() noexcept
	{
		if (m_count.fetch_sub(1, std::memory_order_acq_rel) == 1)
		{
			m_coro.resume();
		}
	}

	bool await_ready() noexcept { return m_count.load(std::memory_order_acquire) == 1; }

	bool await_suspend(std::coroutine_handle<> coro) noexcept
	{
		m_coro = coro;
		return m_count.fetch_sub(1, std::memory_order_acq_rel) != 1;
	}

	void await_resume() noexcept {};
};

template<typename FUNC, typename RANGE, typename SCHEDULER>
cppcoro::task<void> for_each_async(SCHEDULER& scheduler, RANGE& range, FUNC func)
{
	using reference_type = decltype(*range.begin());

	// TODO: Use awaiter_t here instead. This currently assumes that
	// result of scheduler.schedule() doesn't have an operator co_await().
	using schedule_operation = decltype(scheduler.schedule());

	struct work_operation
	{
		fork_join_operation& m_forkJoin;
		FUNC& m_func;
		reference_type m_value;
		schedule_operation m_scheduleOp;

		work_operation(fork_join_operation& forkJoin, SCHEDULER& scheduler, FUNC& func, reference_type&& value)
			: m_forkJoin(forkJoin)
			, m_func(func)
			, m_value(static_cast<reference_type&&>(value))
			, m_scheduleOp(scheduler.schedule())
		{
		}

		bool await_ready() noexcept { return false; }

		CPPCORO_NOINLINE
		void await_suspend(std::coroutine_handle<> coro) noexcept
		{
			fork_join_operation& forkJoin = m_forkJoin;
			FUNC& func = m_func;
			reference_type value = static_cast<reference_type&&>(m_value);

			static_assert(std::is_same_v<decltype(m_scheduleOp.await_suspend(coro)), void>);

			forkJoin.begin_work();

			// Schedule the next iteration of the loop to run
			m_scheduleOp.await_suspend(coro);

			func(static_cast<reference_type&&>(value));

			forkJoin.end_work();
		}

		void await_resume() noexcept {}
	};

	co_await scheduler.schedule();

	fork_join_operation forkJoin;

	for (auto&& x : range)
	{
		co_await work_operation{
			forkJoin,
			scheduler,
			func,
			static_cast<decltype(x)>(x)
		};
	}

	co_await forkJoin;
}

std::uint64_t collatz_distance(std::uint64_t number)
{
	std::uint64_t count = 0;
	while (number > 1)
	{
		if (number % 2 == 0) number /= 2;
		else number = number * 3 + 1;
		++count;
	}
	return count;
}

TEST_CASE("for_each_async")
{
	cppcoro::static_thread_pool tp;

	{
		std::vector<std::uint64_t> values(1'000'000);
		std::iota(values.begin(), values.end(), 1);

		cppcoro::sync_wait([&]() -> cppcoro::task<>
		{
			auto start = std::chrono::high_resolution_clock::now();

			co_await for_each_async(tp, values, [](std::uint64_t& value)
			{
				value = collatz_distance(value);
			});

			auto end = std::chrono::high_resolution_clock::now();

			std::cout << "for_each_async of " << values.size()
				<< " took " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
				<< "us" << std::endl;

			for (std::size_t i = 0; i < 1'000'000; ++i)
			{
				CHECK(values[i] == collatz_distance(i + 1));
			}
		}());
	}

	{
		std::vector<std::uint64_t> values(1'000'000);
		std::iota(values.begin(), values.end(), 1);

		auto start = std::chrono::high_resolution_clock::now();

		for (auto&& x : values)
		{
			x = collatz_distance(x);
		}

		auto end = std::chrono::high_resolution_clock::now();

		std::cout << "single-threaded for loop of " << values.size()
			<< " took " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
			<< "us" << std::endl;
	}

}

TEST_SUITE_END();
