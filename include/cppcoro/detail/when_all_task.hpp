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
		template<typename TASK_CONTAINER>
		class when_all_ready_awaitable;

		template<typename RESULT>
		class when_all_task;

		template<typename RESULT>
		class when_all_task_promise final
		{
		public:

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

#if CPPCORO_COMPILER_MSVC
			// HACK: This is needed to work around a bug in MSVC 2017.7/2017.8.
			// See comment in make_when_all_task below.
			template<typename Awaitable>
			Awaitable&& await_transform(Awaitable&& awaitable)
			{
				return static_cast<Awaitable&&>(awaitable);
			}

			struct get_promise_t {};
			static constexpr get_promise_t get_promise = {};

			auto await_transform(get_promise_t)
			{
				class awaiter
				{
				public:
					awaiter(when_all_task_promise* promise) noexcept : m_promise(promise) {}
					bool await_ready() noexcept {
						return true;
					}
					void await_suspend(std::experimental::coroutine_handle<>) noexcept {}
					when_all_task_promise& await_resume() noexcept
					{
						return *m_promise;
					}
				private:
					when_all_task_promise* m_promise;
				};
				return awaiter{ this };
			}
#endif


			auto yield_value(RESULT&& result) noexcept
			{
				m_result = std::addressof(result);
				return final_suspend();
			}

			void start(when_all_counter& counter) noexcept
			{
				m_counter = &counter;
				coroutine_handle_t::from_promise(*this).resume();
			}

			RESULT& result() &
			{
				rethrow_if_exception();
				return *m_result;
			}

			RESULT&& result() &&
			{
				rethrow_if_exception();
				return std::forward<RESULT>(*m_result);
			}

		private:

			void rethrow_if_exception()
			{
				if (m_exception)
				{
					std::rethrow_exception(m_exception);
				}
			}

			when_all_counter* m_counter;
			std::exception_ptr m_exception;
			std::add_pointer_t<RESULT> m_result;

		};

		template<>
		class when_all_task_promise<void> final
		{
		public:

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

			void start(when_all_counter& counter) noexcept
			{
				m_counter = &counter;
				coroutine_handle_t::from_promise(*this).resume();
			}

			void result()
			{
				if (m_exception)
				{
					std::rethrow_exception(m_exception);
				}
			}

		private:

			when_all_counter* m_counter;
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

			decltype(auto) result() &
			{
				return m_coroutine.promise().result();
			}

			decltype(auto) result() &&
			{
				return std::move(m_coroutine.promise()).result();
			}

			decltype(auto) non_void_result() &
			{
				if constexpr (std::is_void_v<decltype(this->result())>)
				{
					this->result();
					return void_value{};
				}
				else
				{
					return this->result();
				}
			}

			decltype(auto) non_void_result() &&
			{
				if constexpr (std::is_void_v<decltype(this->result())>)
				{
					std::move(*this).result();
					return void_value{};
				}
				else
				{
					return std::move(*this).result();
				}
			}

		private:

			template<typename TASK_CONTAINER>
			friend class when_all_ready_awaitable;

			void start(when_all_counter& counter) noexcept
			{
				m_coroutine.promise().start(counter);
			}

			coroutine_handle_t m_coroutine;

		};

		template<
			typename AWAITABLE,
			typename RESULT = typename cppcoro::awaitable_traits<AWAITABLE&&>::await_result_t,
			std::enable_if_t<!std::is_void_v<RESULT>, int> = 0>
		when_all_task<RESULT> make_when_all_task(AWAITABLE awaitable)
		{
#if CPPCORO_COMPILER_MSVC
			// HACK: Workaround another bug in MSVC where the expression 'co_yield co_await x' seems
			// to completely ignore the co_yield an never calls promise.yield_value().
			// The coroutine seems to be resuming the 'co_await' after the 'co_yield'
			// rather than before the 'co_yield'.
			// This bug is present in VS 2017.7 and VS 2017.8.
			auto& promise = co_await when_all_task_promise<RESULT>::get_promise;
			co_await promise.yield_value(co_await std::forward<AWAITABLE>(awaitable));
#else
			co_yield co_await static_cast<AWAITABLE&&>(awaitable);
#endif
		}

		template<
			typename AWAITABLE,
			typename RESULT = typename cppcoro::awaitable_traits<AWAITABLE&&>::await_result_t,
			std::enable_if_t<std::is_void_v<RESULT>, int> = 0>
		when_all_task<void> make_when_all_task(AWAITABLE awaitable)
		{
			co_await static_cast<AWAITABLE&&>(awaitable);
		}

		template<
			typename AWAITABLE,
			typename RESULT = typename cppcoro::awaitable_traits<AWAITABLE&>::await_result_t,
			std::enable_if_t<!std::is_void_v<RESULT>, int> = 0>
		when_all_task<RESULT> make_when_all_task(std::reference_wrapper<AWAITABLE> awaitable)
		{
#if CPPCORO_COMPILER_MSVC
			// HACK: Workaround another bug in MSVC where the expression 'co_yield co_await x' seems
			// to completely ignore the co_yield and never calls promise.yield_value().
			// The coroutine seems to be resuming the 'co_await' after the 'co_yield'
			// rather than before the 'co_yield'.
			// This bug is present in VS 2017.7 and VS 2017.8.
			auto& promise = co_await when_all_task_promise<RESULT>::get_promise;
			co_await promise.yield_value(co_await awaitable.get());
#else
			co_yield co_await awaitable.get();
#endif
		}

		template<
			typename AWAITABLE,
			typename RESULT = typename cppcoro::awaitable_traits<AWAITABLE&>::await_result_t,
			std::enable_if_t<std::is_void_v<RESULT>, int> = 0>
		when_all_task<void> make_when_all_task(std::reference_wrapper<AWAITABLE> awaitable)
		{
			co_await awaitable.get();
		}
	}
}

#endif
