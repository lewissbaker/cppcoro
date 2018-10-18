///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/async_generator.hpp>
#include <cppcoro/single_consumer_event.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>

#include <ostream>
#include "doctest/doctest.h"

TEST_SUITE_BEGIN("async_generator");

TEST_CASE("default-constructed async_generator is an empty sequence")
{
	cppcoro::sync_wait([]() -> cppcoro::task<>
	{
		// Iterating over default-constructed async_generator just
		// gives an empty sequence.
		cppcoro::async_generator<int> g;
		auto it = co_await g.begin();
		CHECK(it == g.end());
	}());
}

TEST_CASE("async_generator doesn't start if begin() not called")
{
	bool startedExecution = false;
	{
		auto gen = [&]() -> cppcoro::async_generator<int>
		{
			startedExecution = true;
			co_yield 1;
		}();
		CHECK(!startedExecution);
	}
	CHECK(!startedExecution);
}

TEST_CASE("enumerate sequence of 1 value")
{
	cppcoro::sync_wait([]() -> cppcoro::task<>
	{
		bool startedExecution = false;
		auto makeGenerator = [&]() -> cppcoro::async_generator<std::uint32_t>
		{
			startedExecution = true;
			co_yield 1;
		};

		auto gen = makeGenerator();

		CHECK(!startedExecution);

		auto it = co_await gen.begin();

		CHECK(startedExecution);
		CHECK(it != gen.end());
		CHECK(*it == 1u);
		CHECK(co_await ++it == gen.end());
	}());
}

TEST_CASE("enumerate sequence of multiple values")
{
	cppcoro::sync_wait([]() -> cppcoro::task<>
	{
		bool startedExecution = false;
		auto makeGenerator = [&]() -> cppcoro::async_generator<std::uint32_t>
		{
			startedExecution = true;
			co_yield 1;
			co_yield 2;
			co_yield 3;
		};

		auto gen = makeGenerator();

		CHECK(!startedExecution);

		auto it = co_await gen.begin();

		CHECK(startedExecution);

		CHECK(it != gen.end());
		CHECK(*it == 1u);

		CHECK(co_await ++it != gen.end());
		CHECK(*it == 2u);

		CHECK(co_await ++it != gen.end());
		CHECK(*it == 3u);

		CHECK(co_await ++it == gen.end());
	}());
}

namespace
{
	class set_to_true_on_destruction
	{
	public:

		set_to_true_on_destruction(bool* value)
			: m_value(value)
		{}

		set_to_true_on_destruction(set_to_true_on_destruction&& other)
			: m_value(other.m_value)
		{
			other.m_value = nullptr;
		}

		~set_to_true_on_destruction()
		{
			if (m_value != nullptr)
			{
				*m_value = true;
			}
		}

		set_to_true_on_destruction(const set_to_true_on_destruction&) = delete;
		set_to_true_on_destruction& operator=(const set_to_true_on_destruction&) = delete;

	private:

		bool* m_value;
	};
}

TEST_CASE("destructors of values in scope are called when async_generator destructed early")
{
	cppcoro::sync_wait([]() -> cppcoro::task<>
	{
		bool aDestructed = false;
		bool bDestructed = false;

		auto makeGenerator = [&](set_to_true_on_destruction a) -> cppcoro::async_generator<std::uint32_t>
		{
			set_to_true_on_destruction b(&bDestructed);
			co_yield 1;
			co_yield 2;
		};

		{
			auto gen = makeGenerator(&aDestructed);

			CHECK(!aDestructed);
			CHECK(!bDestructed);

			auto it = co_await gen.begin();
			CHECK(!aDestructed);
			CHECK(!bDestructed);
			CHECK(*it == 1u);
		}

		CHECK(aDestructed);
		CHECK(bDestructed);
	}());
}

