///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/schedule_on.hpp>
#include <cppcoro/resume_on.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all_ready.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/fmap.hpp>

#include "io_service_fixture.hpp"

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("schedule/resume_on");

#if CPPCORO_OS_WINNT
#define THREAD_ID std::thread::id
#define GET_THIS_THREAD_ID std::this_thread::get_id()
#endif

#if CPPCORO_OS_LINUX
#define THREAD_ID unsigned long long
#define GET_THIS_THREAD_ID get_thread_id()

#include <sstream>

static unsigned long long get_thread_id()
{
  unsigned long long id;
  std::stringstream ss;
  ss << std::this_thread::get_id();
  id = std::stoull(ss.str());
  return id;
}
#endif

TEST_CASE_FIXTURE(io_service_fixture, "schedule_on task<> function")
{
	auto mainThreadId = GET_THIS_THREAD_ID;

	THREAD_ID ioThreadId;

	auto start = [&]() -> cppcoro::task<>
	{
		ioThreadId = GET_THIS_THREAD_ID;
		CHECK(ioThreadId != mainThreadId);
		co_return;
	};

	cppcoro::sync_wait([&]() -> cppcoro::task<>
	{
		CHECK(GET_THIS_THREAD_ID == mainThreadId);

		co_await schedule_on(io_service(), start());

		CHECK(GET_THIS_THREAD_ID == ioThreadId);
	}());
}

TEST_CASE_FIXTURE(io_service_fixture, "schedule_on async_generator<> function")
{
	auto mainThreadId = GET_THIS_THREAD_ID;

	THREAD_ID ioThreadId;

	auto makeSequence = [&]() -> cppcoro::async_generator<int>
	{
		ioThreadId = GET_THIS_THREAD_ID;
		CHECK(ioThreadId != mainThreadId);

		co_yield 1;

		CHECK(GET_THIS_THREAD_ID == ioThreadId);

		co_yield 2;

		CHECK(GET_THIS_THREAD_ID == ioThreadId);

		co_yield 3;

		CHECK(GET_THIS_THREAD_ID == ioThreadId);

		co_return;
	};

	cppcoro::io_service otherIoService;

	cppcoro::sync_wait(cppcoro::when_all_ready(
		[&]() -> cppcoro::task<>
	{
		CHECK(GET_THIS_THREAD_ID == mainThreadId);

		auto seq = schedule_on(io_service(), makeSequence());

		int expected = 1;
		for co_await(int value : seq)
		{
			CHECK(value == expected++);

			// Transfer exection back to main thread before
			// awaiting next item in the loop to chck that
			// the generator is resumed on io_service() thread.
			co_await otherIoService.schedule();
		}

		otherIoService.stop();
	}(),
		[&]() -> cppcoro::task<>
	{
		otherIoService.process_events();
		co_return;
	}()));
}

TEST_CASE_FIXTURE(io_service_fixture, "resume_on task<> function")
{
	auto mainThreadId = GET_THIS_THREAD_ID;

	auto start = [&]() -> cppcoro::task<>
	{
		CHECK(GET_THIS_THREAD_ID == mainThreadId);
		co_return;
	};

	cppcoro::sync_wait([&]() -> cppcoro::task<>
	{
		CHECK(GET_THIS_THREAD_ID == mainThreadId);

		co_await resume_on(io_service(), start());

		CHECK(GET_THIS_THREAD_ID != mainThreadId);
	}());
}

