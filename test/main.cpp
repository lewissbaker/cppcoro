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
#include <cppcoro/async_generator.hpp>
#include <cppcoro/shared_task.hpp>
#include <cppcoro/shared_lazy_task.hpp>
#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/cancellation_token.hpp>
#include <cppcoro/cancellation_registration.hpp>
#include <cppcoro/operation_cancelled.hpp>

#include <memory>
#include <string>
#include <chrono>
#include <iostream>

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

void testPassingParameterByValueToLazyTaskCallsMoveConstructorOnce()
{
	counter::reset_counts();

	auto f = [](counter arg) -> cppcoro::lazy_task<>
	{
		co_return;
	};

	counter c;

	assert(counter::active_count() == 1);
	assert(counter::default_construction_count == 1);
	assert(counter::copy_construction_count == 0);
	assert(counter::move_construction_count == 0);
	assert(counter::destruction_count == 0);

	{
		auto t = f(c);

		// Should have called copy-constructor to pass a copy of 'c' into f by value.
		assert(counter::copy_construction_count == 1);
		// Inside f it should have move-constructed parameter into coroutine frame variable
		assert(counter::move_construction_count == 1);
		// And should have destructed the copy passed to the parameter before returning.
		assert(counter::destruction_count == 1);

		// Active counts should be the instance 'c' and the instance captured in coroutine frame of 't'.
		assert(counter::active_count() == 2);
	}

	assert(counter::active_count() == 1);
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

void testSharedTaskDefaultConstruction()
{
	{
		cppcoro::shared_task<> t;
		assert(t.is_ready());

		cppcoro::shared_task<> tCopy = t;
		assert(t.is_ready());
	}

	auto task = []() -> cppcoro::task<>
	{
		try
		{
			co_await cppcoro::shared_task<>{};
			assert(false);
		}
		catch (const cppcoro::broken_promise&)
		{
		}
		catch (...)
		{
			assert(false);
		}
	}();

	assert(task.is_ready());
}

void testSharedTaskMultipleWaiters()
{
	cppcoro::single_consumer_event event;

	auto sharedTask = [](cppcoro::single_consumer_event& event) -> cppcoro::shared_task<>
	{
		co_await event;
	}(event);

	assert(!sharedTask.is_ready());

	auto consumeTask = [](cppcoro::shared_task<> task) -> cppcoro::task<>
	{
		co_await task;
	};

	auto t1 = consumeTask(sharedTask);
	auto t2 = consumeTask(sharedTask);

	assert(!t1.is_ready());
	assert(!t2.is_ready());

	event.set();

	assert(sharedTask.is_ready());
	assert(t1.is_ready());
	assert(t2.is_ready());

	auto t3 = consumeTask(sharedTask);

	assert(t3.is_ready());
}

void testSharedTaskRethrowsUnhandledException()
{
	class X {};

	auto throwingTask = []() -> cppcoro::shared_task<>
	{
		co_await std::experimental::suspend_never{};
		throw X{};
	};

	[&]() -> cppcoro::task<>
	{
		auto t = throwingTask();
		assert(t.is_ready());

		try
		{
			co_await t;
			assert(false);
		}
		catch (X)
		{
		}
		catch (...)
		{
			assert(false);
		}
	}();
}

void testSharedTaskDestroysValueWhenLastReferenceIsDestroyed()
{
	counter::reset_counts();

	{
		cppcoro::shared_task<counter> tCopy;

		{
			auto t = []() -> cppcoro::shared_task<counter>
			{
				co_return counter{};
			}();

			assert(t.is_ready());

			tCopy = t;

			assert(tCopy.is_ready());
		}

		{
			cppcoro::shared_task<counter> tCopy2 = tCopy;

			assert(tCopy2.is_ready());
		}

		assert(counter::active_count() == 1);
	}

	assert(counter::active_count() == 0);
}

void testAssigningResultFromSharedTaskDoesntMoveResult()
{
	auto f = []() -> cppcoro::shared_task<std::string>
	{
		co_return "string that is longer than short-string optimisation";
	};

	auto t = f();

	auto g = [](cppcoro::shared_task<std::string> t) -> cppcoro::task<>
	{
		auto x = co_await t;
		assert(x == "string that is longer than short-string optimisation");

		auto y = co_await std::move(t);
		assert(y == "string that is longer than short-string optimisation");
	};

	g(t);
	g(t);
}

void testSharedTaskOfReferenceType()
{
	const std::string value = "some string value";

	auto f = [&]() -> cppcoro::shared_task<const std::string&>
	{
		co_return value;
	};

	[&]() -> cppcoro::task<>
	{
		auto& result = co_await f();
		assert(&result == &value);
	}();
}

void testSharedTaskReturningRValueReferenceMovesIntoPromise()
{
	counter::reset_counts();

	auto f = []() -> cppcoro::shared_task<counter>
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

void testSharedTaskEquality()
{
	auto f = []() -> cppcoro::shared_task<>
	{
		co_return;
	};

	cppcoro::shared_task<> t0;
	cppcoro::shared_task<> t1 = t0;
	cppcoro::shared_task<> t2 = f();
	cppcoro::shared_task<> t3 = t2;
	cppcoro::shared_task<> t4 = f();
	assert(t0 == t0);
	assert(t0 == t1);
	assert(t0 != t2);
	assert(t0 != t3);
	assert(t0 != t4);
	assert(t2 == t2);
	assert(t2 == t3);
	assert(t2 != t4);
}

void testMakeSharedTask()
{
	cppcoro::single_consumer_event event;

	auto f = [&]() -> cppcoro::task<std::string>
	{
		co_await event;
		co_return "foo";
	};

	auto t = cppcoro::make_shared_task(f());

	auto consumer = [](cppcoro::shared_task<std::string> task) -> cppcoro::task<>
	{
		assert(co_await task == "foo");
	};

	auto consumerTask0 = consumer(t);
	auto consumerTask1 = consumer(t);

	assert(!consumerTask0.is_ready());
	assert(!consumerTask1.is_ready());

	event.set();

	assert(consumerTask0.is_ready());
	assert(consumerTask1.is_ready());
}

void testMakeSharedTaskOfVoid()
{
	cppcoro::single_consumer_event event;

	auto f = [&]() -> cppcoro::task<>
	{
		co_await event;
	};

	auto t = cppcoro::make_shared_task(f());

	assert(!t.is_ready());

	auto consumer = [](cppcoro::shared_task<> task) -> cppcoro::task<>
	{
		co_await task;
	};

	auto consumerTask0 = consumer(t);
	auto consumerTask1 = consumer(t);

	assert(!consumerTask0.is_ready());
	assert(!consumerTask1.is_ready());

	event.set();

	assert(consumerTask0.is_ready());
	assert(consumerTask1.is_ready());
}

void testDefaultSharedLazyTaskThrowsBrokenPromiseWhenAwaited()
{
	auto t = []() -> cppcoro::task<>
	{
		cppcoro::shared_lazy_task<> t;
		try
		{
			co_await t;
			assert(false);
		}
		catch (cppcoro::broken_promise)
		{
		}
		catch (...)
		{
			assert(false);
		}
	}();

	assert(t.is_ready());
}

void testSharedLazyTaskDoesntStartExecutingUntilAwaited()
{
	bool startedExecuting = false;
	auto f = [&]() -> cppcoro::shared_lazy_task<>
	{
		startedExecuting = true;
		co_return;
	};

	auto t = f();

	assert(!t.is_ready());
	assert(!startedExecuting);

	auto waiter = [](cppcoro::shared_lazy_task<> t) -> cppcoro::task<>
	{
		co_await t;
	}(t);

	assert(waiter.is_ready());
	assert(t.is_ready());
	assert(startedExecuting);
}

void testSharedLazyTaskDestroysResultWhenLastReferenceDestroyed()
{
	counter::reset_counts();

	{
		auto t = []() -> cppcoro::shared_lazy_task<counter>
		{
			co_return counter{};
		}();

		assert(counter::active_count() == 0);

		[](cppcoro::shared_lazy_task<counter> t) -> cppcoro::task<>
		{
			co_await t;
		}(t);

		assert(counter::active_count() == 1);
	}

	assert(counter::active_count() == 0);
}

void testSharedLazyTaskMultipleAwaiters()
{
	cppcoro::single_consumer_event event;
	bool startedExecution = false;
	auto produce = [&]() -> cppcoro::shared_lazy_task<int>
	{
		startedExecution = true;
		co_await event;
		co_return 1;
	};

	auto consume = [](cppcoro::shared_lazy_task<int> t) -> cppcoro::task<>
	{
		int result = co_await t;
		assert(result == 1);
	};

	auto sharedTask = produce();

	assert(!sharedTask.is_ready());
	assert(!startedExecution);

	auto t1 = consume(sharedTask);

	assert(!t1.is_ready());
	assert(startedExecution);
	assert(!sharedTask.is_ready());

	auto t2 = consume(sharedTask);

	assert(!t2.is_ready());
	assert(!sharedTask.is_ready());

	auto t3 = consume(sharedTask);

	event.set();

	assert(sharedTask.is_ready());
	assert(t1.is_ready());
	assert(t2.is_ready());
	assert(t3.is_ready());
}

void testWaitingOnSharedLazyTaskInLoopDoesntBlowStack()
{
	// This test checks that awaiting a shared_lazy_task that completes
	// synchronously doesn't recursively resume the awaiter inside the
	// call to start executing the task. If it were to do this then we'd
	// expect that this test would result in failure due to stack-overflow.

	auto completesSynchronously = []() -> cppcoro::shared_lazy_task<int>
	{
		co_return 1;
	};

	auto run = [&]() -> cppcoro::task<>
	{
		int result = 0;
		for (int i = 0; i < 100'000; ++i)
		{
			result += co_await completesSynchronously();
		}
		assert(result == 100'000);
	};

	auto t = run();
	assert(t.is_ready());
}

void testMakeSharedLazyTask()
{
	bool startedExecution = false;

	auto f = [&]() -> cppcoro::lazy_task<std::string>
	{
		startedExecution = false;
		co_return "test";
	};

	auto t = f();

	cppcoro::shared_lazy_task<std::string> sharedT =
		cppcoro::make_shared_task(std::move(t));

	assert(!sharedT.is_ready());
	assert(!startedExecution);

	auto consume = [](cppcoro::shared_lazy_task<std::string> t) -> cppcoro::task<>
	{
		auto x = co_await std::move(t);
		assert(x == "test");
	};

	auto c1 = consume(sharedT);
	auto c2 = consume(sharedT);

	assert(c1.is_ready());
	assert(c2.is_ready());
}

void testMakeSharedLazyTaskOfVoid()
{
	bool startedExecution = false;

	auto f = [&]() -> cppcoro::lazy_task<>
	{
		startedExecution = true;
		co_return;
	};

	auto t = f();

	cppcoro::shared_lazy_task<> sharedT = cppcoro::make_shared_task(std::move(t));

	assert(!sharedT.is_ready());
	assert(!startedExecution);

	auto consume = [](cppcoro::shared_lazy_task<> t) -> cppcoro::task<>
	{
		co_await t;
	};

	auto c1 = consume(sharedT);
	assert(startedExecution);

	auto c2 = consume(sharedT);

	assert(c1.is_ready());
	assert(c2.is_ready());
}

void testDefaultCancellationTokenIsNotCancellable()
{
	cppcoro::cancellation_token t;
	assert(!t.is_cancellation_requested());
	assert(!t.can_be_cancelled());
}

void testRequestCancellation()
{
	cppcoro::cancellation_source s;
	cppcoro::cancellation_token t = s.token();
	assert(t.can_be_cancelled());
	assert(!t.is_cancellation_requested());
	s.request_cancellation();
	assert(t.is_cancellation_requested());
	assert(t.can_be_cancelled());
}

void testCantBeCancelledWhenLastSourceDestructed()
{
	cppcoro::cancellation_token t;
	{
		cppcoro::cancellation_source s;
		t = s.token();
		assert(t.can_be_cancelled());
	}

	assert(!t.can_be_cancelled());
}

void testCanBeCancelledWhenLastSourceDestructedIfCancellationAlreadyRequested()
{
	cppcoro::cancellation_token t;
	{
		cppcoro::cancellation_source s;
		t = s.token();
		assert(t.can_be_cancelled());
		s.request_cancellation();
	}

	assert(t.can_be_cancelled());
	assert(t.is_cancellation_requested());
}

void testCancellationRegistrationWhenCancellationNotRequested()
{
	cppcoro::cancellation_source s;

	bool callbackExecuted = false;
	{
		cppcoro::cancellation_registration callbackRegistration(
			s.token(),
			[&] { callbackExecuted = true; });
	}

	assert(!callbackExecuted);

	{
		cppcoro::cancellation_registration callbackRegistration(
			s.token(),
			[&] { callbackExecuted = true; });

		assert(!callbackExecuted);

		s.request_cancellation();

		assert(callbackExecuted);
	}
}

void testThrowIfCancellationRequested()
{
	cppcoro::cancellation_source s;
	cppcoro::cancellation_token t = s.token();

	try
	{
		t.throw_if_cancellation_requested();
	}
	catch (cppcoro::operation_cancelled)
	{
		assert(false);
	}

	s.request_cancellation();

	try
	{
		t.throw_if_cancellation_requested();
		assert(false);
	}
	catch (cppcoro::operation_cancelled)
	{
	}
}

void testCancellationRegistrationCalledImmediatelyWhenCancellationAlreadyRequested()
{
	cppcoro::cancellation_source s;
	s.request_cancellation();

	bool executed = false;
	cppcoro::cancellation_registration r{ s.token(), [&] { executed = true; } };
	assert(executed);
}

void testRegisteringManyCallbacks()
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

	assert(callbackExecutionCount == 18);
}

void testConcurrentRegistrationAndCancellation()
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

void testCancellationRegistrationPerformanceSingleThreaded()
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
		std::cout << label << " took " << us << "us (" << (1000.0 * us / count) << " ns/item)"
			<< std::endl;
	};

	report("Individual", time1, iterationCount);
	report("Batch10", time2, 10 * iterationCount);
	report("Batch50", time3, 50 * iterationCount);
}

void testAsyncGeneratorDefaultConstructor()
{
	[]() -> cppcoro::task<>
	{
		// Iterating over default-constructed async_generator just
		// gives an empty sequence.
		cppcoro::async_generator<int> g;
		for co_await(int x : g)
		{
			assert(false);
		}
	}();
}

void testAsyncGeneratorDestructingWithoutCallingBegin()
{
	bool startedExecution = false;
	{
		auto gen = [&]() -> cppcoro::async_generator<int>
		{
			startedExecution = true;
			co_yield 1;
		}();
		assert(!startedExecution);
	}
	assert(!startedExecution);
}

void testEnumerateFirstValueOfSequenceOfOne()
{
	bool startedExecution = false;
	auto gen = [&]() -> cppcoro::async_generator<std::uint32_t>
	{
		startedExecution = true;
		co_yield 1;
	}();

	assert(!startedExecution);

	auto itAwaitable = gen.begin();
	assert(startedExecution);
	assert(itAwaitable.await_ready());
	auto it = itAwaitable.await_resume();
	assert(it != gen.end());
	assert(*it == 1u);
}

void testEnumerateMultipleValues()
{
	bool startedExecution = false;
	auto gen = [&]() -> cppcoro::async_generator<std::uint32_t>
	{
		startedExecution = true;
		co_yield 1;
		co_yield 2;
		co_yield 3;
	}();

	assert(!startedExecution);

	auto beginAwaitable = gen.begin();
	assert(startedExecution);
	assert(beginAwaitable.await_ready());

	auto it = beginAwaitable.await_resume();
	assert(it != gen.end());
	assert(*it == 1u);

	auto incrementAwaitable1 = ++it;
	assert(incrementAwaitable1.await_ready());
	incrementAwaitable1.await_resume();
	assert(*it == 2u);

	auto incrementAwaitable2 = ++it;
	assert(incrementAwaitable2.await_ready());
	incrementAwaitable2.await_resume();
	assert(*it == 3u);

	auto incrementAwaitable3 = ++it;
	assert(incrementAwaitable3.await_ready());
	incrementAwaitable3.await_resume();
	assert(it == gen.end());
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

void testDestructorsAreCalledWhenSequenceDestructedEarly()
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

		assert(!aDestructed);
		assert(!bDestructed);

		auto beginOp = gen.begin();
		assert(beginOp.await_ready());
		assert(!aDestructed);
		assert(!bDestructed);

		auto it = beginOp.await_resume();
		assert(*it == 1u);
		assert(!aDestructed);
		assert(!bDestructed);
	}

	assert(aDestructed);
	assert(bDestructed);
}