TEST_CASE("async producer with async consumer"
	* doctest::description{
		"This test tries to cover the different state-transition code-paths\n"
		"- consumer resuming producer and producer completing asynchronously\n"
		"- producer resuming consumer and consumer requesting next value synchronously\n"
		"- producer resuming consumer and consumer requesting next value asynchronously" })
{
#if defined(_MSC_VER) && _MSC_FULL_VER <= 191025224 && defined(CPPCORO_RELEASE_OPTIMISED)
	FAST_WARN_UNARY_FALSE("MSVC has a known codegen bug under optimised builds, skipping");
	return;
#endif

	cppcoro::single_consumer_event p1;
	cppcoro::single_consumer_event p2;
	cppcoro::single_consumer_event p3;
	cppcoro::single_consumer_event c1;

	auto produce = [&]() -> cppcoro::async_generator<std::uint32_t>
	{
		co_await p1;
		co_yield 1;
		co_await p2;
		co_yield 2;
		co_await p3;
	};

	bool consumerFinished = false;

	auto consume = [&]() -> cppcoro::task<>
	{
		auto generator = produce();
		auto it = co_await generator.begin();
		CHECK(*it == 1u);
		(void)co_await ++it;
		CHECK(*it == 2u);
		co_await c1;
		(void)co_await ++it;
		CHECK(it == generator.end());
		consumerFinished = true;
	};

	auto unblock = [&]() -> cppcoro::task<>
	{
		p1.set();
		p2.set();
		c1.set();
		CHECK(!consumerFinished);
		p3.set();
		CHECK(consumerFinished);
		co_return;
	};

	cppcoro::sync_wait(cppcoro::when_all_ready(consume(), unblock()));
}

TEST_CASE("exception thrown before first yield is rethrown from begin operation")
{
	class TestException {};
	auto gen = [](bool shouldThrow) -> cppcoro::async_generator<std::uint32_t>
	{
		if (shouldThrow)
		{
			throw TestException();
		}
		co_yield 1;
	}(true);

	cppcoro::sync_wait([&]() -> cppcoro::task<>
	{
		CHECK_THROWS_AS(co_await gen.begin(), const TestException&);
	}());
}

TEST_CASE("exception thrown after first yield is rethrown from increment operator")
{
	class TestException {};
	auto gen = [](bool shouldThrow) -> cppcoro::async_generator<std::uint32_t>
	{
		co_yield 1;
		if (shouldThrow)
		{
			throw TestException();
		}
	}(true);

	cppcoro::sync_wait([&]() -> cppcoro::task<>
	{
		auto it = co_await gen.begin();
		CHECK(*it == 1u);
		CHECK_THROWS_AS(co_await ++it, const TestException&);
		CHECK(it == gen.end());
	}());
}

TEST_CASE("large number of synchronous completions doesn't result in stack-overflow")
{

	auto makeSequence = [](cppcoro::single_consumer_event& event) -> cppcoro::async_generator<std::uint32_t>
	{
		for (std::uint32_t i = 0; i < 1'000'000u; ++i)
		{
			if (i == 500'000u) co_await event;
			co_yield i;
		}
	};

	auto consumer = [](cppcoro::async_generator<std::uint32_t> sequence) -> cppcoro::task<>
	{
		std::uint32_t expected = 0;
		for co_await(std::uint32_t i : sequence)
		{
			CHECK(i == expected++);
		}

		CHECK(expected == 1'000'000u);
	};

	auto unblocker = [](cppcoro::single_consumer_event& event) -> cppcoro::task<>
	{
		// Should have processed the first 500'000 elements synchronously with consumer driving
		// iteraction before producer suspends and thus consumer suspends.
		// Then we resume producer in call to set() below and it continues processing remaining
		// 500'000 elements, this time with producer driving the interaction.

		event.set();

		co_return;
	};

	cppcoro::single_consumer_event event;

	cppcoro::sync_wait(
		cppcoro::when_all_ready(
			consumer(makeSequence(event)),
			unblocker(event)));
}

TEST_CASE("fmap")
{
	using cppcoro::async_generator;
	using cppcoro::fmap;

	auto iota = [](int count) -> async_generator<int>
	{
		for (int i = 0; i < count; ++i)
		{
			co_yield i;
		}
	};

	auto squares = iota(5) | fmap([](auto x) { return x * x; });

	cppcoro::sync_wait([&]() -> cppcoro::task<>
	{
		auto it = co_await squares.begin();
		CHECK(*it == 0);
		CHECK(*co_await ++it == 1);
		CHECK(*co_await ++it == 4);
		CHECK(*co_await ++it == 9);
		CHECK(*co_await ++it == 16);
		CHECK(co_await ++it == squares.end());
	}());
}

TEST_SUITE_END();
