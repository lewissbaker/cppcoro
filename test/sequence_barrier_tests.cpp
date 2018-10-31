///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/sequence_barrier.hpp>

#include <cppcoro/config.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/static_thread_pool.hpp>
#include <cppcoro/inline_scheduler.hpp>

#include <stdio.h>
#include <thread>

#include "doctest/doctest.h"

DOCTEST_TEST_SUITE_BEGIN("sequence_barrier");

using namespace cppcoro;

DOCTEST_TEST_CASE("default construction")
{
	sequence_barrier<std::uint32_t> barrier;
	CHECK(barrier.last_published() == sequence_traits<std::uint32_t>::initial_sequence);
	barrier.publish(3);
	CHECK(barrier.last_published() == 3);
}

DOCTEST_TEST_CASE("constructing with initial sequence number")
{
	sequence_barrier<std::uint64_t> barrier{ 100 };
	CHECK(barrier.last_published() == 100);
}

DOCTEST_TEST_CASE("wait_until_published single-threaded")
{
	inline_scheduler scheduler;

	sequence_barrier<std::uint32_t> barrier;
	bool reachedA = false;
	bool reachedB = false;
	bool reachedC = false;
	bool reachedD = false;
	bool reachedE = false;
	bool reachedF = false;
	sync_wait(when_all(
		[&]() -> task<>
		{
			CHECK(co_await barrier.wait_until_published(0, scheduler) == 0);
			reachedA = true;
			CHECK(co_await barrier.wait_until_published(1, scheduler) == 1);
			reachedB = true;
			CHECK(co_await barrier.wait_until_published(3, scheduler) == 3);
			reachedC = true;
			CHECK(co_await barrier.wait_until_published(4, scheduler) == 10);
			reachedD = true;
			co_await barrier.wait_until_published(5, scheduler);
			reachedE = true;
			co_await barrier.wait_until_published(10, scheduler);
			reachedF = true;
		}(),
		[&]() -> task<>
		{
			CHECK(!reachedA);
			barrier.publish(0);
			CHECK(reachedA);
			CHECK(!reachedB);
			barrier.publish(1);
			CHECK(reachedB);
			CHECK(!reachedC);
			barrier.publish(2);
			CHECK(!reachedC);
			barrier.publish(3);
			CHECK(reachedC);
			CHECK(!reachedD);
			barrier.publish(10);
			CHECK(reachedD);
			CHECK(reachedE);
			CHECK(reachedF);
			co_return;
		}()));
	CHECK(reachedF);
}

DOCTEST_TEST_CASE("wait_until_published multiple awaiters")
{
	inline_scheduler scheduler;

	sequence_barrier<std::uint32_t> barrier;
	bool reachedA = false;
	bool reachedB = false;
	bool reachedC = false;
	bool reachedD = false;
	bool reachedE = false;
	sync_wait(when_all(
		[&]() -> task<>
	{
		CHECK(co_await barrier.wait_until_published(0, scheduler) == 0);
		reachedA = true;
		CHECK(co_await barrier.wait_until_published(1, scheduler) == 1);
		reachedB = true;
		CHECK(co_await barrier.wait_until_published(3, scheduler) == 3);
		reachedC = true;
	}(),
		[&]() -> task<>
	{
		CHECK(co_await barrier.wait_until_published(0, scheduler) == 0);
		reachedD = true;
		CHECK(co_await barrier.wait_until_published(3, scheduler) == 3);
		reachedE = true;
	}(),
		[&]() -> task<>
	{
		CHECK(!reachedA);
		CHECK(!reachedD);
		barrier.publish(0);
		CHECK(reachedA);
		CHECK(reachedD);
		CHECK(!reachedB);
		CHECK(!reachedE);
		barrier.publish(1);
		CHECK(reachedB);
		CHECK(!reachedC);
		CHECK(!reachedE);
		barrier.publish(2);
		CHECK(!reachedC);
		CHECK(!reachedE);
		barrier.publish(3);
		CHECK(reachedC);
		CHECK(reachedE);
		co_return;
	}()));
	CHECK(reachedC);
	CHECK(reachedE);
}

DOCTEST_TEST_CASE("multi-threaded usage single consumer")
{
	static_thread_pool tp{ 2 };

	sequence_barrier<std::size_t> writeBarrier;
	sequence_barrier<std::size_t> readBarrier;

	constexpr std::size_t iterationCount = 1'000'000;

	constexpr std::size_t bufferSize = 256;
	std::uint64_t buffer[bufferSize];

	auto[result, dummy] = sync_wait(when_all(
		[&]() -> task<std::uint64_t>
	{
		// Consumer
		std::uint64_t sum = 0;

		bool reachedEnd = false;
		std::size_t nextToRead = 0;
		do
		{
			std::size_t available = co_await writeBarrier.wait_until_published(nextToRead, tp);
			do
			{
				sum += buffer[nextToRead % bufferSize];
			} while (nextToRead++ != available);

			// Zero value is sentinel that indicates the end of the stream.
			reachedEnd = buffer[available % bufferSize] == 0;

			// Notify that we've finished processing up to 'available'.
			readBarrier.publish(available);
		} while (!reachedEnd);

		co_return sum;
	}(),
		[&]() -> task<>
	{
		// Producer
		std::size_t available = readBarrier.last_published() + bufferSize;
		for (std::size_t nextToWrite = 0; nextToWrite <= iterationCount; ++nextToWrite)
		{
			if (sequence_traits<std::size_t>::precedes(available, nextToWrite))
			{
				available = co_await readBarrier.wait_until_published(nextToWrite - bufferSize, tp) + bufferSize;
			}

			if (nextToWrite == iterationCount)
			{
				// Write sentinel (zero) as last element.
				buffer[nextToWrite % bufferSize] = 0;
			}
			else
			{
				// Write value
				buffer[nextToWrite % bufferSize] = nextToWrite + 1;
			}

			// Notify consumer that we've published a new value.
			writeBarrier.publish(nextToWrite);
		}
	}()));

	// Suppress unused variable warning.
	(void)dummy;

	constexpr std::uint64_t expectedResult =
		std::uint64_t(iterationCount) * std::uint64_t(iterationCount + 1) / 2;

	CHECK(result == expectedResult);
}

DOCTEST_TEST_SUITE_END();
