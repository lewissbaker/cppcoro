///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_TASK_HPP_INCLUDED
#define CPPCORO_TASK_HPP_INCLUDED

#include <cppcoro/broken_promise.hpp>
#include <cppcoro/fmap.hpp>

#include <atomic>
#include <exception>
#include <utility>
#include <type_traits>
#include <functional>

#include <experimental/coroutine>

namespace cppcoro
{
	template<typename T>
	class task;

	namespace detail
	{
		class task_promise_base
		{
		public:

			task_promise_base() noexcept
				: m_state(state::running)
			{}

			auto initial_suspend() noexcept
			{
				return std::experimental::suspend_never{};
			}

			auto final_suspend() noexcept
			{
				struct awaitable
				{
					task_promise_base& m_promise;

					awaitable(task_promise_base& promise) noexcept
						: m_promise(promise)
					{}

					bool await_ready() const noexcept
					{
						return m_promise.m_state.load(std::memory_order_acquire) == state::consumer_detached;
					}

					// If resuming awaiter can potentially throw what state would that leave this coroutine in?
					bool await_suspend(std::experimental::coroutine_handle<>) noexcept
					{
						state oldState = m_promise.m_state.exchange(state::finished, std::memory_order_acq_rel);
						if (oldState == state::consumer_suspended)
						{
							m_promise.m_awaiter.resume();
						}

						return oldState != state::consumer_detached;
					}

					void await_resume() noexcept
					{}
				};

				return awaitable{ *this };
			}

			void unhandled_exception() noexcept
			{
				// No point capturing exception if consumer already detached.
				if (m_state.load(std::memory_order_relaxed) != state::consumer_detached)
				{
					m_exception = std::current_exception();
				}
			}

			bool is_ready() const noexcept
			{
				return m_state.load(std::memory_order_acquire) == state::finished;
			}

			bool try_detach() noexcept
			{
				return m_state.exchange(
					state::consumer_detached,
					std::memory_order_acq_rel) == state::running;
			}

			bool try_await(std::experimental::coroutine_handle<> awaiter)
			{
				m_awaiter = awaiter;

				state oldState = state::running;
				return m_state.compare_exchange_strong(
					oldState,
					state::consumer_suspended,
					std::memory_order_release,
					std::memory_order_acquire);
			}

		protected:

			bool completed_with_unhandled_exception()
			{
				return m_exception != nullptr;
			}

			void rethrow_if_unhandled_exception()
			{
				if (m_exception != nullptr)
				{
					std::rethrow_exception(m_exception);
				}
			}

		private:

			enum class state
			{
				running,
				consumer_suspended,
				consumer_detached,
				finished
			};

			std::atomic<state> m_state;
			std::experimental::coroutine_handle<> m_awaiter;
			std::exception_ptr m_exception;

		};

		template<typename T>
		class task_promise : public task_promise_base
		{
		public:

			task_promise() noexcept = default;

			~task_promise()
			{
				if (!completed_with_unhandled_exception())
				{
					reinterpret_cast<T*>(&m_valueStorage)->~T();
				}
			}

			task<T> get_return_object() noexcept;

			template<
				typename VALUE,
				typename = std::enable_if_t<std::is_convertible_v<VALUE&&, T>>>
				void return_value(VALUE&& value)
				noexcept(std::is_nothrow_constructible_v<T, VALUE&&>)
			{
				new (&m_valueStorage) T(std::forward<VALUE>(value));
			}

			T& result() &
			{
				rethrow_if_unhandled_exception();
				return *reinterpret_cast<T*>(&m_valueStorage);
			}

			T&& result() &&
			{
				rethrow_if_unhandled_exception();
				return std::move(*reinterpret_cast<T*>(&m_valueStorage));
			}

		private:

			// Not using std::aligned_storage here due to bug in MSVC 2015 Update 2
			// that means it doesn't work for types with alignof(T) > 8.
			// See MS-Connect bug #2658635.
			alignas(T) char m_valueStorage[sizeof(T)];

		};

		template<>
		class task_promise<void> : public task_promise_base
		{
		public:

			task_promise() noexcept = default;

			task<void> get_return_object() noexcept;

			void return_void() noexcept
			{}

			void result()
			{
				rethrow_if_unhandled_exception();
			}

		};

		template<typename T>
		class task_promise<T&> : public task_promise_base
		{
		public:

			task_promise() noexcept = default;

			task<T&> get_return_object() noexcept;

			void return_value(T& value) noexcept
			{
				m_value = std::addressof(value);
			}

			T& result()
			{
				rethrow_if_unhandled_exception();
				return *m_value;
			}

		private:

			T* m_value;

		};
	}

	template<typename T = void>
	class task
	{
	public:

		using promise_type = detail::task_promise<T>;

	private:

		struct awaitable_base
		{
			std::experimental::coroutine_handle<promise_type> m_coroutine;

			awaitable_base(std::experimental::coroutine_handle<promise_type> coroutine) noexcept
				: m_coroutine(coroutine)
			{}

			bool await_ready() const noexcept
			{
				return !m_coroutine || m_coroutine.promise().is_ready();
			}

