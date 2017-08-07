///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_LAZY_TASK_HPP_INCLUDED
#define CPPCORO_LAZY_TASK_HPP_INCLUDED

#include <cppcoro/broken_promise.hpp>
#include <cppcoro/fmap.hpp>

#include <cppcoro/detail/continuation.hpp>

#include <atomic>
#include <exception>
#include <utility>
#include <type_traits>
#include <cassert>

#include <experimental/coroutine>

namespace cppcoro
{
	template<typename T> class lazy_task;

	namespace detail
	{
		template<typename T>
		class lazy_task_promise final
		{
		public:

			lazy_task_promise() noexcept
				: m_valueState(value_state::empty)
			{}

			~lazy_task_promise()
			{
				switch (m_valueState)
				{
					case value_state::value:
						value().~T();
						break;

					case value_state::exception:
						exception().~exception_ptr();
						break;

					case value_state::empty:
						break;
				}
			}

			lazy_task<T> get_return_object() noexcept;

			std::experimental::suspend_always initial_suspend() noexcept { return{}; }

			auto final_suspend() noexcept;

			void unhandled_exception() noexcept
			{
				new (&m_valueStorage) std::exception_ptr(std::current_exception());
				m_valueState = value_state::exception;
			}

			template<
				typename VALUE,
				typename = std::enable_if_t<std::is_convertible_v<VALUE&&, T>>>
			void return_value(VALUE&& value)
				noexcept(std::is_nothrow_constructible_v<T, VALUE&&>)
			{
				new (&m_valueStorage) T(std::forward<VALUE>(value));
				m_valueState = value_state::value;
			}

			// Support tail-call co_return
			void return_value(lazy_task<T>&& tailTask) noexcept;

			// Disallow tail-call by returning l-value reference.
			void return_value(const lazy_task<T>&) = delete;

			void set_continuation(detail::continuation continuation, const lazy_task<T>* awaitingTask) noexcept
			{
				m_continuation = continuation;
				m_awaitingTask = awaitingTask;
			}

			T& result() &
			{
				rethrow_if_unhandled_exception();
				return value();
			}

			T&& result() &&
			{
				rethrow_if_unhandled_exception();
				return std::move(value());
			}

		private:

			T& value() noexcept
			{
				assert(m_valueState == value_state::value);
				return *reinterpret_cast<T*>(&m_valueStorage);
			}

			std::exception_ptr& exception() noexcept
			{
				assert(m_valueState == value_state::exception);
				return *reinterpret_cast<std::exception_ptr*>(&m_valueStorage);
			}

			void rethrow_if_unhandled_exception()
			{
				if (m_valueState == value_state::exception)
				{
					std::rethrow_exception(exception());
				}
			}

			static constexpr size_t value_size =
				sizeof(T) > sizeof(std::exception_ptr) ?
				sizeof(T) : sizeof(std::exception_ptr);
			static constexpr size_t value_alignment =
				alignof(T) > alignof(std::exception_ptr) ?
				alignof(T) : alignof(std::exception_ptr);

			enum class value_state
			{
				empty,
				value,
				exception
			};
			
			detail::continuation m_continuation;
			const lazy_task<T>* m_awaitingTask;
			value_state m_valueState;

			// Not using std::aligned_storage here due to bug in MSVC 2015 Update 2
			// that means it doesn't work for types with alignof(T) > 8.
			// See MS-Connect bug #2658635.
			alignas(value_alignment) char m_valueStorage[value_size];

		};

		template<>
		class lazy_task_promise<void> final
		{
		public:

			lazy_task_promise() noexcept = default;

			~lazy_task_promise() = default;

			lazy_task<void> get_return_object() noexcept;

			std::experimental::suspend_always initial_suspend() noexcept { return{}; }

			auto final_suspend() noexcept
			{
				struct awaitable
				{
					bool await_ready() noexcept { return false; }

					std::experimental::coroutine_handle<> await_suspend(
						std::experimental::coroutine_handle<lazy_task_promise<void>> coroutine) noexcept
					{
						return coroutine.promise().m_continuation.tail_call_resume();
					}

					void await_resume() noexcept {}
				};

				return awaitable{};
			}

			void return_void() noexcept
			{}

			void unhandled_exception()
			{
				m_exception = std::current_exception();
			}

			void set_continuation(
				detail::continuation continuation,
				[[maybe_unused]] const lazy_task<void>* awaitingTask) noexcept
			{
				m_continuation = continuation;
			}

			void result()
			{
				if (m_exception)
				{
					std::rethrow_exception(m_exception);
				}
			}

		private:

			// TODO: Support tail-calls for lazy_task<void>
			// We currently can't since Coroutines TS doesn't allow both
			// return_void() and return_value() on same promise_type.

			detail::continuation m_continuation;
			std::exception_ptr m_exception;

		};

		template<typename T>
		class lazy_task_promise<T&>
		{
		public:

			lazy_task_promise() noexcept = default;

			lazy_task<T&> get_return_object() noexcept;

			std::experimental::suspend_always initial_suspend() noexcept { return {}; }

