///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_WHEN_ALL_HPP_INCLUDED
#define CPPCORO_WHEN_ALL_HPP_INCLUDED

#include <cppcoro/task.hpp>
#include <cppcoro/shared_task.hpp>
#include <cppcoro/when_all_ready.hpp>

#include <cppcoro/detail/unwrap_reference.hpp>
#include <cppcoro/detail/when_all_awaitable.hpp>

#include <tuple>
#include <functional>
#include <utility>
#include <vector>
#include <type_traits>

namespace cppcoro
{
	//////////
	// Variadic when_all()

	namespace detail
	{
		template<typename T>
		T&& move_if_not_reference_wrapper(T& value)
		{
			return std::move(value);
		}

		template<typename T>
		T& move_if_not_reference_wrapper(std::reference_wrapper<T>& value)
		{
			return value.get();
		}
	}

	inline task<std::tuple<>> when_all()
	{
		co_return std::tuple<>{};
	}

	template<typename TASK>
	task<std::tuple<typename detail::unwrap_reference_t<TASK>::value_type>> when_all(TASK task)
	{
		// Specialisation for one task parameter that avoids use of atomics as no synchronisation
		// is required.
		co_return std::tuple<typename detail::unwrap_reference_t<TASK>::value_type>{ co_await std::move(task) };
	}

	template<typename... TASKS>
	[[nodiscard]]
	task<std::tuple<typename detail::unwrap_reference_t<TASKS>::value_type...>> when_all(TASKS... tasks)
	{
		detail::when_all_awaitable awaitable{ sizeof...(TASKS) };

		const std::initializer_list<int> dummy = {
			(std::ref(tasks).get().get_starter().start(awaitable.get_continuation()), 0)...,
			(co_await awaitable, 0)
		};

		co_return std::tuple<typename detail::unwrap_reference_t<TASKS>::value_type...>{
			co_await detail::move_if_not_reference_wrapper(tasks)...
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
