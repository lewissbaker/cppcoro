///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_WHEN_ALL_READY_AWAITABLE_HPP_INCLUDED
#define CPPCORO_DETAIL_WHEN_ALL_READY_AWAITABLE_HPP_INCLUDED

#include <cppcoro/detail/when_all_counter.hpp>

#include <cppcoro/coroutine.hpp>
#include <tuple>

namespace cppcoro
{
	namespace detail
	{
		template<typename TASK_CONTAINER>
		class when_all_ready_awaitable;

		template<>
		class when_all_ready_awaitable<std::tuple<>>
		{
		public:

			constexpr when_all_ready_awaitable() noexcept {}
			explicit constexpr when_all_ready_awaitable(std::tuple<>) noexcept {}

			constexpr bool await_ready() const noexcept { return true; }
			void await_suspend(cppcoro::coroutine_handle<>) noexcept {}
			std::tuple<> await_resume() const noexcept { return {}; }

		};

		template<typename... TASKS>
		class when_all_ready_awaitable<std::tuple<TASKS...>>
		{
		public:

			explicit when_all_ready_awaitable(TASKS&&... tasks)
				noexcept(std::conjunction_v<std::is_nothrow_move_constructible<TASKS>...>)
				: m_counter(sizeof...(TASKS))
				, m_tasks(std::move(tasks)...)
			{}

			explicit when_all_ready_awaitable(std::tuple<TASKS...>&& tasks)
				noexcept(std::is_nothrow_move_constructible_v<std::tuple<TASKS...>>)
				: m_counter(sizeof...(TASKS))
				, m_tasks(std::move(tasks))
			{}

			when_all_ready_awaitable(when_all_ready_awaitable&& other) noexcept
				: m_counter(sizeof...(TASKS))
				, m_tasks(std::move(other.m_tasks))
			{}

			auto operator co_await() & noexcept
			{
				struct awaiter
				{
					awaiter(when_all_ready_awaitable& awaitable) noexcept
						: m_awaitable(awaitable)
					{}

					bool await_ready() const noexcept
					{
						return m_awaitable.is_ready();
					}

					bool await_suspend(cppcoro::coroutine_handle<> awaitingCoroutine) noexcept
					{
						return m_awaitable.try_await(awaitingCoroutine);
					}

					std::tuple<TASKS...>& await_resume() noexcept
					{
						return m_awaitable.m_tasks;
					}

				private:

					when_all_ready_awaitable& m_awaitable;

				};

				return awaiter{ *this };
			}

			auto operator co_await() && noexcept
			{
				struct awaiter
				{
					awaiter(when_all_ready_awaitable& awaitable) noexcept
						: m_awaitable(awaitable)
					{}

					bool await_ready() const noexcept
					{
						return m_awaitable.is_ready();
					}

					bool await_suspend(cppcoro::coroutine_handle<> awaitingCoroutine) noexcept
					{
						return m_awaitable.try_await(awaitingCoroutine);
					}

					std::tuple<TASKS...>&& await_resume() noexcept
					{
						return std::move(m_awaitable.m_tasks);
					}

				private:

					when_all_ready_awaitable& m_awaitable;

				};

				return awaiter{ *this };
			}

		private:

			bool is_ready() const noexcept
			{
				return m_counter.is_ready();
			}

			bool try_await(cppcoro::coroutine_handle<> awaitingCoroutine) noexcept
			{
				start_tasks(std::make_integer_sequence<std::size_t, sizeof...(TASKS)>{});
				return m_counter.try_await(awaitingCoroutine);
			}

			template<std::size_t... INDICES>
			void start_tasks(std::integer_sequence<std::size_t, INDICES...>) noexcept
			{
				(void)std::initializer_list<int>{
					(std::get<INDICES>(m_tasks).start(m_counter), 0)...
				};
			}

			when_all_counter m_counter;
			std::tuple<TASKS...> m_tasks;

		};

		template<typename TASK_CONTAINER>
		class when_all_ready_awaitable
		{
		public:

			explicit when_all_ready_awaitable(TASK_CONTAINER&& tasks) noexcept
				: m_counter(tasks.size())
				, m_tasks(std::forward<TASK_CONTAINER>(tasks))
			{}

			when_all_ready_awaitable(when_all_ready_awaitable&& other)
				noexcept(std::is_nothrow_move_constructible_v<TASK_CONTAINER>)
				: m_counter(other.m_tasks.size())
				, m_tasks(std::move(other.m_tasks))
			{}

			when_all_ready_awaitable(const when_all_ready_awaitable&) = delete;
			when_all_ready_awaitable& operator=(const when_all_ready_awaitable&) = delete;

			auto operator co_await() & noexcept
			{
				class awaiter
				{
				public:

					awaiter(when_all_ready_awaitable& awaitable)
						: m_awaitable(awaitable)
					{}

					bool await_ready() const noexcept
					{
						return m_awaitable.is_ready();
					}

					bool await_suspend(cppcoro::coroutine_handle<> awaitingCoroutine) noexcept
					{
						return m_awaitable.try_await(awaitingCoroutine);
					}

					TASK_CONTAINER& await_resume() noexcept
					{
						return m_awaitable.m_tasks;
					}

				private:

					when_all_ready_awaitable& m_awaitable;

				};

				return awaiter{ *this };
			}


			auto operator co_await() && noexcept
			{
				class awaiter
				{
				public:

					awaiter(when_all_ready_awaitable& awaitable)
						: m_awaitable(awaitable)
					{}

					bool await_ready() const noexcept
					{
						return m_awaitable.is_ready();
					}

					bool await_suspend(cppcoro::coroutine_handle<> awaitingCoroutine) noexcept
					{
						return m_awaitable.try_await(awaitingCoroutine);
					}

					TASK_CONTAINER&& await_resume() noexcept
					{
						return std::move(m_awaitable.m_tasks);
					}

				private:

					when_all_ready_awaitable& m_awaitable;

				};

				return awaiter{ *this };
			}

		private:

			bool is_ready() const noexcept
			{
				return m_counter.is_ready();
			}

			bool try_await(cppcoro::coroutine_handle<> awaitingCoroutine) noexcept
			{
				for (auto&& task : m_tasks)
				{
					task.start(m_counter);
				}

				return m_counter.try_await(awaitingCoroutine);
			}

			when_all_counter m_counter;
			TASK_CONTAINER m_tasks;

		};
	}
}

#endif