			auto final_suspend() noexcept;

			void return_value(T& value) noexcept
			{
				m_value = std::addressof(value);
			}

			void return_value(lazy_task<T&>&& tailCallTask) noexcept;

			void unhandled_exception() noexcept
			{
				m_exception = std::current_exception();
			}

			void set_continuation(detail::continuation continuation, const lazy_task<T&>* awaitingTask) noexcept
			{
				m_continuation = continuation;
				m_awaitingTask = awaitingTask;
			}

			T& result()
			{
				if (m_exception)
				{
					std::rethrow_exception(m_exception);
				}

				return *m_value;
			}

		private:

			detail::continuation m_continuation;
			const lazy_task<T&>* m_awaitingTask;
			std::exception_ptr m_exception;
			T* m_value;

		};
	}

	/// \brief
	/// A lazy task represents an asynchronous operation that is not started
	/// until it is first awaited.
	///
	/// When you call a coroutine that returns a lazy_task, the coroutine
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
	/// The awaiting coroutine is suspended prior to the lazy_task being started
	/// which means that when the lazy_task completes it can unconditionally
	/// resume the awaiter.
	template<typename T = void>
	class lazy_task
	{
	public:

		using promise_type = detail::lazy_task_promise<T>;

		using value_type = T;

	private:

    struct awaitable_base;
		friend struct awaitable_base;

		struct awaitable_base
		{
			const lazy_task* m_task;

			awaitable_base(const lazy_task& task) noexcept
				: m_task(std::addressof(task))
			{}

			bool await_ready() const noexcept
			{
				return m_task->is_ready();
			}

			auto await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept
			{
				auto coroutineHandle = m_task->m_coroutine;
				coroutineHandle.promise().set_continuation(detail::continuation{ awaiter }, m_task);
				return coroutineHandle;
			}

		};

	public:

		lazy_task() noexcept
			: m_coroutine(nullptr)
		{}

		explicit lazy_task(std::experimental::coroutine_handle<promise_type> coroutine)
			: m_coroutine(coroutine)
		{}

		lazy_task(lazy_task&& t) noexcept
			: m_coroutine(t.m_coroutine)
		{
			t.m_coroutine = nullptr;
		}

		/// Disable copy construction/assignment.
		lazy_task(const lazy_task&) = delete;
		lazy_task& operator=(const lazy_task&) = delete;

		/// Frees resources used by this task.
		~lazy_task()
		{
			if (m_coroutine)
			{
				m_coroutine.destroy();
			}
		}

		lazy_task& operator=(lazy_task&& other) noexcept
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
		/// Awaiting a task that is ready will not block.
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
					auto coroutine = this->m_task->m_coroutine;
					if (!coroutine)
					{
						throw broken_promise{};
					}

