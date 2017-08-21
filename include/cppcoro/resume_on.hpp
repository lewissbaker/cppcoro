///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_RESUME_ON_HPP_INCLUDED
#define CPPCORO_RESUME_ON_HPP_INCLUDED

#include <cppcoro/task.hpp>
#include <cppcoro/shared_task.hpp>
#include <cppcoro/async_generator.hpp>

namespace cppcoro
{
	template<typename SCHEDULER>
	struct resume_on_transform
	{
		explicit resume_on_transform(SCHEDULER& s) noexcept
			: scheduler(s)
		{}

		SCHEDULER& scheduler;
	};

	template<typename SCHEDULER>
	resume_on_transform<SCHEDULER> resume_on(SCHEDULER& scheduler) noexcept
	{
		return resume_on_transform<SCHEDULER>(scheduler);
	}

	template<typename T, typename SCHEDULER>
	decltype(auto) operator|(T&& value, resume_on_transform<SCHEDULER> transform)
	{
		return resume_on(transform.scheduler, std::forward<T>(value));
	}

	template<typename SCHEDULER, typename T>
	task<T> resume_on(SCHEDULER& scheduler, task<T> task)
	{
		co_await task.when_ready();
		co_await scheduler.schedule();
		co_return co_await std::move(task);
	}

	template<typename SCHEDULER, typename T>
	task<T> resume_on(SCHEDULER& scheduler, shared_task<T> task)
	{
		co_await task.when_ready();
		co_await scheduler.schedule();
		co_return co_await std::move(task);
	}

	template<typename SCHEDULER, typename T>
	async_generator<T> resume_on(SCHEDULER& scheduler, async_generator<T> source)
	{
		for co_await(auto& value : source)
		{
			co_await scheduler.schedule();
			co_yield value;
		}
	}
}

#endif
