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
			co_yield 1;
			co_yield 2;
			co_return;
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

TEST_SUITE_END();