					return coroutine.promise().result();
				}
			};

			return awaitable{ *this };
		}

		auto operator co_await() const && noexcept
		{
			struct awaitable : awaitable_base
			{
				using awaitable_base::awaitable_base;

				decltype(auto) await_resume()
				{
					auto coroutine = this->m_task->m_coroutine;
					if (!coroutine)
					{
						throw broken_promise{};
					}

					return std::move(coroutine.promise()).result();
				}
			};

			return awaitable{ *this };
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

			return awaitable{ *this };
		}

		// Internal helper method for when_all() implementation.
		auto get_starter() const noexcept
		{
			class starter
			{
			public:

				explicit starter(const lazy_task<T>& task) noexcept
					: m_task(std::addressof(task))
				{}

				void start(detail::continuation c) noexcept
				{
					if (!m_task->is_ready())
					{
						m_task->m_coroutine.promise().set_continuation(c, m_task);
						m_task->m_coroutine.resume();
					}
					else
					{
						c.resume();
					}
				}

			private:

				const lazy_task<T>* m_task;
			};

			return starter{ *this };
		}

	private:

		friend class detail::lazy_task_promise<T>;

		mutable std::experimental::coroutine_handle<promise_type> m_coroutine;

	};

	namespace detail
	{
		template<typename T>
		lazy_task<T> lazy_task_promise<T>::get_return_object() noexcept
		{
			return lazy_task<T>{ std::experimental::coroutine_handle<lazy_task_promise>::from_promise(*this) };
		}

		template<typename T>
		void lazy_task_promise<T>::return_value(lazy_task<T>&& tailCallTask) noexcept
		{
			assert(m_awaitingTask != nullptr);
			assert(m_awaitingTask->m_coroutine);
			assert(std::addressof(m_awaitingTask->m_coroutine.promise()) == this);

			// Transfer ownership of the tail call task's coroutine to the
			// awaiting task which was previously pointing to this coroutine.
			// We will detect this case in final_suspend() where we will destroy
			// this coroutine before resuming the tail-call one.
			// We can't destroy this coroutine here because it's not yet suspended.

			m_awaitingTask->m_coroutine = tailCallTask.m_coroutine;
			tailCallTask.m_coroutine = std::experimental::coroutine_handle<lazy_task_promise>{};
		}

		template<typename T>
		auto lazy_task_promise<T>::final_suspend() noexcept
		{
			struct awaitable
			{
				bool await_ready() noexcept { return false; }

				std::experimental::coroutine_handle<> await_suspend(std::experimental::coroutine_handle<lazy_task_promise> coroutine) noexcept
				{
					auto& promise = coroutine.promise();
					auto* awaitingTask = promise.m_awaitingTask;
					if (awaitingTask->m_coroutine == coroutine)
					{
						// Normal completion (non tail-call)
						return promise.m_continuation.tail_call_resume();
					}
					else
					{
						// Tail-call completion

						// Take a copy of the continuation before destroying the coroutine.
						auto continuation = promise.m_continuation;

						coroutine.destroy();
						
						if (awaitingTask->is_ready())
						{
							return continuation.tail_call_resume();
						}
						else
						{
							awaitingTask->m_coroutine.promise().set_continuation(continuation, awaitingTask);
							return awaitingTask->m_coroutine;
						}
					}
				}

				void await_resume() noexcept {}
			};

			return awaitable{};
		}

		inline lazy_task<void> lazy_task_promise<void>::get_return_object() noexcept
		{
			return lazy_task<void>{ std::experimental::coroutine_handle<lazy_task_promise>::from_promise(*this) };
		}

		template<typename T>
		lazy_task<T&> lazy_task_promise<T&>::get_return_object() noexcept
		{
			return lazy_task<T&>{ std::experimental::coroutine_handle<lazy_task_promise>::from_promise(*this) };
		}

		template<typename T>
		void lazy_task_promise<T&>::return_value(lazy_task<T&>&& tailCallTask) noexcept
		{
			assert(m_awaitingTask != nullptr);
			assert(m_awaitingTask->m_coroutine);
			assert(std::addressof(m_awaitingTask->m_coroutine.promise()) == this);

			// Transfer ownership of the tail call task's coroutine to the
			// awaiting task which was previously pointing to this coroutine.
			// We will detect this case in final_suspend() where we will destroy
			// this coroutine before resuming the tail-call one.
			// We can't destroy this coroutine here because it's not yet suspended.

			m_awaitingTask->m_coroutine = tailCallTask.m_coroutine;
			tailCallTask.m_coroutine = std::experimental::coroutine_handle<lazy_task_promise>{};
		}

		template<typename T>
		auto lazy_task_promise<T&>::final_suspend() noexcept
		{
			struct awaitable
			{
				bool await_ready() noexcept { return false; }

				std::experimental::coroutine_handle<> await_suspend(std::experimental::coroutine_handle<lazy_task_promise> coroutine) noexcept
				{
					auto& promise = coroutine.promise();
					auto* awaitingTask = promise.m_awaitingTask;
					if (awaitingTask->m_coroutine == coroutine)
					{
						// Normal completion (non tail-call)
						return promise.m_continuation.tail_call_resume();
					}
					else
					{
						// Tail-call completion

						// Take a copy of the continuation before destroying the coroutine.
						auto continuation = promise.m_continuation;

						coroutine.destroy();
						
						if (awaitingTask->is_ready())
						{
							return continuation.tail_call_resume();
						}
						else
						{
							awaitingTask->m_coroutine.promise().set_continuation(continuation, awaitingTask);
							return awaitingTask->m_coroutine;
						}
					}
				}

				void await_resume() noexcept {}
			};

			return awaitable{};
		}
	}

	// operator| for fmap_transform

	namespace detail
	{
		template<typename T, typename FUNC>
		lazy_task<std::result_of_t<FUNC&&(T&&)>> apply_fmap(lazy_task<T> t, FUNC func)
		{
			static_assert(
				!std::is_reference_v<FUNC>,
				"Passing by reference to lazy_task<T> coroutine is unsafe. "
				"Use std::ref or std::cref to explicitly pass by reference.");

			co_return std::invoke(std::move(func), co_await std::move(t));
		}

		template<typename FUNC>
		lazy_task<std::result_of_t<FUNC&&()>> apply_fmap(lazy_task<> t, FUNC func)
		{
			static_assert(
				!std::is_reference_v<FUNC>,
				"Passing by reference to lazy_task<T> coroutine is unsafe. "
				"Use std::ref or std::cref to explicitly pass by reference.");

			co_await t;
			co_return std::invoke(std::move(func));
		}
	}

	template<typename T, typename FUNC>
	auto operator|(lazy_task<T>&& t, fmap_transform<FUNC>&& transform)
	{
		return detail::apply_fmap(std::move(t), std::forward<FUNC>(transform.func));
	}

	template<typename T, typename FUNC>
	auto operator|(lazy_task<T>&& t, fmap_transform<FUNC>& transform)
	{
		return detail::apply_fmap(std::move(t), transform.func);
	}

	template<typename T, typename FUNC>
	auto operator|(lazy_task<T>&& t, const fmap_transform<FUNC>& transform)
	{
		return detail::apply_fmap(std::move(t), transform.func);
	}
}

#endif
