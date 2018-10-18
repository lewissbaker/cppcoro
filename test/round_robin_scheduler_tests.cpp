///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/config.hpp>
#include <cppcoro/round_robin_scheduler.hpp>

#include <vector>
#include <random>
#include <chrono>

#include "doctest/doctest.h"

#if CPPCORO_CPU_HAVE_SSE
# include <xmmintrin.h>
#endif

namespace
{
	class simple_task
	{
	public:
		class promise_type
		{
		public:
			simple_task get_return_object() noexcept
			{
				return simple_task{ std::experimental::coroutine_handle<promise_type>::from_promise(*this) };
			}

			std::experimental::suspend_never initial_suspend() noexcept { return {}; }
			std::experimental::suspend_always final_suspend() noexcept { return {}; }

			[[noreturn]] void unhandled_exception() noexcept
			{
				std::terminate();
			}

			void return_void() noexcept {}
		};

		explicit simple_task(
			std::experimental::coroutine_handle<promise_type> coro) noexcept
			: m_coro(coro)
		{}

		simple_task(simple_task&& other) noexcept
			: m_coro(std::exchange(other.m_coro, {}))
		{}

		~simple_task()
		{
			if (m_coro) m_coro.destroy();
		}

	private:

		std::experimental::coroutine_handle<promise_type> m_coro;

	};

	template<size_t N, typename Scheduler, typename Func>
	inline void concurrently_impl(Scheduler& scheduler, Func& taskFactory)
	{
		auto t = std::invoke(taskFactory, scheduler);
		if constexpr (N > 0)
		{
			concurrently_impl<N - 1>(scheduler, taskFactory);
		}
		else
		{
			scheduler.drain();
		}
	}

	template<size_t N, typename Func>
	inline void concurrently(Func&& taskFactory)
	{
		cppcoro::round_robin_scheduler<N> scheduler;
		concurrently_impl<N>(scheduler, taskFactory);
	}

	inline void random_access_prefetch(const void* p)
	{
#if CPPCORO_CPU_HAVE_SSE
		_mm_prefetch((const char*)p, _MM_HINT_NTA);
#endif
	}

	constexpr size_t no_result = size_t(-1);

	void multi_binary_search(
		const int index[],
		size_t indexSize,
		const int lookupValues[],
		size_t lookupResults[],
		size_t lookupCount)
	{
		concurrently<10>([=, nextLookup = size_t(0)](auto& scheduler) mutable -> simple_task
		{
			co_await scheduler.schedule();
			while (nextLookup < lookupCount)
			{
				size_t thisLookup = nextLookup++;
				const auto lookupValue = lookupValues[thisLookup];
				auto& lookupResult = lookupResults[thisLookup];

				size_t low = 0;
				size_t high = indexSize;
				lookupResult = no_result;
				while (low < high)
				{
					size_t mid = low + (high - low) / 2;

					// Prefetch the next index value and yield exection to some other
					// operation while it is being fetched from memory.
					random_access_prefetch(&index[mid]);
					co_await scheduler.schedule();

					const int midValue = index[mid];
					if (midValue == lookupValue) {
						lookupResult = mid;
						break;
					}
					low = lookupValue > midValue ? mid + 1 : low;
					high = lookupValue < midValue ? mid : high;
				}
			}
			co_return;
		});
	}

	size_t single_binary_search(const int index[], size_t indexSize, int lookupValue)
	{
		size_t low = 0;
		size_t high = indexSize;
		while (low < high)
		{
			size_t mid = low + (high - low) / 2;
			int midValue = index[mid];
			if (midValue == lookupValue) return mid;
			low = lookupValue > midValue ? mid + 1 : low;
			high = lookupValue < midValue ? mid : high;
		}
		return no_result;
	}

	std::vector<int> make_random_sorted_array_no_duplicates(size_t size)
	{
		std::minstd_rand0 rand{ 101 };
		std::uniform_int_distribution<int> incrementGenerator{ 1, 10 };

		std::vector<int> values;
		values.reserve(size);
		int value = 0;
		for (size_t i = 0; i < size; ++i)
		{
			value += incrementGenerator(rand);
			values.push_back(value);
		}

		return values;
	}

	std::vector<int> make_random_unsorted_array(size_t size, int min, int max)
	{
		std::minstd_rand0 rand{ 101 };
		std::uniform_int_distribution<int> generator{ min, max };

		std::vector<int> values;
		values.reserve(size);

		for (size_t i = 0; i < size; ++i)
		{
			values.push_back(generator(rand));
		}

		return values;
	}

}

TEST_SUITE_BEGIN("round_robin_scheduler");

TEST_CASE("round_robin_scheduler performance")
{
	auto index = make_random_sorted_array_no_duplicates(100'000'000);
	auto lookups = make_random_unsorted_array(1'000'000, -1000, index.back() + 1000);

	std::vector<size_t> results1{ lookups.size() };
	std::vector<size_t> results2{ lookups.size() };

	auto start = std::chrono::high_resolution_clock::now();

	// Calls to naive 1-at-a-time implementation
	for (size_t i = 0, count = lookups.size(); i < count; ++i)
	{
		results1[i] = single_binary_search(index.data(), index.size(), lookups[i]);
	}

	auto end = std::chrono::high_resolution_clock::now();

	const auto naiveTime = end - start;

	start = end;

	multi_binary_search(index.data(), index.size(), lookups.data(), results2.data(), lookups.size());

	end = std::chrono::high_resolution_clock::now();

	const auto concurrentTime = end - start;

	if (results1 != results2)
	{
		MESSAGE("ERROR: concurrent and naive algorithms produced diffent results");
	}

	auto toNs = [](auto x)
	{
		return std::chrono::duration_cast<std::chrono::nanoseconds>(x).count();
	};

	MESSAGE("Naive lookup of " << lookups.size()
		<< " items in index of " << index.size()
		<< " took " << toNs(naiveTime) << "ns ("
		<< ((double)toNs(naiveTime) / lookups.size()) << "ns/item)");

	MESSAGE("Concurrent lookup of " << lookups.size()
		<< " items in index of " << index.size()
		<< " took " << toNs(concurrentTime) << "ns ("
		<< ((double)toNs(concurrentTime) / lookups.size()) << "ns/item)");
}

TEST_SUITE_END();
