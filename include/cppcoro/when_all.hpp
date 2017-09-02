///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_WHEN_ALL_HPP_INCLUDED
#define CPPCORO_WHEN_ALL_HPP_INCLUDED

#include <cppcoro/task.hpp>
#include <cppcoro/shared_task.hpp>
#include <cppcoro/when_all_ready.hpp>

#include <cppcoro/detail/when_all_awaitable2.hpp>

#include <tuple>
#include <functional>
#include <utility>
#include <vector>
#include <type_traits>
#include <cassert>

namespace cppcoro
{
	//////////
	// Variadic when_all()

	template<typename... AWAITABLES>
	[[nodiscard]] auto when_all(AWAITABLES&&... awaitables)
	{
		return detail::when_all_awaitable2<std::remove_reference_t<AWAITABLES>...>{
			std::forward<AWAITABLES>(awaitables)...
		};
	}

	//////////
	// when_all() with vector of task

	[[nodiscard]]
	inline task<> when_all(std::vector<task<>> tasks)
	{
		tasks = co_await when_all_ready(std::move(tasks));

		// Now await each task so that any exceptions are rethrown.
		for (auto& t : tasks)
		{
			co_await std::move(t);
		}
	}

	template<typename T>
	[[nodiscard]]
	task<std::vector<T>> when_all(std::vector<task<T>> tasks)
	{
		tasks = co_await when_all_ready(std::move(tasks));

		std::vector<T> results;
		results.reserve(tasks.size());

		for (auto& t : tasks)
		{
			results.emplace_back(co_await std::move(t));
		}

		co_return std::move(results);
	}

	template<typename T>
	[[nodiscard]]
	task<std::vector<std::reference_wrapper<T>>> when_all(std::vector<task<T&>> tasks)
	{
		tasks = co_await when_all_ready(std::move(tasks));

		std::vector<std::reference_wrapper<T>> results;
		results.reserve(tasks.size());

		for (auto& t : tasks)
		{
			results.emplace_back(co_await std::move(t));
		}

		co_return std::move(results);
	}

	//////////
	// when_all() with vector of shared_task

	[[nodiscard]]
	inline task<> when_all(std::vector<shared_task<>> tasks)
	{
		tasks = co_await when_all_ready(std::move(tasks));

		// Now await each task so that any exceptions are rethrown.
		for (auto& t : tasks)
		{
			co_await std::move(t);
		}
	}

	template<typename T>
	[[nodiscard]]
	task<std::vector<T>> when_all(std::vector<shared_task<T>> tasks)
	{
		tasks = co_await when_all_ready(std::move(tasks));

		std::vector<T> results;
		results.reserve(tasks.size());

		for (auto& t : tasks)
		{
			results.emplace_back(co_await std::move(t));
		}

		co_return std::move(results);
	}

	template<typename T>
	[[nodiscard]]
	task<std::vector<std::reference_wrapper<T>>> when_all(std::vector<shared_task<T&>> tasks)
	{
		tasks = co_await when_all_ready(std::move(tasks));

		std::vector<std::reference_wrapper<T>> results;
		results.reserve(tasks.size());

		for (auto& t : tasks)
		{
			results.emplace_back(co_await std::move(t));
		}

		co_return std::move(results);
	}
}

#endif
