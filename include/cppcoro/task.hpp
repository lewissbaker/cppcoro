///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_LAZY_TASK_HPP_INCLUDED
#define CPPCORO_LAZY_TASK_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/broken_promise.hpp>
#include <cppcoro/fmap.hpp>

#include <cppcoro/detail/continuation.hpp>

#include <atomic>
#include <exception>
#include <utility>
#include <type_traits>
#include <cstdint>

#include <experimental/coroutine>

namespace cppcoro
{
	template<typename T> class task;

	namespace detail
	{
		class task_promise_base
		{
			friend struct final_awaitable;

			struct final_awaitable
			{
				bool await_ready() const noexcept { return false; }

				template<typename PROMISE>
				void await_suspend(std::experimental::coroutine_handle<PROMISE> coroutine)
				{
					task_promise_base& promise = coroutine.promise();

					// Use 'release' memory semantics in case we finish before the
					// awaiter can suspend so that the awaiting thread sees our
					// writes to the resulting value.
					// Use 'acquire' memory semantics in case the caller registered
					// the continuation before we finished. Ensure we see their write
					// to m_continuation.
					if (promise.m_state.exchange(true, std::memory_order_acq_rel))
					{
						promise.m_continuation.resume();
					}
				}

				void await_resume() noexcept {}
			};

		public:

			task_promise_base() noexcept
				: m_state(false)
			{}

			auto initial_suspend() noexcept
			{
				return std::experimental::suspend_always{};
			}

			auto final_suspend() noexcept
			{
				return final_awaitable{};
			}

			void unhandled_exception() noexcept
			{
				m_exception = std::current_exception();
			}

			bool try_set_continuation(continuation c)
			{
				m_continuation = c;
				return !m_state.exchange(true, std::memory_order_acq_rel);
			}

		protected:

			bool completed() const noexcept
			{
				return m_state.load(std::memory_order_relaxed);
			}

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

			continuation m_continuation;
			std::exception_ptr m_exception;

			// Initially false. Set to true when either a continuation is registered
			// or when the coroutine has run to completion. Whichever operation
			// successfully transitions from false->true got there first.
			std::atomic<bool> m_state;

		};

		template<typename T>
		class task_promise final : public task_promise_base
		{
		public:

			task_promise() noexcept = default;

			~task_promise()
			{
				if (completed() && !completed_with_unhandled_exception())
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

#if CPPCORO_COMPILER_MSVC
# pragma warning(push)
# pragma warning(disable : 4324) // structure was padded due to alignment.
#endif

			// Not using std::aligned_storage here due to bug in MSVC 2015 Update 2
			// that means it doesn't work for types with alignof(T) > 8.
			// See MS-Connect bug #2658635.
			alignas(T) char m_valueStorage[sizeof(T)];

#if CPPCORO_COMPILER_MSVC
# pragma warning(pop)
#endif

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

	/// \brief
	/// A lazy task represents an asynchronous operation that is not started
	/// until it is first awaited.
	///
	/// When you call a coroutine that returns a task, the coroutine
	/// simply captures any passed parameters and returns exeuction to the
	/// caller. Execution of the coroutine body does not start until the
	/// coroutine is first co_await'ed.
	///
	/// Comparison with task<T>
	/// -----------------------
	/// The lazy task has lower overhead than cppcoro::task<T> as it does not
	/// require the use of atomic operations to synchronise potential races
	/// between the awaiting coroutine suspending and the coroutine completing.
	///
	/// The awaiting coroutine is suspended prior to the task being started
	/// which means that when the task completes it can unconditionally
	/// resume the awaiter.
	///
	/// One limitation of this approach is that if the task completes
	/// synchronously then, unless the compiler is able to perform tail-calls,
	/// the awaiting coroutine will be resumed inside a nested stack-frame.
	/// This call lead to stack-overflow if long chains of tasks complete
	/// synchronously.
	///
	/// The task<T> type does not have this issue as the awaiting coroutine is
	/// not suspended in the case that the task completes synchronously.
	template<typename T = void>
	class task
	{
	public:

		using promise_type = detail::task_promise<T>;

		using value_type = T;

	private:

		struct awaitable_base
		{
			std::experimental::coroutine_handle<promise_type> m_coroutine;

			awaitable_base(std::experimental::coroutine_handle<promise_type> coroutine) noexcept
				: m_coroutine(coroutine)
			{}

			bool await_ready() const noexcept
			{
				return !m_coroutine || m_coroutine.done();
			}

			bool await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept
			{
				m_coroutine.resume();
				return m_coroutine.promise().try_set_continuation(detail::continuation{ awaiter });
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
		~task()
		{
			if (m_coroutine)
			{
				m_coroutine.destroy();
			}
		}

		task& operator=(task&& other) noexcept
		{
			if (std::addressof(other) != this)
			{
				if (m_coroutine)
				{
					m_coroutine.destroy();
				}

				m_coroutine = other.m_coroutine;
				other.m_coroutine = nullptr;
			}

			return *this;
		}

		/// \brief
		/// Query if the task result is complete.
		///
		/// Awaiting a task that is ready is guaranteed not to block/suspend.
		bool is_ready() const noexcept
		{
			return !m_coroutine || m_coroutine.done();
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

		// Internal helper method for when_all() implementation.
		auto get_starter() const noexcept
		{
			class starter
			{
			public:

				starter(std::experimental::coroutine_handle<promise_type> coroutine) noexcept
					: m_coroutine(coroutine)
				{}

				void start(detail::continuation continuation) noexcept
				{
					if (m_coroutine && !m_coroutine.done())
					{
						m_coroutine.resume();
						if (m_coroutine.promise().try_set_continuation(continuation))
						{
							return;
						}
					}

					continuation.resume();
				}

			private:

				std::experimental::coroutine_handle<promise_type> m_coroutine;
			};

			return starter{ m_coroutine };
		}

	private:

		std::experimental::coroutine_handle<promise_type> m_coroutine;

	};

	namespace detail
	{
		template<typename T>
		task<T> task_promise<T>::get_return_object() noexcept
		{
			return task<T>{ std::experimental::coroutine_handle<task_promise>::from_promise(*this) };
		}

		inline task<void> task_promise<void>::get_return_object() noexcept
		{
			return task<void>{ std::experimental::coroutine_handle<task_promise>::from_promise(*this) };
		}

		template<typename T>
		task<T&> task_promise<T&>::get_return_object() noexcept
		{
			return task<T&>{ std::experimental::coroutine_handle<task_promise>::from_promise(*this) };
		}
	}

	// fmap() overloads for task<T>

	template<typename FUNC, typename T>
	task<std::result_of_t<FUNC&&(T&&)>> fmap(FUNC func, task<T> t)
	{
		static_assert(
			!std::is_reference_v<FUNC>,
			"Passing by reference to task<T> coroutine is unsafe. "
			"Use std::ref or std::cref to explicitly pass by reference.");

		co_return std::invoke(std::move(func), co_await std::move(t));
	}

	template<typename FUNC>
	task<std::result_of_t<FUNC&&()>> fmap(FUNC func, task<> t)
	{
		static_assert(
			!std::is_reference_v<FUNC>,
			"Passing by reference to task<T> coroutine is unsafe. "
			"Use std::ref or std::cref to explicitly pass by reference.");

		co_await t;
		co_return std::invoke(std::move(func));
	}
}

#endif
