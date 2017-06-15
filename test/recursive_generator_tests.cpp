///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/recursive_generator.hpp>
#include <cppcoro/on_scope_exit.hpp>

#include <chrono>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("recursive_generator");

using cppcoro::recursive_generator;

TEST_CASE("default constructed recursive_generator is empty")
{
	recursive_generator<std::uint32_t> ints;
	CHECK(ints.begin() == ints.end());
}

TEST_CASE("non-recursive use of recursive_generator")
{
	auto f = []() -> recursive_generator<float>
	{
		co_yield 1.0f;
		co_yield 2.0f;
	};

	auto gen = f();
	auto iter = gen.begin();
	CHECK(*iter == 1.0f);
	++iter;
	CHECK(*iter == 2.0f);
	++iter;
	CHECK(iter == gen.end());
}

TEST_CASE("throw before first yield")
{
	class MyException : public std::exception {};

	auto f = []() -> recursive_generator<std::uint32_t>
	{
		throw MyException{};
		co_return;
	};

	auto gen = f();
	try
	{
		auto iter = gen.begin();
		CHECK(false);
	}
	catch (MyException)
	{
		CHECK(true);
	}
}

TEST_CASE("throw after first yield")
{
	class MyException : public std::exception {};

	auto f = []() -> recursive_generator<std::uint32_t>
	{
		co_yield 1;
		throw MyException{};
	};

	auto gen = f();
	auto iter = gen.begin();
	CHECK(*iter == 1u);
	try
	{
		++iter;
		CHECK(false);
	}
	catch (MyException)
	{
		CHECK(true);
	}
}

TEST_CASE("generator doesn't start executing until begin is called")
{
	bool reachedA = false;
	bool reachedB = false;
	bool reachedC = false;
	auto f = [&]() -> recursive_generator<std::uint32_t>
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
	CHECK(*iter == 1u);
	++iter;
	CHECK(reachedB);
	CHECK(!reachedC);
	CHECK(*iter == 2u);
	++iter;
	CHECK(reachedC);
	CHECK(iter == gen.end());
}

TEST_CASE("destroying generator before completion destructs objects on stack")
{
	bool destructed = false;
	bool completed = false;
	auto f = [&]() -> recursive_generator<std::uint32_t>
	{
		auto onExit = cppcoro::on_scope_exit([&]
		{
			destructed = true;
		});

		co_yield 1;
		co_yield 2;
		completed = true;
	};

	for (auto x : f())
	{
		CHECK(x == 1u);
		CHECK(!destructed);
		break;
	}

	CHECK(!completed);
	CHECK(destructed);
}

TEST_CASE("simple recursive yield")
{
	auto f = [](int n, auto& f) -> recursive_generator<const std::uint32_t>
	{
		co_yield n;
		if (n > 0)
		{
			co_yield f(n - 1, f);
			co_yield n;
		}
	};

	auto f2 = [&f](int n)
	{
		return f(n, f);
	};

	{
		auto gen = f2(1);
		auto iter = gen.begin();
		CHECK(*iter == 1u);
		++iter;
		CHECK(*iter == 0u);
		++iter;
		CHECK(*iter == 1u);
		++iter;
		CHECK(iter == gen.end());
	}

	{
		auto gen = f2(2);
		auto iter = gen.begin();
		CHECK(*iter == 2u);
		++iter;
		CHECK(*iter == 1u);
		++iter;
		CHECK(*iter == 0u);
		++iter;
		CHECK(*iter == 1u);
		++iter;
		CHECK(*iter == 2u);
		++iter;
		CHECK(iter == gen.end());
	}
}

TEST_CASE("nested yield that yields nothing")
{
	auto f = []() -> recursive_generator<std::uint32_t>
	{
		co_return;
	};

	auto g = [&f]() -> recursive_generator<std::uint32_t>
	{
		co_yield 1;
		co_yield f();
		co_yield 2;
	};

	auto gen = g();
	auto iter = gen.begin();
	CHECK(*iter == 1u);
	++iter;
	CHECK(*iter == 2u);
	++iter;
	CHECK(iter == gen.end());
}

TEST_CASE("exception thrown from recursive call can be caught by caller")
{
	class SomeException : public std::exception {};

	auto f = [](std::uint32_t depth, auto&& f) -> recursive_generator<std::uint32_t>
	{
		if (depth == 1u)
		{
			throw SomeException{};
		}

		co_yield 1;

		try
		{
			co_yield f(1, f);
		}
		catch (SomeException)
		{
		}

		co_yield 2;
	};

	auto gen = f(0, f);
	auto iter = gen.begin();
	CHECK(*iter == 1u);
	++iter;
	CHECK(*iter == 2u);
	++iter;
	CHECK(iter == gen.end());
}

TEST_CASE("exceptions thrown from nested call can be caught by caller")
{
	class SomeException : public std::exception {};

	auto f = [](std::uint32_t depth, auto&& f) -> recursive_generator<std::uint32_t>
	{
		if (depth == 4u)
		{
			throw SomeException{};
		}
		else if (depth == 3u)
		{
			co_yield 3;

			try
			{
				co_yield f(4, f);
			}
			catch (SomeException)
			{
			}

			co_yield 33;

			throw SomeException{};
		}
		else if (depth == 2u)
		{
			bool caught = false;
			try
			{
				co_yield f(3, f);
			}
			catch (SomeException)
			{
				caught = true;
			}

			if (caught)
			{
				co_yield 2;
			}
		}
		else
		{
			co_yield 1;
			co_yield f(2, f);
			co_yield f(3, f);
		}
	};

	auto gen = f(1, f);
	auto iter = gen.begin();
	CHECK(*iter == 1u);
	++iter;
	CHECK(*iter == 3u);
	++iter;
	CHECK(*iter == 33u);
	++iter;
	CHECK(*iter == 2u);
	++iter;
	CHECK(*iter == 3u);
	++iter;
	CHECK(*iter == 33u);
	try
	{
		++iter;
		CHECK(false);
	}
	catch (SomeException)
	{
	}

	CHECK(iter == gen.end());
}

recursive_generator<std::uint32_t> iterate_range(std::uint32_t begin, std::uint32_t end)
{
	if ((end - begin) <= 10u)
	{
		for (std::uint32_t i = begin; i < end; ++i)
		{
			co_yield i;
		}
	}
	else
	{
		std::uint32_t mid = begin + (end - begin) / 2;
		co_yield iterate_range(begin, mid);
		co_yield iterate_range(mid, end);
	}
}

TEST_CASE("recursive iteration performance")
{
	const std::uint32_t count = 100000;

	auto start = std::chrono::high_resolution_clock::now();

	std::uint64_t sum = 0;
	for (auto i : iterate_range(0, count))
	{
		sum += i;
	}

	auto end = std::chrono::high_resolution_clock::now();

	CHECK(sum == (std::uint64_t(count) * (count - 1)) / 2);

	const auto timeTakenUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
	MESSAGE("Range iteration of " << count << "elements took " << timeTakenUs << "us");
}

TEST_SUITE_END();