TEST_CASE_FIXTURE(io_service_fixture, "resume_on async_generator<> function")
{
	auto mainThreadId = GET_THIS_THREAD_ID;

	THREAD_ID ioThreadId;

	auto makeSequence = [&]() -> cppcoro::async_generator<int>
	{
		co_await io_service().schedule();

		ioThreadId = GET_THIS_THREAD_ID;

		CHECK(ioThreadId != mainThreadId);

		co_yield 1;

		co_yield 2;

		co_await io_service().schedule();

		co_yield 3;

		co_await io_service().schedule();

		co_return;
	};

	cppcoro::io_service otherIoService;

	cppcoro::sync_wait(cppcoro::when_all_ready(
		[&]() -> cppcoro::task<>
	{
		auto stopOnExit = cppcoro::on_scope_exit([&] { otherIoService.stop(); });

		CHECK(GET_THIS_THREAD_ID == mainThreadId);

		auto seq = resume_on(otherIoService, makeSequence());

		int expected = 1;
		for co_await(int value : seq)
		{
			// Every time we receive a value it should be on our requested
			// scheduler (ie. main thread)
			CHECK(GET_THIS_THREAD_ID == mainThreadId);
			CHECK(value == expected++);

			// Occasionally transfer execution to a different thread before
			// awaiting next element.
			if (value == 2)
			{
				co_await io_service().schedule();
			}
		}

		otherIoService.stop();
	}(),
		[&]() -> cppcoro::task<>
	{
		otherIoService.process_events();
		co_return;
	}()));
}

TEST_CASE_FIXTURE(io_service_fixture, "schedule_on task<> pipe syntax")
{
	auto mainThreadId = GET_THIS_THREAD_ID;

	auto makeTask = [&]() -> cppcoro::task<int>
	{
		CHECK(GET_THIS_THREAD_ID != mainThreadId);
		co_return 123;
	};

	auto triple = [&](int x)
	{
		CHECK(GET_THIS_THREAD_ID != mainThreadId);
		return x * 3;
	};

	CHECK(cppcoro::sync_wait(makeTask() | schedule_on(io_service())) == 123);

	// Shouldn't matter where in sequence schedule_on() appears since it applies
	// at the start of the pipeline (ie. before first task starts).
	CHECK(cppcoro::sync_wait(makeTask() | schedule_on(io_service()) | cppcoro::fmap(triple)) == 369);
	CHECK(cppcoro::sync_wait(makeTask() | cppcoro::fmap(triple) | schedule_on(io_service())) == 369);
}

TEST_CASE_FIXTURE(io_service_fixture, "resume_on task<> pipe syntax")
{
	auto mainThreadId = GET_THIS_THREAD_ID;

	auto makeTask = [&]() -> cppcoro::task<int>
	{
		CHECK(GET_THIS_THREAD_ID == mainThreadId);
		co_return 123;
	};

	cppcoro::sync_wait([&]() -> cppcoro::task<>
	{
		cppcoro::task<int> t = makeTask() | cppcoro::resume_on(io_service());
		CHECK(co_await t == 123);
		CHECK(GET_THIS_THREAD_ID != mainThreadId);
	}());
}

TEST_CASE_FIXTURE(io_service_fixture, "resume_on task<> pipe syntax multiple uses")
{
	auto mainThreadId = GET_THIS_THREAD_ID;

	auto makeTask = [&]() -> cppcoro::task<int>
	{
		CHECK(GET_THIS_THREAD_ID == mainThreadId);
		co_return 123;
	};

	auto triple = [&](int x)
	{
		CHECK(GET_THIS_THREAD_ID != mainThreadId);
		return x * 3;
	};

	cppcoro::io_service otherIoService;

	cppcoro::sync_wait(cppcoro::when_all_ready(
		[&]() -> cppcoro::task<>
	{
		auto stopOnExit = cppcoro::on_scope_exit([&] { otherIoService.stop(); });

		CHECK(GET_THIS_THREAD_ID == mainThreadId);

		cppcoro::task<int> t =
			makeTask()
			| cppcoro::resume_on(io_service())
			| cppcoro::fmap(triple)
			| cppcoro::resume_on(otherIoService);

		CHECK(co_await t == 369);

		CHECK(GET_THIS_THREAD_ID == mainThreadId);
	}(),
		[&]() -> cppcoro::task<>
	{
		otherIoService.process_events();
		co_return;
	}()));
}

TEST_SUITE_END();
