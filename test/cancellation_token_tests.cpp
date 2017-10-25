///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/cancellation_token.hpp>
#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/cancellation_registration.hpp>
#include <cppcoro/operation_cancelled.hpp>

#include <thread>

#include <ostream>
#include "doctest/doctest.h"

TEST_SUITE_BEGIN("cancellation_token tests");

TEST_CASE("default cancellation_token is not cancellable")
{
	cppcoro::cancellation_token t;
	CHECK(!t.is_cancellation_requested());
	CHECK(!t.can_be_cancelled());
}

TEST_CASE("calling request_cancellation on cancellation_source updates cancellation_token")
{
	cppcoro::cancellation_source s;
	cppcoro::cancellation_token t = s.token();
	CHECK(t.can_be_cancelled());
	CHECK(!t.is_cancellation_requested());
	s.request_cancellation();
	CHECK(t.is_cancellation_requested());
	CHECK(t.can_be_cancelled());
}

TEST_CASE("cancellation_token can't be cancelled when last cancellation_source destructed")
{
	cppcoro::cancellation_token t;
	{
		cppcoro::cancellation_source s;
		t = s.token();
		CHECK(t.can_be_cancelled());
	}

	CHECK(!t.can_be_cancelled());
}

TEST_CASE("cancelation_token can be cancelled when last cancellation_source destructed if cancellation already requested")
{
	cppcoro::cancellation_token t;
	{
		cppcoro::cancellation_source s;
		t = s.token();
		CHECK(t.can_be_cancelled());
		s.request_cancellation();
	}

	CHECK(t.can_be_cancelled());
	CHECK(t.is_cancellation_requested());
}

TEST_CASE("cancellation_registration when cancellation not yet requested")
{
	cppcoro::cancellation_source s;

	bool callbackExecuted = false;
	{
		cppcoro::cancellation_registration callbackRegistration(
			s.token(),
			[&] { callbackExecuted = true; });
	}

	CHECK(!callbackExecuted);

	{
		cppcoro::cancellation_registration callbackRegistration(
			s.token(),
			[&] { callbackExecuted = true; });

		CHECK(!callbackExecuted);

		s.request_cancellation();

		CHECK(callbackExecuted);
	}
}

TEST_CASE("throw_if_cancellation_requested")
{
	cppcoro::cancellation_source s;
	cppcoro::cancellation_token t = s.token();

	CHECK_NOTHROW(t.throw_if_cancellation_requested());

	s.request_cancellation();

	CHECK_THROWS_AS(t.throw_if_cancellation_requested(), const cppcoro::operation_cancelled&);
}

TEST_CASE("cancellation_registration called immediately when cancellation already requested")
{
	cppcoro::cancellation_source s;
	s.request_cancellation();

	bool executed = false;
	cppcoro::cancellation_registration r{ s.token(), [&] { executed = true; } };
	CHECK(executed);
}

TEST_CASE("register many callbacks"
	* doctest::description{
	"this checks the code-path that allocates the next chunk of entries "
	"in the internal data-structres, which occurs on 17th callback" })
{
	cppcoro::cancellation_source s;
	auto t = s.token();

	int callbackExecutionCount = 0;
	auto callback = [&] { ++callbackExecutionCount; };

	// Allocate enough to require a second chunk to be allocated.
	cppcoro::cancellation_registration r1{ t, callback };
	cppcoro::cancellation_registration r2{ t, callback };
	cppcoro::cancellation_registration r3{ t, callback };
	cppcoro::cancellation_registration r4{ t, callback };
	cppcoro::cancellation_registration r5{ t, callback };
	cppcoro::cancellation_registration r6{ t, callback };
	cppcoro::cancellation_registration r7{ t, callback };
	cppcoro::cancellation_registration r8{ t, callback };
	cppcoro::cancellation_registration r9{ t, callback };
	cppcoro::cancellation_registration r10{ t, callback };
	cppcoro::cancellation_registration r11{ t, callback };
	cppcoro::cancellation_registration r12{ t, callback };
	cppcoro::cancellation_registration r13{ t, callback };
	cppcoro::cancellation_registration r14{ t, callback };
	cppcoro::cancellation_registration r15{ t, callback };
	cppcoro::cancellation_registration r16{ t, callback };
	cppcoro::cancellation_registration r17{ t, callback };
	cppcoro::cancellation_registration r18{ t, callback };

	s.request_cancellation();

	CHECK(callbackExecutionCount == 18);
}

