///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_WHEN_ALL_READY_HPP_INCLUDED
#define CPPCORO_WHEN_ALL_READY_HPP_INCLUDED

#include <cppcoro/task.hpp>
#include <cppcoro/shared_task.hpp>

#include <cppcoro/detail/when_all_awaitable.hpp>

#include <tuple>
#include <functional>
#include <utility>
#include <vector>

namespace cppcoro
{
	[[nodiscard]]
	inline task<std::tuple<>> when_all_ready()
	{
		co_return std::tuple<>{};
	}

	template<typename TASK>
	[[nodiscard]]
	task<std::tuple<TASK>> when_all_ready(TASK task)
	{
		// Slightly more efficient implementation for single task case that avoids
		// using atomics that are otherwise required to coordinate completion of
		// multiple tasks in general version below.
		co_await std::ref(task).get().when_ready();
		co_return std::tuple<TASK>{ std::move(task) };
	}

	template<typename... TASKS>
	[[nodiscard]]
	task<std::tuple<TASKS...>> when_all_ready(TASKS... tasks)
	{
		detail::when_all_awaitable awaitable{ sizeof...(TASKS) };

		// Use std::initializer_list trick here to force sequential ordering
		// of evaluation of the arguments so that tasks are deterministically
		// started in the order they are passed-in and the 'co_await' is
		// evaluated last but before all of the temporary 'starter' objects
		// are destructed.
		const std::initializer_list<int> dummy = {
			(std::ref(tasks).get().get_starter().start(awaitable.get_continuation()), 0)...,
			(co_await awaitable, 0)
		};

		co_return std::tuple<TASKS...>{ std::move(tasks)... };
	}

	template<typename T>
	[[nodiscard]]
	task<std::vector<task<T>>> when_all_ready(std::vector<task<T>> tasks)
	{
		if (!tasks.empty())
		{
			detail::when_all_awaitable awaitable{ tasks.size() };

			for (auto& t : tasks)
			{
				// NOTE: We are relying on the fact that the 'starter' type returned by get_starter()
				// is not required to live until the task completes.
				t.get_starter().start(awaitable.get_continuation());
			}

			co_await awaitable;
		}

		co_return std::move(tasks);
	}

	template<typename T>
	[[nodiscard]]
	task<std::vector<shared_task<T>>> when_all_ready(std::vector<shared_task<T>> tasks)
	{
		if (!tasks.empty())
		{
			detail::when_all_awaitable awaitable{ tasks.size() };

			using starter_t = decltype(std::declval<shared_task<T>>().get_starter());

			std::vector<starter_t> starters;

			// Reserve the desired number of elements to ensure elements aren't moved as we
			// add elements to the vector in the loop below as that would leave dangling
			// pointers registered as continuations for the tasks.
			starters.reserve(tasks.size());

			for (auto& t : tasks)
			{
				starters.emplace_back(t.get_starter()).start(awaitable.get_continuation());
			}

			co_await awaitable;
		}

		co_return std::move(tasks);
	}
}

#endif
