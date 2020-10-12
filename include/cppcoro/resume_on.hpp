///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_RESUME_ON_HPP_INCLUDED
#define CPPCORO_RESUME_ON_HPP_INCLUDED

#include <cppcoro/task.hpp>
#include <cppcoro/async_generator.hpp>
#include <cppcoro/awaitable_traits.hpp>
#include <cppcoro/detail/get_awaiter.hpp>

#include <exception>
#include <type_traits>

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

	template<
		typename SCHEDULER,
		typename AWAITABLE,
		typename AWAIT_RESULT = detail::remove_rvalue_reference_t<typename awaitable_traits<AWAITABLE>::await_result_t>,
		std::enable_if_t<!std::is_void_v<AWAIT_RESULT>, int> = 0>
	auto resume_on(SCHEDULER& scheduler, AWAITABLE awaitable)
		-> task<AWAIT_RESULT>
	{
		bool rescheduled = false;
		std::exception_ptr ex;
		try
		{
			// We manually get the awaiter here so that we can keep
			// it alive across the call to `scheduler.schedule()`
			// just in case the result is a reference to a value
			// in the awaiter that would otherwise be a temporary
			// and destructed before the value could be returned.

			auto&& awaiter = detail::get_awaiter(static_cast<AWAITABLE&&>(awaitable));

			auto&& result = co_await static_cast<decltype(awaiter)>(awaiter);

			// Flag as rescheduled before scheduling in case it is the
			// schedule() operation that throws an exception as we don't
			// want to attempt to schedule twice if scheduling fails.
			rescheduled = true;

			co_await scheduler.schedule();

			co_return static_cast<decltype(result)>(result);
		}
		catch (...)
		{
			ex = std::current_exception();
		}

		// We still want to resume on the scheduler even in the presence
		// of an exception.
		if (!rescheduled)
		{
			co_await scheduler.schedule();
		}

		std::rethrow_exception(ex);
	}

	template<
		typename SCHEDULER,
		typename AWAITABLE,
		typename AWAIT_RESULT = detail::remove_rvalue_reference_t<typename awaitable_traits<AWAITABLE>::await_result_t>,
		std::enable_if_t<std::is_void_v<AWAIT_RESULT>, int> = 0>
	auto resume_on(SCHEDULER& scheduler, AWAITABLE awaitable)
		-> task<>
	{
		std::exception_ptr ex;
		try
		{
			co_await static_cast<AWAITABLE&&>(awaitable);
		}
		catch (...)
		{
			ex = std::current_exception();
		}

		// NOTE: We're assuming that `schedule()` operation is noexcept
		// here. If it were to throw what would we do if 'ex' was non-null?
		// Presumably we'd treat it the same as throwing an exception while
		// unwinding and call std::terminate()?

		co_await scheduler.schedule();

		if (ex)
		{
			std::rethrow_exception(ex);
		}
	}

	template<typename SCHEDULER, typename T>
	async_generator<T> resume_on(SCHEDULER& scheduler, async_generator<T> source)
	{
		for (auto iter = co_await source.begin(); iter != source.end(); co_await ++iter)
		{
			auto& value = *iter;
			co_await scheduler.schedule();
			co_yield value;
		}
	}
}

#endif
