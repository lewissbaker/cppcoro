///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/multi_producer_sequencer.hpp>

#include <cppcoro/config.hpp>
#include <cppcoro/sequence_barrier.hpp>
#include <cppcoro/sequence_traits.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/static_thread_pool.hpp>

#include <thread>
#include <chrono>

#include <ostream>
#include "doctest/doctest.h"

DOCTEST_TEST_SUITE_BEGIN("multi_producer_sequencer");

using namespace cppcoro;

namespace
{
	task<> one_at_a_time_producer(
		static_thread_pool& tp,
		multi_producer_sequencer<std::size_t>& sequencer,
		std::uint64_t buffer[],
		std::uint64_t iterationCount)
	{
		if (iterationCount == 0) co_return;

		co_await tp.schedule();

		const std::size_t bufferSize = sequencer.buffer_size();
		const std::size_t mask = bufferSize - 1;

		std::uint64_t i = 0;
		while (i < iterationCount)
		{
			auto seq = co_await sequencer.claim_one(tp);
			buffer[seq & mask] = ++i;
			sequencer.publish(seq);
		}

		auto finalSeq = co_await sequencer.claim_one(tp);
		buffer[finalSeq & mask] = 0;
		sequencer.publish(finalSeq);
	}

	task<> batch_producer(
		static_thread_pool& tp,
		multi_producer_sequencer<std::size_t>& sequencer,
		std::uint64_t buffer[],
		std::uint64_t iterationCount,
		std::size_t maxBatchSize)
	{
		const std::size_t bufferSize = sequencer.buffer_size();

		std::uint64_t i = 0;
		while (i < iterationCount)
		{
			const std::size_t batchSize = static_cast<std::size_t>(
				std::min<std::uint64_t>(maxBatchSize, iterationCount - i));
			auto sequences = co_await sequencer.claim_up_to(batchSize, tp);
			for (auto seq : sequences)
			{
				buffer[seq % bufferSize] = ++i;
			}
			sequencer.publish(sequences);
		}

		auto finalSeq = co_await sequencer.claim_one(tp);
		buffer[finalSeq % bufferSize] = 0;
		sequencer.publish(finalSeq);
	}

	task<std::uint64_t> consumer(
		static_thread_pool& tp,
		const multi_producer_sequencer<std::size_t>& sequencer,
		sequence_barrier<std::size_t>& readBarrier,
		const std::uint64_t buffer[],
		std::uint32_t producerCount)
	{
		co_await tp.schedule();

		const std::size_t mask = sequencer.buffer_size() - 1;

		std::uint64_t sum = 0;

		std::uint32_t endCount = 0;
		std::size_t nextToRead = 0;
		do
		{
			std::size_t available = co_await sequencer.wait_until_published(nextToRead, nextToRead - 1, tp);
			do
			{
				const auto& value = buffer[nextToRead & mask];
				sum += value;

				// Zero value is sentinel that indicates the end of one of the streams.
				const bool isEndOfStream = value == 0;
				endCount += isEndOfStream ? 1 : 0;
			} while (nextToRead++ != available);

			// Notify that we've finished processing up to 'available'.
			readBarrier.publish(available);
		} while (endCount < producerCount);

		co_return sum;
	}
}

DOCTEST_TEST_CASE("two producers (batch) / single consumer")
{
	static_thread_pool tp{ 3 };

	// Allow time for threads to start up.
	using namespace std::chrono_literals;
	std::this_thread::sleep_for(1ms);

	constexpr std::size_t batchSize = 10;
	constexpr std::size_t bufferSize = 16384;

	sequence_barrier<std::size_t> readBarrier;
	multi_producer_sequencer<std::size_t> sequencer(readBarrier, bufferSize);

	constexpr std::uint64_t iterationCount = 1'000'000;

	std::uint64_t buffer[bufferSize];

	auto startTime = std::chrono::high_resolution_clock::now();

	constexpr std::uint32_t producerCount = 2;
	auto result = std::get<0>(sync_wait(when_all(
		consumer(tp, sequencer, readBarrier, buffer, producerCount),
		batch_producer(tp, sequencer, buffer, iterationCount, batchSize),
		batch_producer(tp, sequencer, buffer, iterationCount, batchSize))));

	auto endTime = std::chrono::high_resolution_clock::now();

	auto totalTimeInNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count();

	MESSAGE(
		"Producers = " << producerCount
		<< ", BatchSize = " << batchSize
		<< ", MessagesPerProducer = " << iterationCount
		<< ", TotalTime = " << totalTimeInNs/1000 << "us"
		<< ", TimePerMessage = " << totalTimeInNs/double(iterationCount * producerCount) << "ns"
		<< ", MessagesPerSecond = " << 1'000'000'000 * (producerCount * iterationCount) / totalTimeInNs);

	constexpr std::uint64_t expectedResult =
		producerCount * std::uint64_t(iterationCount) * std::uint64_t(iterationCount + 1) / 2;

	CHECK(result == expectedResult);
}

DOCTEST_TEST_CASE("two producers (single) / single consumer")
{
	static_thread_pool tp{ 3 };

	// Allow time for threads to start up.
	using namespace std::chrono_literals;
	std::this_thread::sleep_for(1ms);

	constexpr std::size_t bufferSize = 16384;

	sequence_barrier<std::size_t> readBarrier;
	multi_producer_sequencer<std::size_t> sequencer(readBarrier, bufferSize);

	constexpr std::uint64_t iterationCount = 1'000'000;

	std::uint64_t buffer[bufferSize];

	auto startTime = std::chrono::high_resolution_clock::now();

	constexpr std::uint32_t producerCount = 2;
	auto result = std::get<0>(sync_wait(when_all(
		consumer(tp, sequencer, readBarrier, buffer, producerCount),
		one_at_a_time_producer(tp, sequencer, buffer, iterationCount),
		one_at_a_time_producer(tp, sequencer, buffer, iterationCount))));

	auto endTime = std::chrono::high_resolution_clock::now();

	auto totalTimeInNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count();

	MESSAGE(
		"Producers = " << producerCount
		<< ", NoBatch"
		<< ", MessagesPerProducer = " << iterationCount
		<< ", TotalTime = " << totalTimeInNs / 1000 << "us"
		<< ", TimePerMessage = " << totalTimeInNs / double(iterationCount * producerCount) << "ns"
		<< ", MessagesPerSecond = " << 1'000'000'000 * (producerCount * iterationCount) / totalTimeInNs);

	constexpr std::uint64_t expectedResult =
		producerCount * std::uint64_t(iterationCount) * std::uint64_t(iterationCount + 1) / 2;

	CHECK(result == expectedResult);
}

DOCTEST_TEST_SUITE_END();
