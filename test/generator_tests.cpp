///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/generator.hpp>
#include <cppcoro/on_scope_exit.hpp>

#include <vector>
#include <string>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("generator");

using cppcoro::generator;

TEST_CASE("default-constructed generator is empty sequence")
{
	generator<int> ints;
	CHECK(ints.begin() == ints.end());
}

TEST_CASE("generator of arithmetic type returns by copy")
{
	auto f = []() -> generator<float>
	{
		co_yield 1.0f;
		co_yield 2.0f;
	};

	auto gen = f();
	auto iter = gen.begin();
	// TODO: Should this really be required?
	//static_assert(std::is_same<decltype(*iter), float>::value, "operator* should return float by value");
	CHECK(*iter == 1.0f);
	++iter;
	CHECK(*iter == 2.0f);
	++iter;
	CHECK(iter == gen.end());
}

TEST_CASE("generator of reference returns by reference")
{
	auto f = [](float& value) -> generator<float&>
	{
		co_yield value;
	};

	float value = 1.0f;
	for (auto& x : f(value))
	{
		CHECK(&x == &value);
		x += 1.0f;
	}

	CHECK(value == 2.0f);
}

TEST_CASE("generator doesn't start until its called")
{
	bool reachedA = false;
	bool reachedB = false;
	bool reachedC = false;
	auto f = [&]() -> generator<int>
	{
		reachedA = true;
		co_yield 1;
		reachedB = true;
		co_yield 2;
		reachedC = true;
	};

	auto gen = f();
	CHECK(!reachedA);
	auto iter = gen.begin();
	CHECK(reachedA);
	CHECK(!reachedB);
	CHECK(*iter == 1);
	++iter;
	CHECK(reachedB);
	CHECK(!reachedC);
	CHECK(*iter == 2);
	++iter;
	CHECK(reachedC);
	CHECK(iter == gen.end());
}

TEST_CASE("destroying generator before completion destructs objects on stack")
{
	bool destructed = false;
	bool completed = false;
	auto f = [&]() -> generator<int>
	{
		auto onExit = cppcoro::on_scope_exit([&]
		{
			destructed = true;
		});

		co_yield 1;
		co_yield 2;
		completed = true;
	};

	{
		auto g = f();
		auto it = g.begin();
		auto itEnd = g.end();
		CHECK(it != itEnd);
		CHECK(*it == 1u);
		CHECK(!destructed);
	}

	CHECK(!completed);
	CHECK(destructed);
}

TEST_CASE("generator throwing before yielding first element rethrows out of begin()")
{
	class X {};

	auto g = []() -> cppcoro::generator<int>
	{
		throw X{};
		co_return;
	}();

	try
	{
		g.begin();
		FAIL("should have thrown");
	}
	catch (const X&)
	{
	}
}

TEST_CASE("generator throwing after first element rethrows out of operator++")
{
	class X {};

	auto g = []() -> cppcoro::generator<int>
	{
		co_yield 1;
		throw X{};
	}();

	auto iter = g.begin();
	REQUIRE(iter != g.end());
	try
	{
		++iter;
		FAIL("should have thrown");
	}
	catch (const X&)
	{
	}
}

template<typename FIRST, typename SECOND>
auto concat(FIRST&& first, SECOND&& second)
{
	using value_type = std::remove_reference_t<decltype(*first.begin())>;
	return [](FIRST first, SECOND second) -> cppcoro::generator<value_type>
	{
		for (auto&& x : first) co_yield x;
		for (auto&& y : second) co_yield y;
	}(std::forward<FIRST>(first), std::forward<SECOND>(second));
}

TEST_CASE("safe capture of r-value reference args")
{
	using namespace std::string_literals;

	// Check that we can capture l-values by reference and that temporary
	// values are moved into the coroutine frame.
	std::string byRef = "bar";
	auto g = concat("foo"s, concat(byRef, std::vector<char>{ 'b', 'a', 'z' }));

	byRef = "buzz";

	std::string s;
	for (char c : g)
	{
		s += c;
	}

	CHECK(s == "foobuzzbaz");
}

TEST_SUITE_END();
