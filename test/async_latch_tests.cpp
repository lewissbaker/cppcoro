///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/async_latch.hpp>
#include <cppcoro/single_consumer_event.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all_ready.hpp>
#include <cppcoro/sync_wait.hpp>

#include <ostream>
#include "doctest/doctest.h"

TEST_SUITE_BEGIN("async_latch");

using namespace cppcoro;

TEST_CASE("latch constructed with zero count is initially ready")
{
	async_latch latch(0);
	CHECK(latch.is_ready());
}

TEST_CASE("latch constructed with negative count is initially ready")
{
	async_latch latch(-3);
	CHECK(latch.is_ready());
}

TEST_CASE("count_down and is_ready")
{
	async_latch latch(3);
	CHECK(!latch.is_ready());
	latch.count_down();
	CHECK(!latch.is_ready());
	latch.count_down();
	CHECK(!latch.is_ready());
	latch.count_down();
	CHECK(latch.is_ready());
}

TEST_CASE("count_down by n")
{
	async_latch latch(5);
	latch.count_down(3);
	CHECK(!latch.is_ready());
	latch.count_down(2);
	CHECK(latch.is_ready());
}

TEST_CASE("single awaiter")
{
	async_latch latch(2);
	bool after = false;
	sync_wait(when_all_ready(
		[&]() -> task<>
		{
			co_await latch;
			after = true;
		}(),
		[&]() -> task<>
		{
			CHECK(!after);
			latch.count_down();
			CHECK(!after);
			latch.count_down();
			CHECK(after);
			co_return;
		}()
	));
}

TEST_CASE("multiple awaiters")
{
	async_latch latch(2);
	bool after1 = false;
	bool after2 = false;
	bool after3 = false;
	sync_wait(when_all_ready(
		[&]() -> task<>
		{
			co_await latch;
			after1 = true;
		}(),
		[&]() -> task<>
		{
			co_await latch;
			after2 = true;
		}(),
		[&]() -> task<>
		{
			co_await latch;
			after3 = true;
		}(),
		[&]() -> task<>
		{
			CHECK(!after1);
			CHECK(!after2);
			CHECK(!after3);
			latch.count_down();
			CHECK(!after1);
			CHECK(!after2);
			CHECK(!after3);
			latch.count_down();
			CHECK(after1);
			CHECK(after2);
			CHECK(after3);
			co_return;
		}()));
}

TEST_SUITE_END();
