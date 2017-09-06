///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_WHEN_ALL_TASK_HPP_INCLUDED
#define CPPCORO_DETAIL_WHEN_ALL_TASK_HPP_INCLUDED

#include <cppcoro/awaitable_traits.hpp>

#include <cppcoro/detail/when_all_counter.hpp>
#include <cppcoro/detail/void_value.hpp>

#include <experimental/coroutine>
#include <cassert>

namespace cppcoro
{
	namespace detail
	{
		template<typename RESULT>
		class when_all_task;

		template<typename RESULT>
		class when_all_task_promise
		{
		public:

			using reference = RESULT&&;

			using coroutine_handle_t = std::experimental::coroutine_handle<when_all_task_promise<RESULT>>;

			when_all_task_promise() noexcept
			{}

			auto get_return_object() noexcept
			{
				return coroutine_handle_t::from_promise(*this);
			}

			std::experimental::suspend_always initial_suspend() noexcept
			{
				return{};
			}

			auto final_suspend() noexcept
			{
				class completion_notifier
				{
				public:

					bool await_ready() const noexcept { return false; }

					void await_suspend(coroutine_handle_t coro) const noexcept
					{
						coro.promise().m_counter->notify_awaitable_completed();
					}

					void await_resume() const noexcept {}

				};

				return completion_notifier{};
			}

			void unhandled_exception() noexcept
			{
				m_exception = std::current_exception();
			}

			void return_void() noexcept
			{
				// We should have either suspended at co_yield point or
				// an exception was thrown before running off the end of
				// the coroutine.
				assert(false);
			}

			auto yield_value(reference result) noexcept
			{
				m_result = std::addressof(result);
				return final_suspend();
			}

			void start(when_all_awaitable_counter& counter) noexcept
			{
				m_counter = &counter;
				coroutine_handle_t::from_promise(*this).resume();
			}

			reference result()
			{
				if (m_exception)
				{
					std::rethrow_exception(m_exception);
				}

				return static_cast<reference>(*m_result);
			}

		private:

			when_all_awaitable_counter* m_counter;
			std::exception_ptr m_exception;
			std::add_pointer_t<RESULT> m_result;

		};

		template<>
		class when_all_task_promise<void>
		{
		public:

			using reference = void_value;

			using coroutine_handle_t = std::experimental::coroutine_handle<when_all_task_promise<void>>;

			when_all_task_promise() noexcept
			{}

			auto get_return_object() noexcept
			{
				return coroutine_handle_t::from_promise(*this);
			}

			std::experimental::suspend_always initial_suspend() noexcept
			{
				return{};
			}

			auto final_suspend() noexcept
			{
				class completion_notifier
				{
				public:

					bool await_ready() const noexcept { return false; }

					void await_suspend(coroutine_handle_t coro) const noexcept
					{
						coro.promise().m_counter->notify_awaitable_completed();
					}

					void await_resume() const noexcept {}

				};

				return completion_notifier{};
			}

			void unhandled_exception() noexcept
			{
				m_exception = std::current_exception();
			}

			void return_void() noexcept
			{
			}

			void start(when_all_awaitable_counter& counter) noexcept
			{
				m_counter = &counter;
				coroutine_handle_t::from_promise(*this).resume();
			}

			reference result()
			{
				if (m_exception)
				{
					std::rethrow_exception(m_exception);
				}

				return void_value{};
			}

		private:

			when_all_awaitable_counter* m_counter;
			std::exception_ptr m_exception;

		};

		template<typename RESULT>
		class when_all_task final
		{
		public:

			using promise_type = when_all_task_promise<RESULT>;

			using coroutine_handle_t = typename promise_type::coroutine_handle_t;

			when_all_task(coroutine_handle_t coroutine) noexcept
				: m_coroutine(coroutine)
			{}

			when_all_task(when_all_task&& other) noexcept
				: m_coroutine(std::exchange(other.m_coroutine, coroutine_handle_t{}))
			{}

			~when_all_task()
			{
				if (m_coroutine) m_coroutine.destroy();
			}

			when_all_task(const when_all_task&) = delete;
			when_all_task& operator=(const when_all_task&) = delete;

			void start(when_all_awaitable_counter& counter) noexcept
			{
				m_coroutine.promise().start(counter);
			}

			decltype(auto) result()
			{
				return m_coroutine.promise().result();
			}

		private:

			coroutine_handle_t m_coroutine;

		};

		template<
			typename AWAITABLE,
			typename RESULT = typename cppcoro::awaitable_traits<AWAITABLE&&>::await_result_t,
			std::enable_if_t<!std::is_void_v<RESULT>, int> = 0>
		when_all_task<RESULT> make_when_all_task(AWAITABLE awaitable)
		{
			co_yield co_await static_cast<AWAITABLE&&>(awaitable);
		}

		template<
			typename AWAITABLE,
			typename RESULT = typename cppcoro::awaitable_traits<AWAITABLE&&>::await_result_t,
			std::enable_if_t<std::is_void_v<RESULT>, int> = 0>
		when_all_task<void> make_when_all_task(AWAITABLE awaitable)
		{
			co_await static_cast<AWAITABLE&&>(awaitable);
		}
	}
}

#endif
