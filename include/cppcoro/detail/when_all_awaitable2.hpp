///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_WHEN_ALL_AWAITABLE2_HPP_INCLUDED
#define CPPCORO_DETAIL_WHEN_ALL_AWAITABLE2_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/awaitable_traits.hpp>
#include <cppcoro/detail/when_all_counter.hpp>
#include <cppcoro/detail/when_all_task.hpp>

#include <experimental/coroutine>
#include <tuple>

namespace cppcoro
{
	namespace detail
	{
		template<typename... AWAITABLES>
		class when_all_awaitable2;

		template<>
		class when_all_awaitable2<>
		{
		public:

			bool await_ready() const noexcept { return true; }
			void await_suspend([[maybe_unused]] std::experimental::coroutine_handle<> awaiter) noexcept {}
			std::tuple<> await_resume() const noexcept { return {}; }

		};

		template<typename AWAITABLE>
		class when_all_awaitable2<AWAITABLE>
		{
			class wrapped_awaiter
			{
				using awaiter_t = typename cppcoro::awaitable_traits<AWAITABLE&&>::awaiter_t;

			public:

				wrapped_awaiter(AWAITABLE&& awaitable)
					: m_awaiter(get_awaiter(static_cast<AWAITABLE&&>(awaitable)))
				{}

				auto await_ready()
				{
					return static_cast<awaiter_t&&>(m_awaiter).await_ready();
				}

				template<typename PROMISE>
				auto await_suspend(std::experimental::coroutine_handle<PROMISE> awaitingCoroutine)
				{
					return static_cast<awaiter_t&&>(m_awaiter).await_suspend(awaitingCoroutine);
				}

				auto await_resume()
				{
					using await_result_t = decltype(static_cast<awaiter_t&&>(m_awaiter).await_resume());
					if constexpr (std::is_void_v<await_result_t>)
					{
						static_cast<awaiter_t&&>(m_awaiter).await_resume();
						return std::tuple<void_value>{};
					}
					else
					{
						return std::make_tuple(
							static_cast<awaiter_t&&>(m_awaiter).await_resume());
					}
				}

			private:

				awaiter_t m_awaiter;

			};

		public:

			explicit when_all_awaitable2(AWAITABLE&& awaitable)
				noexcept(std::is_nothrow_move_constructible_v<AWAITABLE>)
				: m_awaitable(std::move(awaitable))
			{}

			auto operator co_await()
			{
				// TODO: Should we have separate overloads for l-value reference to *this
				// versus for r-value reference to *this that forward on to the corresponding
				// l-value/r-value operator co_await() overloads on underlying m_awaitable?
				return wrapped_awaiter{ static_cast<AWAITABLE&&>(m_awaitable) };
			}

		private:

			AWAITABLE m_awaitable;

		};

		template<typename... AWAITABLES>
		class when_all_awaitable2
		{
		public:

			static_assert(sizeof...(AWAITABLES) >= 2);

			when_all_awaitable2(AWAITABLES&&... awaitables)
				: m_counter(sizeof...(AWAITABLES))
				, m_tasks(make_when_all_task(static_cast<AWAITABLES&&>(awaitables))...)
			{}

			when_all_awaitable2(when_all_awaitable2&& other) noexcept
				: m_counter(sizeof...(AWAITABLES))
				, m_tasks(std::move(other.m_tasks))
			{
			}

			bool await_ready() const noexcept
			{
				// Should be ready if it's previously been awaited.
				return m_counter.is_ready();
			}

			bool await_suspend(std::experimental::coroutine_handle<> awaitingCoroutine) noexcept
			{
				// Now that we have a continuation we can start all of the tasks.
				// Once all of the tasks have been started we can register the continuation
				// with the when_all_awaitable_counter. If all tasks have already completed
				// synchronously then this will return false and we will continue without
				// suspending here.
				start_all_tasks(std::make_integer_sequence<std::size_t, sizeof...(AWAITABLES)>{});
				return m_counter.try_await(awaitingCoroutine);
			}

			auto await_resume()
			{
				return get_result(std::make_integer_sequence<std::size_t, sizeof...(AWAITABLES)>{});
			}

		private:

			template<std::size_t... INDICES>
			auto get_result(std::integer_sequence<std::size_t, INDICES...>)
			{
				return std::make_tuple(std::get<INDICES>(m_tasks).non_void_result()...);
			}

			template<std::size_t... INDICES>
			void start_all_tasks(std::integer_sequence<std::size_t, INDICES...>) noexcept
			{
				const std::initializer_list<int> dummy =
				{
					(std::get<INDICES>(m_tasks).start(m_counter), 0)...
				};
			}

			when_all_awaitable_counter m_counter;
			std::tuple<when_all_task<typename cppcoro::awaitable_traits<AWAITABLES&&>::await_result_t>...> m_tasks;

		};
	}
}

#endif
