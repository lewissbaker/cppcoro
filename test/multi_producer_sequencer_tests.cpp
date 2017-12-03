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

#if CPPCORO_OS_WINNT
# include <cppcoro/io_service.hpp>
# include <thread>
# include <chrono>
#endif

#include <ostream>
#include "doctest/doctest.h"

DOCTEST_TEST_SUITE_BEGIN("multi_producer_sequencer");

using namespace cppcoro;

#if CPPCORO_OS_WINNT

DOCTEST_TEST_CASE("multi-threaded usage single consumer")
{
	io_service ioSvc;

	// Spin up 3 I/O threads
	std::thread ioThread1{ [&] { ioSvc.process_events(); } };
	auto joinOnExit1 = on_scope_exit([&] { ioThread1.join(); });
	auto stopOnExit1 = on_scope_exit([&] { ioSvc.stop(); });
	std::thread ioThread2{ [&] { ioSvc.process_events(); } };
	auto joinOnExit2 = on_scope_exit([&] { ioThread2.join(); });
	auto stopOnExit2 = std::move(stopOnExit1);
	std::thread ioThread3{ [&] { ioSvc.process_events(); } };
	auto joinOnExit3 = on_scope_exit([&] { ioThread3.join(); });
	auto stopOnExit3 = std::move(stopOnExit2);

	constexpr std::size_t bufferSize = 256;

	sequence_barrier<std::size_t> readBarrier;
	multi_producer_sequencer<std::size_t> sequencer(readBarrier, bufferSize);

	constexpr std::size_t iterationCount = 1'000'000;

	std::uint64_t buffer[bufferSize];

	const auto producer = [&]() -> task<>
	{
		co_await ioSvc.schedule();

		constexpr std::size_t maxBatchSize = 10;

		std::size_t i = 0;
		while (i < iterationCount)
		{
			const std::size_t batchSize = std::min(maxBatchSize, iterationCount - i);
			auto sequences = co_await sequencer.claim_up_to(batchSize);
			for (auto seq : sequences)
			{
				buffer[seq % bufferSize] = ++i;
			}
			sequencer.publish(sequences);
		}

		auto finalSeq = co_await sequencer.claim_one();
		buffer[finalSeq % bufferSize] = 0;
		sequencer.publish(finalSeq);
	};

	const auto consumer = [&](std::uint32_t producerCount) -> task<std::uint64_t>
	{
		co_await ioSvc.schedule();

		std::uint64_t sum = 0;

		std::uint32_t endCount = 0;
		std::size_t nextToRead = 0;
		do
		{
			std::size_t available = sequencer.last_published_after(nextToRead - 1);
			if (sequence_traits<std::size_t>::precedes(available, nextToRead))
			{
				available = co_await sequencer.wait_until_published(nextToRead, nextToRead - 1);
				co_await ioSvc.schedule();
			}

			do
			{
				const auto& value = buffer[nextToRead % bufferSize];
				sum += value;

				// Zero value is sentinel that indicates the end of one of the streams.
				const bool isEndOfStream = value == 0;
				endCount += isEndOfStream ? 1 : 0;
			} while (nextToRead++ != available);

			// Notify that we've finished processing up to 'available'.
			readBarrier.publish(available);
		} while (endCount < producerCount);

		co_return sum;
	};

	// Allow time for threads to start up.
	using namespace std::chrono_literals;
	std::this_thread::sleep_for(1ms);

	auto startTime = std::chrono::high_resolution_clock::now();

	constexpr std::uint32_t producerCount = 2;
	auto result = std::get<0>(sync_wait(when_all(consumer(producerCount), producer() , producer())));

	auto endTime = std::chrono::high_resolution_clock::now();

	auto totalTimeInNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count();

	MESSAGE(
		"Producers = " << producerCount
		<< ", MessagesPerProducer = " << iterationCount
		<< ", TotalTime = " << totalTimeInNs/1000 << "us"
		<< ", TimePerMessage = " << totalTimeInNs/double(iterationCount * producerCount) << "ns");

	constexpr std::uint64_t expectedResult =
		producerCount * std::uint64_t(iterationCount) * std::uint64_t(iterationCount + 1) / 2;

	CHECK(result == expectedResult);
}

#endif

DOCTEST_TEST_SUITE_END();
