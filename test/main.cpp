///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifdef NDEBUG
# undef NDEBUG
#endif

#include <cppcoro/task.hpp>
#include <cppcoro/lazy_task.hpp>
#include <cppcoro/single_consumer_event.hpp>
#include <cppcoro/async_mutex.hpp>

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

void testLazyTaskDoesntStartUntilAwaited()
{
	bool started = false;
	auto func = [&]() -> cppcoro::lazy_task<>
	{
		started = true;
		co_return;
	};

	auto t = func();
	assert(!started);

	[&]() -> cppcoro::task<>
	{
		co_await t;
	}();

	assert(started);
}

void testAwaitingDefaultConstructedLazyTaskThrowsBrokenPromise()
{
	bool ok = false;
	[&]() -> cppcoro::task<>
	{
		cppcoro::lazy_task<> t;
		try
		{
			co_await t;
			assert(false);
		}
		catch (const cppcoro::broken_promise&)
		{
			ok = true;
		}
		catch (...)
		{
			assert(false);
		}
	}();

	assert(ok);
}

void testAwaitingLazyTaskThatCompletesAsynchronously()
{
	bool reachedBeforeEvent = false;
	bool reachedAfterEvent = false;
	cppcoro::single_consumer_event event;
	auto f = [&]() -> cppcoro::lazy_task<>
	{
		reachedBeforeEvent = true;
		co_await event;
		reachedAfterEvent = true;
	};

	auto t = f();

	assert(!t.is_ready());
	assert(!reachedBeforeEvent);

	auto t2 = [](cppcoro::lazy_task<>& t) -> cppcoro::task<>
	{
		co_await t;
	}(t);

	assert(!t2.is_ready());

	event.set();

	assert(t.is_ready());
	assert(t2.is_ready());
	assert(reachedAfterEvent);
}

void testLazyTaskNeverAwaitedDestroysCapturedArgs()
{
	counter::reset_counts();

	auto f = [](counter c) -> cppcoro::lazy_task<counter>
	{
		co_return c;
	};

	assert(counter::active_count() == 0);

	{
		auto t = f(counter{});
		assert(counter::active_count() == 1);
	}

	assert(counter::active_count() == 0);
}

void testLazyTaskResultLifetime()
{
	counter::reset_counts();

	auto f = []() -> cppcoro::lazy_task<counter>
	{
		co_return counter{};
	};

	{
		auto t = f();
		assert(counter::active_count() == 0);

		[](cppcoro::lazy_task<counter>& t) -> cppcoro::task<>
		{
			co_await t;
			assert(t.is_ready());
			assert(counter::active_count() == 1);
		}(t);

		assert(counter::active_count() == 1);
	}

	assert(counter::active_count() == 0);
}

void testLazyTaskReturnByReference()
{
	int value = 3;
	auto f = [&]() -> cppcoro::lazy_task<int&>
	{
		co_return value;
	};

	auto g = [&]() -> cppcoro::task<>
	{
		{
			decltype(auto) result = co_await f();
			static_assert(
				std::is_same<decltype(result), int&>::value,
				"co_await r-value reference of lazy_task<int&> should result in an int&");
			assert(&result == &value);
		}
		{
			auto t = f();
			decltype(auto) result = co_await t;
			static_assert(
				std::is_same<decltype(result), int&>::value,
				"co_await l-value reference of lazy_task<int&> should result in an int&");
			assert(&result == &value);
		}
	};

	auto t = g();
	assert(t.is_ready());
}

void testAsyncMutex()
{
	int value = 0;
	cppcoro::async_mutex mutex;
	cppcoro::single_consumer_event a;
	cppcoro::single_consumer_event b;
	cppcoro::single_consumer_event c;
	cppcoro::single_consumer_event d;

	auto f = [&](cppcoro::single_consumer_event& e) -> cppcoro::task<>
	{
		cppcoro::async_mutex_lock lock = co_await mutex.lock_async();
		co_await e;
		++value;
	};

	auto t1 = f(a);
	assert(!t1.is_ready());
	assert(value == 0);

	auto t2 = f(b);
	auto t3 = f(c);

	a.set();

	assert(value == 1);

	auto t4 = f(d);

	b.set();

	assert(value == 2);

	c.set();

	assert(value == 3);

	d.set();

	assert(value == 4);

	assert(t1.is_ready());
	assert(t2.is_ready());
	assert(t3.is_ready());
	assert(t4.is_ready());
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

	testLazyTaskDoesntStartUntilAwaited();
	testAwaitingDefaultConstructedLazyTaskThrowsBrokenPromise();
	testAwaitingLazyTaskThatCompletesAsynchronously();
	testLazyTaskResultLifetime();
	testLazyTaskNeverAwaitedDestroysCapturedArgs();
	testLazyTaskReturnByReference();

	testAsyncMutex();

	return 0;
}