TEST_CASE("concurrent registration and cancellation")
{
	// Just check this runs and terminates without crashing.
	for (int i = 0; i < 100; ++i)
	{
		cppcoro::cancellation_source source;

		std::thread waiter1{ [token = source.token()]
		{
			std::atomic<bool> cancelled = false;
			while (!cancelled)
			{
				cppcoro::cancellation_registration registration{ token, [&]
				{
					cancelled = true;
				} };

				cppcoro::cancellation_registration reg0{ token, [] {} };
				cppcoro::cancellation_registration reg1{ token, [] {} };
				cppcoro::cancellation_registration reg2{ token, [] {} };
				cppcoro::cancellation_registration reg3{ token, [] {} };
				cppcoro::cancellation_registration reg4{ token, [] {} };
				cppcoro::cancellation_registration reg5{ token, [] {} };
				cppcoro::cancellation_registration reg6{ token, [] {} };
				cppcoro::cancellation_registration reg7{ token, [] {} };
				cppcoro::cancellation_registration reg8{ token, [] {} };
				cppcoro::cancellation_registration reg9{ token, [] {} };
				cppcoro::cancellation_registration reg10{ token, [] {} };
				cppcoro::cancellation_registration reg11{ token, [] {} };
				cppcoro::cancellation_registration reg12{ token, [] {} };
				cppcoro::cancellation_registration reg13{ token, [] {} };
				cppcoro::cancellation_registration reg14{ token, [] {} };
				cppcoro::cancellation_registration reg15{ token, [] {} };
				cppcoro::cancellation_registration reg17{ token, [] {} };

				std::this_thread::yield();
			}
		} };

		std::thread waiter2{ [token = source.token()]
		{
			std::atomic<bool> cancelled = false;
			while (!cancelled)
			{
				cppcoro::cancellation_registration registration{ token, [&]
				{
					cancelled = true;
				} };

				cppcoro::cancellation_registration reg0{ token, [] {} };
				cppcoro::cancellation_registration reg1{ token, [] {} };
				cppcoro::cancellation_registration reg2{ token, [] {} };
				cppcoro::cancellation_registration reg3{ token, [] {} };
				cppcoro::cancellation_registration reg4{ token, [] {} };
				cppcoro::cancellation_registration reg5{ token, [] {} };
				cppcoro::cancellation_registration reg6{ token, [] {} };
				cppcoro::cancellation_registration reg7{ token, [] {} };
				cppcoro::cancellation_registration reg8{ token, [] {} };
				cppcoro::cancellation_registration reg9{ token, [] {} };
				cppcoro::cancellation_registration reg10{ token, [] {} };
				cppcoro::cancellation_registration reg11{ token, [] {} };
				cppcoro::cancellation_registration reg12{ token, [] {} };
				cppcoro::cancellation_registration reg13{ token, [] {} };
				cppcoro::cancellation_registration reg14{ token, [] {} };
				cppcoro::cancellation_registration reg15{ token, [] {} };
				cppcoro::cancellation_registration reg16{ token, [] {} };

				std::this_thread::yield();
			}
		} };

		std::thread waiter3{ [token = source.token()]
		{
			std::atomic<bool> cancelled = false;
			while (!cancelled)
			{
				cppcoro::cancellation_registration registration{ token, [&]
				{
					cancelled = true;
				} };

				cppcoro::cancellation_registration reg0{ token, [] {} };
				cppcoro::cancellation_registration reg1{ token, [] {} };
				cppcoro::cancellation_registration reg2{ token, [] {} };
				cppcoro::cancellation_registration reg3{ token, [] {} };
				cppcoro::cancellation_registration reg4{ token, [] {} };
				cppcoro::cancellation_registration reg5{ token, [] {} };
				cppcoro::cancellation_registration reg6{ token, [] {} };
				cppcoro::cancellation_registration reg7{ token, [] {} };
				cppcoro::cancellation_registration reg8{ token, [] {} };
				cppcoro::cancellation_registration reg9{ token, [] {} };
				cppcoro::cancellation_registration reg10{ token, [] {} };
				cppcoro::cancellation_registration reg11{ token, [] {} };
				cppcoro::cancellation_registration reg12{ token, [] {} };
				cppcoro::cancellation_registration reg13{ token, [] {} };
				cppcoro::cancellation_registration reg14{ token, [] {} };
				cppcoro::cancellation_registration reg15{ token, [] {} };
				cppcoro::cancellation_registration reg16{ token, [] {} };

				std::this_thread::yield();
			}
		} };

		std::thread canceller{ [&source]
		{
			source.request_cancellation();
		} };

		canceller.join();
		waiter1.join();
		waiter2.join();
		waiter3.join();
	}
}

TEST_CASE("cancellation registration single-threaded performance")
{
	struct batch
	{
		batch(cppcoro::cancellation_token t)
			: r0(t, [] {})
			, r1(t, [] {})
			, r2(t, [] {})
			, r3(t, [] {})
			, r4(t, [] {})
			, r5(t, [] {})
			, r6(t, [] {})
			, r7(t, [] {})
			, r8(t, [] {})
			, r9(t, [] {})
		{}

		cppcoro::cancellation_registration r0;
		cppcoro::cancellation_registration r1;
		cppcoro::cancellation_registration r2;
		cppcoro::cancellation_registration r3;
		cppcoro::cancellation_registration r4;
		cppcoro::cancellation_registration r5;
		cppcoro::cancellation_registration r6;
		cppcoro::cancellation_registration r7;
		cppcoro::cancellation_registration r8;
		cppcoro::cancellation_registration r9;
	};

	cppcoro::cancellation_source s;

	constexpr int iterationCount = 100'000;

	auto start = std::chrono::high_resolution_clock::now();

	for (int i = 0; i < iterationCount; ++i)
	{
		cppcoro::cancellation_registration r{ s.token(), [] {} };
	}

	auto end = std::chrono::high_resolution_clock::now();

	auto time1 = end - start;

	start = end;

	for (int i = 0; i < iterationCount; ++i)
	{
		batch b{ s.token() };
	}

	end = std::chrono::high_resolution_clock::now();

	auto time2 = end - start;

	start = end;

	for (int i = 0; i < iterationCount; ++i)
	{
		batch b0{ s.token() };
		batch b1{ s.token() };
		batch b2{ s.token() };
		batch b3{ s.token() };
		batch b4{ s.token() };
	}

	end = std::chrono::high_resolution_clock::now();

	auto time3 = end - start;

	auto report = [](const char* label, auto time, std::uint64_t count)
	{
		auto us = std::chrono::duration_cast<std::chrono::microseconds>(time).count();
		MESSAGE(label << " took " << us << "us (" << (1000.0 * us / count) << " ns/item)");
	};

	report("Individual", time1, iterationCount);
	report("Batch10", time2, 10 * iterationCount);
	report("Batch50", time3, 50 * iterationCount);
}

TEST_SUITE_END();
