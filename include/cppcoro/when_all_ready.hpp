///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_WHEN_ALL_READY_HPP_INCLUDED
#define CPPCORO_WHEN_ALL_READY_HPP_INCLUDED

#include <cppcoro/task.hpp>
#include <cppcoro/shared_task.hpp>

#include <cppcoro/detail/when_all_awaitable.hpp>
#include <cppcoro/detail/when_all_ready_awaitable.hpp>

#include <tuple>
#include <functional>
#include <utility>
#include <vector>

namespace cppcoro
{
	template<typename... AWAITABLES>
	[[nodiscard]] auto when_all_ready(AWAITABLES&&... awaitables)
	{
		return detail::when_all_ready_awaitable<typename awaitable_traits<AWAITABLES>::await_result_t...>{
			detail::make_when_all_task(std::forward<AWAITABLES>(awaitables))...
		};
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
