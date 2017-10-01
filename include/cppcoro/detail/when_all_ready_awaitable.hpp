///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_WHEN_ALL_READY_AWAITABLE_HPP_INCLUDED
#define CPPCORO_DETAIL_WHEN_ALL_READY_AWAITABLE_HPP_INCLUDED

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
		template<typename... RESULTS>
		class when_all_ready_awaitable;

		template<>
		class when_all_ready_awaitable<>
		{
		public:

			bool await_ready() const noexcept { return true; }
			void await_suspend([[maybe_unused]] std::experimental::coroutine_handle<> awaiter) noexcept {}
			std::tuple<> await_resume() const noexcept { return {}; }

		};

		template<typename... RESULTS>
		class when_all_ready_awaitable
		{
			class awaiter_base;
			friend class awaiter_base;

			class awaiter_base
			{
			public:

				awaiter_base(when_all_ready_awaitable& awaitable)
					: m_awaitable(awaitable)
				{}

				bool await_ready() const noexcept
				{
					return m_awaitable.m_counter.is_ready();
				}

				bool await_suspend(std::experimental::coroutine_handle<> awaitingCoroutine) noexcept
				{
					// Now that we have a continuation we can start all of the tasks.
					// Once all of the tasks have been started we can register the continuation
					// with the when_all_awaitable_counter. If all tasks have already completed
					// synchronously then this will return false and we will continue without
					// suspending here.
					m_awaitable.start_all_tasks(std::make_integer_sequence<std::size_t, sizeof...(RESULTS)>{});
					return m_awaitable.m_counter.try_await(awaitingCoroutine);
				}

			protected:

				when_all_ready_awaitable& m_awaitable;

			};

		public:

			when_all_ready_awaitable(detail::when_all_task<RESULTS>&&... tasks)
				: m_counter(sizeof...(RESULTS))
				, m_tasks{ std::move(tasks)... }
			{}

			when_all_ready_awaitable(when_all_ready_awaitable&& other) noexcept
				: m_counter(sizeof...(AWAITABLES))
				, m_tasks(std::move(other.m_tasks))
			{
			}

			auto operator co_await() & noexcept
			{
				class awaiter : public awaiter_base
				{
				public:

					using awaiter_base::awaiter_base;

					[[nodiscard]]
					std::tuple<detail::when_all_task<RESULTS>...>& await_resume() noexcept
					{
						return m_awaitable.m_tasks;
					}

				};

				return awaiter{ *this };
			}

			auto operator co_await() && noexcept
			{
				class awaiter : public awaiter_base
				{
				public:

					using awaiter_base::awaiter_base;

					[[nodiscard]]
					std::tuple<detail::when_all_task<RESULTS>...> await_resume() noexcept
					{
						return std::move(m_awaitable.m_tasks);
					}

				};

				return awaiter{ *this };
			}

		private:

			template<std::size_t... INDICES>
			void start_all_tasks(std::integer_sequence<std::size_t, INDICES...>) noexcept
			{
				const std::initializer_list<int> dummy =
				{
					(std::get<INDICES>(m_tasks).start(m_counter), 0)...
				};
			}

			when_all_awaitable_counter m_counter;
			std::tuple<detail::when_all_task<RESULTS>...> m_tasks;

		};
	}
}

#endif
