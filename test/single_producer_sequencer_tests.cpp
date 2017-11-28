///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/single_producer_sequencer.hpp>

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
#endif

#include <ostream>
#include "doctest/doctest.h"

DOCTEST_TEST_SUITE_BEGIN("single_producer_sequencer");

using namespace cppcoro;

#if CPPCORO_OS_WINNT

DOCTEST_TEST_CASE("multi-threaded usage single consumer")
{
	io_service ioSvc;

	// Spin up 2 io threads
	std::thread ioThread1{ [&] { ioSvc.process_events(); } };
	auto joinOnExit1 = on_scope_exit([&] { ioThread1.join(); });
	auto stopOnExit1 = on_scope_exit([&] { ioSvc.stop(); });
	std::thread ioThread2{ [&] { ioSvc.process_events(); } };
	auto joinOnExit2 = on_scope_exit([&] { ioThread2.join(); });
	auto stopOnExit2 = std::move(stopOnExit1);

	constexpr std::size_t bufferSize = 256;

	sequence_barrier<std::size_t> readBarrier;
	single_producer_sequencer<std::size_t> sequencer(readBarrier, bufferSize);

	constexpr std::size_t iterationCount = 1'000'000;

	std::uint64_t buffer[bufferSize];

	auto[result, dummy] = sync_wait(when_all(
		[&]() -> task<std::uint64_t>
	{
		// Consumer
		co_await ioSvc.schedule();

		std::uint64_t sum = 0;

		bool reachedEnd = false;
		std::size_t nextToRead = 0;
		do
		{
			std::size_t available = sequencer.last_published();
			if (sequence_traits<std::size_t>::precedes(available, nextToRead))
			{
				available = co_await sequencer.wait_until_published(nextToRead);
				co_await ioSvc.schedule();
			}

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
			sequencer.publish(sequences.back());
		}

		auto finalSeq = co_await sequencer.claim_one();
		buffer[finalSeq % bufferSize] = 0;
		sequencer.publish(finalSeq);
	}()));

	// Suppress unused variable warning.
	(void)dummy;

	constexpr std::uint64_t expectedResult =
		std::uint64_t(iterationCount) * std::uint64_t(iterationCount + 1) / 2;

	CHECK(result == expectedResult);
}

#endif

DOCTEST_TEST_SUITE_END();
