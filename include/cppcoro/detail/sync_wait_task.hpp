///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_SYNC_WAIT_TASK_HPP_INCLUDED
#define CPPCORO_DETAIL_SYNC_WAIT_TASK_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/awaitable_traits.hpp>
#include <cppcoro/detail/lightweight_manual_reset_event.hpp>

#include <cppcoro/coroutine.hpp>
#include <cassert>
#include <exception>
#include <utility>

namespace cppcoro
{
	namespace detail
	{
		template<typename RESULT>
		class sync_wait_task;

		template<typename RESULT>
		class sync_wait_task_promise final
		{
			using coroutine_handle_t = cppcoro::coroutine_handle<sync_wait_task_promise<RESULT>>;

		public:

			using reference = RESULT&&;

			sync_wait_task_promise() noexcept
			{}

			void start(detail::lightweight_manual_reset_event& event)
			{
				m_event = &event;
				coroutine_handle_t::from_promise(*this).resume();
			}

			auto get_return_object() noexcept
			{
				return coroutine_handle_t::from_promise(*this);
			}

			cppcoro::suspend_always initial_suspend() noexcept
			{
				return{};
			}

			auto final_suspend() noexcept
			{
				class completion_notifier
				{
				public:

					bool await_ready() const noexcept { return false; }

					void await_suspend(coroutine_handle_t coroutine) const noexcept
					{
						coroutine.promise().m_event->set();
					}

					void await_resume() noexcept {}
				};

				return completion_notifier{};
			}

#if CPPCORO_COMPILER_MSVC && CPPCORO_COMPILER_MSVC < 19'20'00000
			// HACK: This is needed to work around a bug in MSVC 2017.7/2017.8.
			// See comment in make_sync_wait_task below.
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
					awaiter(sync_wait_task_promise* promise) noexcept : m_promise(promise) {}
					bool await_ready() noexcept {
						return true;
					}
					void await_suspend(cppcoro::coroutine_handle<>) noexcept {}
					sync_wait_task_promise& await_resume() noexcept
					{
						return *m_promise;
					}
				private:
					sync_wait_task_promise* m_promise;
				};
				return awaiter{ this };
			}
#endif

			auto yield_value(reference result) noexcept
			{
				m_result = std::addressof(result);
				return final_suspend();
			}

			void return_void() noexcept
			{
				// The coroutine should have either yielded a value or thrown
				// an exception in which case it should have bypassed return_void().
				assert(false);
			}

			void unhandled_exception()
			{
				m_exception = std::current_exception();
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

			detail::lightweight_manual_reset_event* m_event;
			std::remove_reference_t<RESULT>* m_result;
			std::exception_ptr m_exception;

		};

		template<>
		class sync_wait_task_promise<void>
		{
			using coroutine_handle_t = cppcoro::coroutine_handle<sync_wait_task_promise<void>>;

		public:

			sync_wait_task_promise() noexcept
			{}

			void start(detail::lightweight_manual_reset_event& event)
			{
				m_event = &event;
				coroutine_handle_t::from_promise(*this).resume();
			}

			auto get_return_object() noexcept
			{
				return coroutine_handle_t::from_promise(*this);
			}

			cppcoro::suspend_always initial_suspend() noexcept
			{
				return{};
			}

			auto final_suspend() noexcept
			{
				class completion_notifier
				{
				public:

					bool await_ready() const noexcept { return false; }

					void await_suspend(coroutine_handle_t coroutine) const noexcept
					{
						coroutine.promise().m_event->set();
					}

					void await_resume() noexcept {}
				};

				return completion_notifier{};
			}

			void return_void() {}

			void unhandled_exception()
			{
				m_exception = std::current_exception();
			}

			void result()
			{
				if (m_exception)
				{
					std::rethrow_exception(m_exception);
				}
			}

		private:

			detail::lightweight_manual_reset_event* m_event;
			std::exception_ptr m_exception;

		};

		template<typename RESULT>
		class sync_wait_task final
		{
		public:

			using promise_type = sync_wait_task_promise<RESULT>;

			using coroutine_handle_t = cppcoro::coroutine_handle<promise_type>;

			sync_wait_task(coroutine_handle_t coroutine) noexcept
				: m_coroutine(coroutine)
			{}

			sync_wait_task(sync_wait_task&& other) noexcept
				: m_coroutine(std::exchange(other.m_coroutine, coroutine_handle_t{}))
			{}

			~sync_wait_task()
			{
				if (m_coroutine) m_coroutine.destroy();
			}

			sync_wait_task(const sync_wait_task&) = delete;
			sync_wait_task& operator=(const sync_wait_task&) = delete;

			void start(lightweight_manual_reset_event& event) noexcept
			{
				m_coroutine.promise().start(event);
			}

			decltype(auto) result()
			{
				return m_coroutine.promise().result();
			}

		private:

			coroutine_handle_t m_coroutine;

		};

#if CPPCORO_COMPILER_MSVC && CPPCORO_COMPILER_MSVC < 19'20'00000
		// HACK: Work around bug in MSVC where passing a parameter by universal reference
		// results in an error when passed a move-only type, complaining that the copy-constructor
		// has been deleted. The parameter should be passed by reference and the compiler should
		// notcalling the copy-constructor for the argument
		template<
			typename AWAITABLE,
			typename RESULT = typename cppcoro::awaitable_traits<AWAITABLE&&>::await_result_t,
			std::enable_if_t<!std::is_void_v<RESULT>, int> = 0>
		sync_wait_task<RESULT> make_sync_wait_task(AWAITABLE& awaitable)
		{
			// HACK: Workaround another bug in MSVC where the expression 'co_yield co_await x' seems
			// to completely ignore the co_yield an never calls promise.yield_value().
			// The coroutine seems to be resuming the 'co_await' after the 'co_yield'
			// rather than before the 'co_yield'.
			// This bug is present in VS 2017.7 and VS 2017.8.
			auto& promise = co_await sync_wait_task_promise<RESULT>::get_promise;
			co_await promise.yield_value(co_await std::forward<AWAITABLE>(awaitable));

			//co_yield co_await std::forward<AWAITABLE>(awaitable);
		}

		template<
			typename AWAITABLE,
			typename RESULT = typename cppcoro::awaitable_traits<AWAITABLE&&>::await_result_t,
			std::enable_if_t<std::is_void_v<RESULT>, int> = 0>
		sync_wait_task<void> make_sync_wait_task(AWAITABLE& awaitable)
		{
			co_await static_cast<AWAITABLE&&>(awaitable);
		}
#else
		template<
			typename AWAITABLE,
			typename RESULT = typename cppcoro::awaitable_traits<AWAITABLE&&>::await_result_t,
			std::enable_if_t<!std::is_void_v<RESULT>, int> = 0>
		sync_wait_task<RESULT> make_sync_wait_task(AWAITABLE&& awaitable)
		{
			co_yield co_await std::forward<AWAITABLE>(awaitable);
		}

		template<
			typename AWAITABLE,
			typename RESULT = typename cppcoro::awaitable_traits<AWAITABLE&&>::await_result_t,
			std::enable_if_t<std::is_void_v<RESULT>, int> = 0>
		sync_wait_task<void> make_sync_wait_task(AWAITABLE&& awaitable)
		{
			co_await std::forward<AWAITABLE>(awaitable);
		}
#endif
	}
}

#endif