void testAsyncProducerAsyncConsumer()
{
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
		assert(*it == 1u);
		co_await ++it;
		assert(*it == 2u);
		co_await c1;
		co_await ++it;
		assert(it == producer.end());
	}();

	p1.set();
	p2.set();
	c1.set();
	assert(!consumer.is_ready());
	p3.set();
	assert(consumer.is_ready());
}

void testExceptionThrownBeforeFirstYield()
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
	assert(beginAwaitable.await_ready());
	try
	{
		auto it = beginAwaitable.await_resume();
		assert(false);
	}
	catch (const TestException&)
	{
	}
}

void testExceptionThrownAfterFirstYield()
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
	assert(beginAwaitable.await_ready());
	auto it = beginAwaitable.await_resume();
	assert(*it == 1u);
	auto incrementAwaitable = ++it;
	assert(incrementAwaitable.await_ready());
	try
	{
		incrementAwaitable.await_resume();
		assert(false);
	}
	catch (const TestException&)
	{
	}
}

void testAsyncGeneratorLargeNumberOfSynchronousCompletions()
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
			assert(i == expected++);
		}

		assert(expected == 1'000'000u);
	}(std::move(sequence));

	assert(!consumerTask.is_ready());

	// Should have processed the first 500'000 elements synchronously with consumer driving
	// iteraction before producer suspends and thus consumer suspends.
	// Then we resume producer in call to set() below and it continues processing remaining
	// 500'000 elements, this time with producer driving the interaction.

	event.set();

	assert(consumerTask.is_ready());
}

