///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/async_stream.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/fmap.hpp>
#include <cppcoro/subscribable.hpp>
#include <cppcoro/consume.hpp>

#include <iostream>
#include <functional>

#include "doctest/doctest.h"

using namespace cppcoro;

TEST_SUITE_BEGIN("async_stream");

TEST_CASE("consume async_stream")
{
	auto subscribable = make_subscribable(
		[]() -> async_stream_subscription<int>
		{
			bool shouldContinue = co_yield 1;
			if (!shouldContinue) co_return;
			shouldContinue = co_yield 2;
			if (!shouldContinue) co_return;
		});

	int result = sync_wait(consume(subscribable, [](async_stream<int> stream) -> task<int>
	{
		int sum = 0;
		for co_await(int value : stream)
		{
			sum += value;
		}
		co_return sum;
	}));

	CHECK(result == 3);
}

TEST_CASE("producer exiting early on destruction of stream")
{
	int lastProduced = -1;
	bool ranFinalisation = false;

	auto subscribable = make_subscribable(
		[&]() -> async_stream_subscription<int>
		{
			for (int i = 0; i < 5; ++i)
			{
				lastProduced = i;
				bool produceMore = co_yield i;
				if (!produceMore) break;
			}

			ranFinalisation = true;
		});

	sync_wait(consume(subscribable, [](async_stream<int> stream) -> task<>
	{
		// TRICKY: Need to move parameter to local var here to ensure it is destructed when the
		// coroutine runs to completion. Normally, the destructor of parameters won't run until
		// the coroutine is destroyed. However, the coroutine won't be destroyed until the
		// entire consume operation completes, which includes waiting until the producer coroutine
		// runs to completion. However, the producer coroutine isn't going to run to completion
		// unless we either consume it completely or detach from the stream, causing a deadlock.
		auto localStream = std::move(stream);
		for co_await(int value : localStream)
		{
			// Don't consume any more values once we get to '3'.
			if (value == 3) break;
		}
	}));

	CHECK(lastProduced == 3);
	CHECK(ranFinalisation);
}

template<typename COUNT, typename SUBSCRIBABLE>
auto take(COUNT n, SUBSCRIBABLE&& s)
{
	return make_subscribable(
		[n, s = std::forward<SUBSCRIBABLE>(s)]
		{
			auto[sourceStream, sourceTask] = s.subscribe();

			using value_type = typename std::remove_reference_t<decltype(sourceStream)>::value_type;

			auto[outputStream, outputTask] = [](COUNT n, auto stream) -> async_stream_subscription<value_type>
			{
				auto localStream = std::move(stream);
				if (n > 0)
				{
					for co_await(auto&& value : localStream)
					{
						bool produceMore = co_yield value;
						if (!produceMore) break;
						if (--n == 0) break;
					}
				}
			}(n, std::move(sourceStream));

			return std::make_tuple(
				std::move(outputStream),
				when_all(std::move(sourceTask), std::move(outputTask)) | fmap([](auto) {}));
		});
}

TEST_CASE("take(5)")
{
	auto subscribable = take(5, make_subscribable([]() -> async_stream_subscription<int>
	{
		for (int i = 0; i < 10; ++i)
		{
			bool produceMore = co_yield i;
			if (!produceMore) break;
		}
	}));

	auto values = sync_wait(consume(subscribable, [](async_stream<int> stream) -> task<std::vector<int>>
	{
		auto localStream = std::move(stream);

		std::vector<int> values;
		for co_await(int value : localStream)
		{
			values.push_back(value);
		}
		return std::move(values);
	}));

	CHECK(values.size() == 5);
	CHECK(values[0] == 0);
	CHECK(values[4] == 4);
}

TEST_SUITE_END();