			bool await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept
			{
				return m_coroutine.promise().try_await(awaiter);
			}
		};

	public:

		task() noexcept
			: m_coroutine(nullptr)
		{}

		explicit task(std::experimental::coroutine_handle<promise_type> coroutine)
			: m_coroutine(coroutine)
		{}

		task(task&& t) noexcept
			: m_coroutine(t.m_coroutine)
		{
			t.m_coroutine = nullptr;
		}

		/// Disable copy construction/assignment.
		task(const task&) = delete;
		task& operator=(const task&) = delete;

		/// Frees resources used by this task.
		///
		/// Calls std::terminate() if the task is not complete and
		/// has not been detached (by calling detach() or moving into
		/// another task).
		~task()
		{
			destroy();
		}

		/// Move assignment.
		task& operator=(task&& other) noexcept
		{
			if (std::addressof(other) != this)
			{
				destroy();

				m_coroutine = other.m_coroutine;
				other.m_coroutine = nullptr;
			}

			return *this;
		}

		/// \brief
		/// Query if the task result is complete.
		///
		/// Awaiting a task that is ready will not block.
		bool is_ready() const noexcept
		{
			return !m_coroutine || m_coroutine.promise().is_ready();
		}

		/// \brief
		/// Detach this task value from the coroutine.
		///
		/// You will not be able to await the result of the task after this.
		void detach()
		{
			if (m_coroutine)
			{
				auto coro = m_coroutine;
				m_coroutine = nullptr;

				if (!coro.promise().try_detach())
				{
					coro.destroy();
				}
			}
		}

		auto operator co_await() const & noexcept
		{
			struct awaitable : awaitable_base
			{
				using awaitable_base::awaitable_base;

				decltype(auto) await_resume()
				{
					if (!this->m_coroutine)
					{
						throw broken_promise{};
					}

					return this->m_coroutine.promise().result();
				}
			};

			return awaitable{ m_coroutine };
		}

		auto operator co_await() const && noexcept
		{
			struct awaitable : awaitable_base
			{
				using awaitable_base::awaitable_base;

				decltype(auto) await_resume()
				{
					if (!this->m_coroutine)
					{
						throw broken_promise{};
					}

					return std::move(this->m_coroutine.promise()).result();
				}
			};

			return awaitable{ m_coroutine };
		}

		/// \brief
		/// Returns an awaitable that will await completion of the task without
		/// attempting to retrieve the result.
		auto when_ready() const noexcept
		{
			struct awaitable : awaitable_base
			{
				using awaitable_base::awaitable_base;

				void await_resume() const noexcept {}
			};

			return awaitable{ m_coroutine };
		}

	private:

		void destroy() noexcept
		{
			if (m_coroutine)
			{
				if (!m_coroutine.promise().is_ready())
				{
					std::terminate();
				}

				m_coroutine.destroy();
			}
		}

		std::experimental::coroutine_handle<promise_type> m_coroutine;

	};

	template<typename T>
	task<T> detail::task_promise<T>::get_return_object() noexcept
	{
		return task<T>{ std::experimental::coroutine_handle<task_promise<T>>::from_promise(*this) };
	}

	template<typename T>
	task<T&> detail::task_promise<T&>::get_return_object() noexcept
	{
		return task<T&>{ std::experimental::coroutine_handle<task_promise<T&>>::from_promise(*this) };
	}

	inline task<void> detail::task_promise<void>::get_return_object() noexcept
	{
		return task<void>{ std::experimental::coroutine_handle<task_promise<void>>::from_promise(*this) };
	}

	namespace detail
	{
		template<typename T, typename FUNC>
		task<std::result_of_t<FUNC&&(T&&)>> apply_fmap(task<T> t, FUNC func)
		{
			static_assert(
				!std::is_reference_v<FUNC>,
				"Passing by reference to task<T> coroutine is unsafe. "
				"Use std::ref or std::cref to explicitly pass by reference.");

			co_return std::invoke(std::move(func), co_await std::move(t));
		}

		template<typename FUNC>
		task<std::result_of_t<FUNC&&()>> apply_fmap(task<> t, FUNC func)
		{
			static_assert(
				!std::is_reference_v<FUNC>,
				"Passing by reference to task<T> coroutine is unsafe. "
				"Use std::ref or std::cref to explicitly pass by reference.");

			co_await t;
			co_return std::invoke(std::move(func));
		}
	}

	template<typename T, typename FUNC>
	auto operator|(task<T>&& t, fmap_transform<FUNC>&& transform)
	{
		return detail::apply_fmap(std::move(t), std::forward<FUNC>(transform.func));
	}

	template<typename T, typename FUNC>
	auto operator|(task<T>&& t, fmap_transform<FUNC>& transform)
	{
		return detail::apply_fmap(std::move(t), transform.func);
	}

	template<typename T, typename FUNC>
	auto operator|(task<T>&& t, const fmap_transform<FUNC>& transform)
	{
		return detail::apply_fmap(std::move(t), transform.func);
	}
}

#endif