int main(int argc, char** argv)
{
	// task<T> tests
	testAwaitSynchronouslyCompletingVoidFunction();
	testAwaitTaskReturningMoveOnlyType();
	testAwaitTaskReturningReference();
	testAwaitDelayedCompletionChain();
	testAwaitTaskReturningValueMovesIntoPromiseIfPassedRValue();
	testAwaitTaskReturningValueCopiesIntoPromiseIfPassedLValue();
	testAwaitingBrokenPromiseThrows();
	testAwaitRethrowsException();
	testAwaitWhenReadyDoesntThrowException();

	// lazy_task<T> tests
	testLazyTaskDoesntStartUntilAwaited();
	testAwaitingDefaultConstructedLazyTaskThrowsBrokenPromise();
	testAwaitingLazyTaskThatCompletesAsynchronously();
	testLazyTaskResultLifetime();
	testLazyTaskNeverAwaitedDestroysCapturedArgs();
	testLazyTaskReturnByReference();

	// NOTE: Test is disabled as MSVC 2017.1 compiler is currently
	// failing the test as it calls move-constructor twice for the
	// captured parameter value. Need to check whether this is a
	// bug or something that is unspecified in standard.
#if !defined(_MSC_VER) || _MSC_FULL_VER > 191025224
	testPassingParameterByValueToLazyTaskCallsMoveConstructorOnce();
#endif

	// async_mutex tests
	testAsyncMutex();

	// shared_task<T> tests
	testSharedTaskDefaultConstruction();
	testSharedTaskMultipleWaiters();
	testSharedTaskRethrowsUnhandledException();
	testSharedTaskDestroysValueWhenLastReferenceIsDestroyed();
	testAssigningResultFromSharedTaskDoesntMoveResult();
	testSharedTaskOfReferenceType();
	testSharedTaskReturningRValueReferenceMovesIntoPromise();
	testSharedTaskEquality();
	testMakeSharedTask();
	testMakeSharedTaskOfVoid();

	// shared_lazy_task<T> tests
	testDefaultSharedLazyTaskThrowsBrokenPromiseWhenAwaited();
	testSharedLazyTaskDoesntStartExecutingUntilAwaited();
	testSharedLazyTaskMultipleAwaiters();
	testMakeSharedLazyTask();
	testMakeSharedLazyTaskOfVoid();
	testWaitingOnSharedLazyTaskInLoopDoesntBlowStack();
	testSharedLazyTaskDestroysResultWhenLastReferenceDestroyed();

	// cancellation_source/cancellation_token/cancellation_registration tests
	testDefaultCancellationTokenIsNotCancellable();
	testRequestCancellation();
	testCantBeCancelledWhenLastSourceDestructed();
	testCanBeCancelledWhenLastSourceDestructedIfCancellationAlreadyRequested();
	testCancellationRegistrationWhenCancellationNotRequested();
	testCancellationRegistrationCalledImmediatelyWhenCancellationAlreadyRequested();
	testThrowIfCancellationRequested();
	testRegisteringManyCallbacks();
	testConcurrentRegistrationAndCancellation();
	testCancellationRegistrationPerformanceSingleThreaded();

	// async_generator<T> tests
	testAsyncGeneratorDefaultConstructor();
	testAsyncGeneratorDestructingWithoutCallingBegin();
	testEnumerateFirstValueOfSequenceOfOne();
	testEnumerateMultipleValues();
	testDestructorsAreCalledWhenSequenceDestructedEarly();
	testExceptionThrownAfterFirstYield();
	testExceptionThrownBeforeFirstYield();
	testAsyncProducerAsyncConsumer();
	testAsyncGeneratorLargeNumberOfSynchronousCompletions();

	return 0;
}
