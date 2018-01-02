///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/generator.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/fmap.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <forward_list>

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

namespace
{
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

namespace
{
	cppcoro::generator<int> range(int start, int end)
	{
		for (; start < end; ++start)
		{
			co_yield start;
		}
	}
}

TEST_CASE("fmap operator")
{
	cppcoro::generator<int> gen = range(0, 5)
		| cppcoro::fmap([](int x) { return x * 3; });

	auto it = gen.begin();
	CHECK(*it == 0);
	CHECK(*++it == 3);
	CHECK(*++it == 6);
	CHECK(*++it == 9);
	CHECK(*++it == 12);
	CHECK(++it == gen.end());
}

namespace
{
	template<std::size_t window, typename Range>
	cppcoro::generator<const double> low_pass(Range rng)
	{
		auto it = std::begin(rng);
		const auto itEnd = std::end(rng);

		const double invCount = 1.0 / window;
		double sum = 0;

		using iter_cat =
			typename std::iterator_traits<decltype(it)>::iterator_category;

		if constexpr (std::is_base_of_v<std::random_access_iterator_tag, iter_cat>)
		{
			for (std::size_t count = 0; it != itEnd && count < window; ++it)
			{
				sum += *it;
				++count;
				co_yield sum / count;
			}

			for (; it != itEnd; ++it)
			{
				sum -= *(it - window);
				sum += *it;
				co_yield sum * invCount;
			}
		}
		else if constexpr (std::is_base_of_v<std::forward_iterator_tag, iter_cat>)
		{
			auto windowStart = it;
			for (std::size_t count = 0; it != itEnd && count < window; ++it)
			{
				sum += *it;
				++count;
				co_yield sum / count;
			}

			for (; it != itEnd; ++it, ++windowStart)
			{
				sum -= *windowStart;
				sum += *it;
				co_yield sum * invCount;
			}
		}
		else
		{
			// Just assume an input iterator
			double buffer[window];

			for (std::size_t count = 0; it != itEnd && count < window; ++it)
			{
				buffer[count] = *it;
				sum += buffer[count];
				++count;
				co_yield sum / count;
			}

			for (std::size_t pos = 0; it != itEnd; ++it, pos = (pos + 1 == window) ? 0 : (pos + 1))
			{
				sum -= std::exchange(buffer[pos], *it);
				sum += buffer[pos];
				co_yield sum * invCount;
			}
		}
	}
}

// HACK: Disable this test as it's causing heap corruption errors under MSVC 2017 Update 5 x86 debug builds.
// Still needs investigation of root cause.
TEST_CASE("low_pass" * doctest::skip{ true })
{
	// With random-access iterator
	{
		auto gen = low_pass<4>(std::vector<int>{ 10, 13, 10, 15, 18, 9, 11, 15 });
		auto it = gen.begin();
		CHECK(*it == 10.0);
		CHECK(*++it == 11.5);
		CHECK(*++it == 11.0);
		CHECK(*++it == 12.0);
		CHECK(*++it == 14.0);
		CHECK(*++it == 13.0);
		CHECK(*++it == 13.25);
		CHECK(*++it == 13.25);
		CHECK(++it == gen.end());
	}

	// With forward-iterator
	{
		auto gen = low_pass<4>(std::forward_list<int>{ 10, 13, 10, 15, 18, 9, 11, 15 });
		auto it = gen.begin();
		CHECK(*it == 10.0);
		CHECK(*++it == 11.5);
		CHECK(*++it == 11.0);
		CHECK(*++it == 12.0);
		CHECK(*++it == 14.0);
		CHECK(*++it == 13.0);
		CHECK(*++it == 13.25);
		CHECK(*++it == 13.25);
		CHECK(++it == gen.end());
	}

	// With input-iterator
	{
		auto gen = low_pass<3>(range(10, 20));
		auto it = gen.begin();
		CHECK(*it == 10.0);
		CHECK(*++it == 10.5);
		CHECK(*++it == 11.0);
		CHECK(*++it == 12.0);
		CHECK(*++it == 13.0);
		CHECK(*++it == 14.0);
		CHECK(*++it == 15.0);
		CHECK(*++it == 16.0);
		CHECK(*++it == 17.0);
		CHECK(*++it == 18.0);
		CHECK(++it == gen.end());
	}
}

TEST_SUITE_END();
