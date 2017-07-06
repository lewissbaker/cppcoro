///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/async_generator.hpp>
#include <cppcoro/single_consumer_event.hpp>
#include <cppcoro/task.hpp>

#include "doctest/doctest.h"

TEST_CASE("default-constructed async_generator is an empty sequence")
{
	[]() -> cppcoro::task<>
	{
		// Iterating over default-constructed async_generator just
		// gives an empty sequence.
		cppcoro::async_generator<int> g;
		auto it = co_await g.begin();
		CHECK(it == g.end());
	}();
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
	bool startedExecution = false;
	auto gen = [&]() -> cppcoro::async_generator<std::uint32_t>
	{
		startedExecution = true;
		co_yield 1;
	}();

	CHECK(!startedExecution);

	auto itAwaitable = gen.begin();
	CHECK(startedExecution);
	CHECK(itAwaitable.await_ready());
	auto it = itAwaitable.await_resume();
	CHECK(it != gen.end());
	CHECK(*it == 1u);
}

TEST_CASE("enumerate sequence of multiple values")
{
	bool startedExecution = false;
	auto gen = [&]() -> cppcoro::async_generator<std::uint32_t>
	{
		startedExecution = true;
		co_yield 1;
		co_yield 2;
		co_yield 3;
	}();

	CHECK(!startedExecution);

	auto beginAwaitable = gen.begin();
	CHECK(startedExecution);
	CHECK(beginAwaitable.await_ready());

	auto it = beginAwaitable.await_resume();
	CHECK(it != gen.end());
	CHECK(*it == 1u);

	auto incrementAwaitable1 = ++it;
	CHECK(incrementAwaitable1.await_ready());
	incrementAwaitable1.await_resume();
	CHECK(*it == 2u);

	auto incrementAwaitable2 = ++it;
	CHECK(incrementAwaitable2.await_ready());
	incrementAwaitable2.await_resume();
	CHECK(*it == 3u);

	auto incrementAwaitable3 = ++it;
	CHECK(incrementAwaitable3.await_ready());
	incrementAwaitable3.await_resume();
	CHECK(it == gen.end());
}

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

TEST_CASE("destructors of values in scope are called when async_generator destructed early")
{
	bool aDestructed = false;
	bool bDestructed = false;
	{
		auto gen = [&](set_to_true_on_destruction a) -> cppcoro::async_generator<std::uint32_t>
		{
			set_to_true_on_destruction b(&bDestructed);
			co_yield 1;
			co_yield 2;
		}(&aDestructed);

		CHECK(!aDestructed);
		CHECK(!bDestructed);

		auto beginOp = gen.begin();
		CHECK(beginOp.await_ready());
		CHECK(!aDestructed);
		CHECK(!bDestructed);

		auto it = beginOp.await_resume();
		CHECK(*it == 1u);
		CHECK(!aDestructed);
		CHECK(!bDestructed);
	}

	CHECK(aDestructed);
	CHECK(bDestructed);
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

	auto producer = [&]() -> cppcoro::async_generator<std::uint32_t>
	{
		co_await p1;
		co_yield 1;
		co_await p2;
		co_yield 2;
		co_await p3;
	}();

	auto consumer = [&]() -> cppcoro::task<>
	{
		auto it = co_await producer.begin();
		CHECK(*it == 1u);
		co_await ++it;
		CHECK(*it == 2u);
		co_await c1;
		co_await ++it;
		CHECK(it == producer.end());
	}();

	p1.set();
	p2.set();
	c1.set();
	CHECK(!consumer.is_ready());
	p3.set();
	CHECK(consumer.is_ready());
}

TEST_CASE("exception thrown before first yield is rethrown from begin operation")
{
	class TestException {};
	auto gen = [&](bool shouldThrow) -> cppcoro::async_generator<std::uint32_t>
	{
		if (shouldThrow)
		{
			throw TestException();
		}
		co_yield 1;
	}(true);

	auto beginAwaitable = gen.begin();
	CHECK(beginAwaitable.await_ready());
	CHECK_THROWS_AS(beginAwaitable.await_resume(), const TestException&);
}

TEST_CASE("exception thrown after first yield is rethrown from increment operator")
{
	class TestException {};
	auto gen = [&](bool shouldThrow) -> cppcoro::async_generator<std::uint32_t>
	{
		co_yield 1;
		if (shouldThrow)
		{
			throw TestException();
		}
	}(true);

	auto beginAwaitable = gen.begin();
	CHECK(beginAwaitable.await_ready());
	auto it = beginAwaitable.await_resume();
	CHECK(*it == 1u);
	auto incrementAwaitable = ++it;
	CHECK(incrementAwaitable.await_ready());
	CHECK_THROWS_AS(incrementAwaitable.await_resume(), const TestException&);
	CHECK(it == gen.end());
}

TEST_CASE("large number of synchronous completions doesn't result in stack-overflow")
{
	cppcoro::single_consumer_event event;

	auto sequence = [](cppcoro::single_consumer_event& event) -> cppcoro::async_generator<std::uint32_t>
	{
		for (std::uint32_t i = 0; i < 1'000'000u; ++i)
		{
			if (i == 500'000u) co_await event;
			co_yield i;
		}
	}(event);

	auto consumerTask = [](cppcoro::async_generator<std::uint32_t> sequence) -> cppcoro::task<>
	{
		std::uint32_t expected = 0;
		for co_await(std::uint32_t i : sequence)
		{
			CHECK(i == expected++);
		}

		CHECK(expected == 1'000'000u);
	}(std::move(sequence));

	CHECK(!consumerTask.is_ready());

	// Should have processed the first 500'000 elements synchronously with consumer driving
	// iteraction before producer suspends and thus consumer suspends.
	// Then we resume producer in call to set() below and it continues processing remaining
	// 500'000 elements, this time with producer driving the interaction.

	event.set();

	CHECK(consumerTask.is_ready());
}
