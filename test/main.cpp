///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#include <cppcoro/task.hpp>
#include <cppcoro/single_consumer_event.hpp>

#include <memory>

#include <cassert>

struct counter
{
	static int default_construction_count;
	static int copy_construction_count;
	static int move_construction_count;
	static int destruction_count;

	int id;

	static void reset_counts()
	{
		default_construction_count = 0;
		copy_construction_count = 0;
		move_construction_count = 0;
		destruction_count = 0;
	}

	static int construction_count()
	{
		return default_construction_count + copy_construction_count + move_construction_count;
	}

	static int active_count()
	{
		return construction_count() - destruction_count;
	}

	counter() : id(default_construction_count++) {}
	counter(const counter& other) : id(other.id) { ++copy_construction_count; }
	counter(counter&& other) : id(other.id) { ++move_construction_count; other.id = -1; }
	~counter() { ++destruction_count; }

};

int counter::default_construction_count;
int counter::copy_construction_count;
int counter::move_construction_count;
int counter::destruction_count;

void testAwaitSynchronouslyCompletingVoidFunction()
{
	auto doNothingAsync = []() -> cppcoro::task<>
	{
		co_return;
	};

	auto task = doNothingAsync();

	assert(task.is_ready());

	bool ok = false;
	auto test = [&]() -> cppcoro::task<>
	{
		co_await task;
		ok = true;
	};

	test();

	assert(ok);
}

void testAwaitTaskReturningMoveOnlyType()
{
	auto getIntPtrAsync = []() -> cppcoro::task<std::unique_ptr<int>>
	{
		co_return std::make_unique<int>(123);
	};

	auto test = [&]() -> cppcoro::task<>
	{
		auto intPtr = co_await getIntPtrAsync();
		assert(*intPtr == 123);

		auto intPtrTask = getIntPtrAsync();
		{
			// co_await yields l-value reference if task is l-value
			auto& intPtr2 = co_await intPtrTask;
			assert(*intPtr2 == 123);
		}

		{
			// Returns r-value reference if task is r-value
			auto intPtr3 = co_await std::move(intPtrTask);
			assert(*intPtr3 == 123);
		}
	};

	auto task = test();

	assert(task.is_ready());
}

void testAwaitTaskReturningReference()
{
	int value = 0;
	auto getRefAsync = [&]() -> cppcoro::task<int&>
	{
		co_return value;
	};

	auto test = [&]() -> cppcoro::task<>
	{
		// Await r-value task results in l-value reference
		decltype(auto) result = co_await getRefAsync();
		assert(&result == &value);

		// Await l-value task results in l-value reference
		auto getRefTask = getRefAsync();
		decltype(auto) result2 = co_await getRefTask;
		assert(&result2 == &value);
	};

	auto task = test();
	assert(task.is_ready());
}

void testAwaitTaskReturningValueMovesIntoPromiseIfPassedRValue()
{
	counter::reset_counts();

	auto f = []() -> cppcoro::task<counter>
	{
		co_return counter{};
	};

	assert(counter::active_count() == 0);

	{
		auto t = f();
		assert(counter::default_construction_count == 1);
		assert(counter::copy_construction_count == 0);
		assert(counter::move_construction_count == 1);
		assert(counter::destruction_count == 1);
		assert(counter::active_count() == 1);

		// Moving task doesn't move/copy result.
		auto t2 = std::move(t);
		assert(counter::default_construction_count == 1);
		assert(counter::copy_construction_count == 0);
		assert(counter::move_construction_count == 1);
		assert(counter::destruction_count == 1);
		assert(counter::active_count() == 1);
	}

	assert(counter::active_count() == 0);
}

void testAwaitTaskReturningValueCopiesIntoPromiseIfPassedLValue()
{
	counter::reset_counts();

	auto f = []() -> cppcoro::task<counter>
	{
		counter temp;

		// Should be calling copy-constructor here since <promise>.return_value()
		// is being passed an l-value reference.
		co_return temp;
	};

	assert(counter::active_count() == 0);

	{
		auto t = f();
		assert(counter::default_construction_count == 1);
		assert(counter::copy_construction_count == 1);
		assert(counter::move_construction_count == 0);
		assert(counter::destruction_count == 1);
		assert(counter::active_count() == 1);

		// Moving the task doesn't move/copy the result
		auto t2 = std::move(t);
		assert(counter::default_construction_count == 1);
		assert(counter::copy_construction_count == 1);
		assert(counter::move_construction_count == 0);
		assert(counter::destruction_count == 1);
		assert(counter::active_count() == 1);
	}

	assert(counter::active_count() == 0);
}

void testAwaitDelayedCompletionChain()
{
	cppcoro::single_consumer_event event;
	bool reachedPointA = false;
	bool reachedPointB = false;
	auto async1 = [&]() -> cppcoro::task<int>
	{
		reachedPointA = true;
		co_await event;
		reachedPointB = true;
		co_return 1;
	};

	bool reachedPointC = false;
	bool reachedPointD = false;
	auto async2 = [&]() -> cppcoro::task<int>
	{
		reachedPointC = true;
		int result = co_await async1();
		reachedPointD = true;
		co_return result;
	};

	auto task = async2();

	assert(!task.is_ready());
	assert(reachedPointA);
	assert(!reachedPointB);
	assert(reachedPointC);
	assert(!reachedPointD);

	event.set();

	assert(task.is_ready());
	assert(reachedPointB);
	assert(reachedPointD);

	[](cppcoro::task<int> t) -> cppcoro::task<>
	{
		int value = co_await t;
		assert(value == 1);
	}(std::move(task));
}

void testAwaitingBrokenPromiseThrows()
{
	bool ok = false;
	auto test = [&]() -> cppcoro::task<>
	{
		cppcoro::task<> broken;
		try
		{
			co_await broken;
		}
		catch (cppcoro::broken_promise)
		{
			ok = true;
		}
	};

	auto t = test();
	assert(t.is_ready());
	assert(ok);
}

void testAwaitRethrowsException()
{
	class X {};

	auto run = [](bool doThrow) -> cppcoro::task<>
	{
		if (doThrow) throw X{};
		co_return;
	};

	auto t = run(true);

	bool ok = false;
	auto consumeT = [&]() -> cppcoro::task<>
	{
		try
		{
			co_await t;
		}
		catch (X)
		{
			ok = true;
		}
	};

	auto consumer = consumeT();

	assert(t.is_ready());
	assert(consumer.is_ready());
	assert(ok);
}

void testAwaitWhenReadyDoesntThrowException()
{
	class X {};

	auto run = [](bool doThrow) -> cppcoro::task<>
	{
		if (doThrow) throw X{};
		co_return;
	};

	auto t = run(true);

	bool ok = false;
	auto consumeT = [&]() -> cppcoro::task<>
	{
		try
		{
			co_await t.when_ready();
			ok = true;
		}
		catch (...)
		{
		}
	};

	auto consumer = consumeT();

	assert(t.is_ready());
	assert(consumer.is_ready());
	assert(ok);
}

int main(int argc, char** argv)
{
	testAwaitSynchronouslyCompletingVoidFunction();
	testAwaitTaskReturningMoveOnlyType();
	testAwaitTaskReturningReference();
	testAwaitDelayedCompletionChain();
	testAwaitTaskReturningValueMovesIntoPromiseIfPassedRValue();
	testAwaitTaskReturningValueCopiesIntoPromiseIfPassedLValue();
	testAwaitingBrokenPromiseThrows();
	testAwaitRethrowsException();
	testAwaitWhenReadyDoesntThrowException();
	return 0;
}
