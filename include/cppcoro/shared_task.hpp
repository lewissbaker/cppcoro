///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_SHARED_TASK_HPP_INCLUDED
#define CPPCORO_SHARED_TASK_HPP_INCLUDED

#include <cppcoro/broken_promise.hpp>
#include <cppcoro/task.hpp>

#include <atomic>
#include <exception>
#include <utility>
#include <type_traits>

#include <experimental/coroutine>

namespace cppcoro
{
	template<typename T>
	class task;

	template<typename T>
	class shared_task;

	namespace detail
	{
		struct shared_task_waiter
		{
			std::experimental::coroutine_handle<> m_coroutine;
			shared_task_waiter* m_next;
		};

		class shared_task_promise_base
		{
		public:

			shared_task_promise_base() noexcept
				: m_waiters(nullptr)
				, m_refCount(2)
				, m_exception(nullptr)
			{}

			auto initial_suspend() noexcept
			{
				return std::experimental::suspend_never{};
			}

			auto final_suspend() noexcept
			{
				struct awaitable
				{
					shared_task_promise_base& m_promise;

					awaitable(shared_task_promise_base& promise) noexcept
						: m_promise(promise)
					{}

					bool await_ready() const noexcept
					{
						return m_promise.m_refCount.load(std::memory_order_acquire) == 1;
					}

					bool await_suspend(std::experimental::coroutine_handle<>) noexcept
					{
						return m_promise.m_refCount.fetch_sub(1, std::memory_order_acq_rel) > 1;
					}

					void await_resume() noexcept
					{}
				};

				void* waiters = m_waiters.exchange(static_cast<void*>(this), std::memory_order_acq_rel);
				if (waiters != nullptr)
				{
					shared_task_waiter* next = static_cast<shared_task_waiter*>(waiters);
					do
					{
						// Read the m_next pointer before resuming the coroutine
						// since resuming the coroutine may destroy the shared_task_waiter value.
						auto coroutine = next->m_coroutine;
						next = next->m_next;
						coroutine.resume();
					} while (next != nullptr);
				}

				return awaitable{ *this };
			}

			void unhandled_exception() noexcept
			{
				// No point capturing exception if no more references to the task.
				if (m_refCount.load(std::memory_order_relaxed) > 1)
				{
					m_exception = std::current_exception();
				}
			}

			void set_exception(std::exception_ptr exception)
			{
				m_exception = std::move(exception);
			}

			bool is_ready() const noexcept
			{
				return m_waiters.load(std::memory_order_acquire) == static_cast<const void*>(this);
			}

			void add_ref() noexcept
			{
				m_refCount.fetch_add(1, std::memory_order_relaxed);
			}

			/// Decrement the reference count.
			///
			/// \return
			/// true if successfully detached, false if this was the last
			/// reference to the coroutine, in which case the caller must
			/// call destroy() on the coroutine handle.
			bool try_detach() noexcept
			{
				return m_refCount.fetch_sub(1, std::memory_order_acq_rel) > 1;
			}

			/// Try to enqueue a waiter to the list of waiters.
			///
			/// \return
			/// true if the waiter was successfully queued, in which case
			/// waiter->m_coroutine will be resumed when the task completes.
			/// false if the coroutine was already completed and the awaiting
			/// coroutine can continue without suspending.
			bool try_await(shared_task_waiter* waiter)
			{
				void* oldWaiters = m_waiters.load(std::memory_order_acquire);
				do
				{
					if (oldWaiters == static_cast<void*>(this))
					{
						// Coroutine already completed, don't suspend.
						return false;
					}

					waiter->m_next = static_cast<shared_task_waiter*>(oldWaiters);
				} while (!m_waiters.compare_exchange_weak(
					oldWaiters,
					static_cast<void*>(waiter),
					std::memory_order_release,
					std::memory_order_acquire));

				return true;
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

			std::atomic<std::uint32_t> m_refCount;

			// Value is either
			// - nullptr - indicates no waiters
			// - this - indicates value is ready
			// - other - pointer to head item in linked-list of waiters.
			//           values are of type 'cppcoro::shared_task_waiter'.
			std::atomic<void*> m_waiters;

			std::exception_ptr m_exception;

		};

		template<typename T>
		class shared_task_promise : public shared_task_promise_base
		{
		public:

			shared_task_promise() noexcept = default;

			~shared_task_promise()
			{
				if (!completed_with_unhandled_exception())
				{
					reinterpret_cast<T*>(&m_valueStorage)->~T();
				}
			}

			auto get_return_object() noexcept
			{
				return std::experimental::coroutine_handle<shared_task_promise>::from_promise(*this);
			}

			template<
				typename VALUE,
				typename = std::enable_if_t<std::is_convertible_v<VALUE&&, T>>>
			void return_value(VALUE&& value)
				noexcept(std::is_nothrow_constructible_v<T, VALUE&&>)
			{
				new (&m_valueStorage) T(std::forward<VALUE>(value));
			}

			T& result()
			{
				rethrow_if_unhandled_exception();
				return *reinterpret_cast<T*>(&m_valueStorage);
			}

		private:

			// Not using std::aligned_storage here due to bug in MSVC 2015 Update 2
			// that means it doesn't work for types with alignof(T) > 8.
			// See MS-Connect bug #2658635.
			alignas(T) char m_valueStorage[sizeof(T)];

		};

		template<>
		class shared_task_promise<void> : public shared_task_promise_base
		{
		public:

			shared_task_promise() noexcept = default;

			auto get_return_object() noexcept
			{
				return std::experimental::coroutine_handle<shared_task_promise>::from_promise(*this);
			}

			void return_void() noexcept
			{}

			void result()
			{
				rethrow_if_unhandled_exception();
			}

		};

		template<typename T>
		class shared_task_promise<T&> : public shared_task_promise_base
		{
		public:

			shared_task_promise() noexcept = default;

			auto get_return_object() noexcept
			{
				return std::experimental::coroutine_handle<shared_task_promise>::from_promise(*this);
			}

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
	class shared_task
	{
	public:

		using promise_type = detail::shared_task_promise<T>;

	private:

		struct awaitable_base
		{
			std::experimental::coroutine_handle<promise_type> m_coroutine;
			detail::shared_task_waiter m_waiter;

			awaitable_base(std::experimental::coroutine_handle<promise_type> coroutine) noexcept
				: m_coroutine(coroutine)
			{}

			bool await_ready() const noexcept
			{
				return !m_coroutine || m_coroutine.promise().is_ready();
			}

			bool await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept
			{
				m_waiter.m_coroutine = awaiter;
				return m_coroutine.promise().try_await(&m_waiter);
			}
		};

	public:

		shared_task() noexcept
			: m_coroutine(nullptr)
		{}

		explicit shared_task(std::experimental::coroutine_handle<promise_type> coroutine)
			: m_coroutine(coroutine)
		{
			// Don't increment the ref-count here since it has already been
			// initialised to 2 (one for shared_task and one for coroutine)
			// in the shared_task_promise constructor.
		}

		shared_task(shared_task&& other) noexcept
			: m_coroutine(other.m_coroutine)
		{
			other.m_coroutine = nullptr;
		}

		shared_task(const shared_task& other) noexcept
			: m_coroutine(other.m_coroutine)
		{
			if (m_coroutine)
			{
				m_coroutine.promise().add_ref();
			}
		}

		~shared_task()
		{
			destroy();
		}

		shared_task& operator=(shared_task&& other) noexcept
		{
			if (&other != this)
			{
				destroy();

				m_coroutine = other.m_coroutine;
				other.m_coroutine = nullptr;
			}

			return *this;
		}

		shared_task& operator=(const shared_task& other) noexcept
		{
			if (m_coroutine != other.m_coroutine)
			{
				destroy();

				m_coroutine = other.m_coroutine;

				if (m_coroutine)
				{
					m_coroutine.promise().add_ref();
				}
			}

			return *this;
		}

		void swap(shared_task& other) noexcept
		{
			std::swap(m_coroutine, other.m_coroutine);
		}

		/// \brief
		/// Query if the task result is complete.
		///
		/// Awaiting a task that is ready will not block.
		bool is_ready() const noexcept
		{
			return !m_coroutine || m_coroutine.promise().is_ready();
		}

		auto operator co_await() const noexcept
		{
			struct awaitable : awaitable_base
			{
				using awaitable_base::awaitable_base;

				decltype(auto) await_resume()
				{
					if (!m_coroutine)
					{
						throw broken_promise{};
					}

					return m_coroutine.promise().result();
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

		template<typename U>
		friend bool operator==(const shared_task<U>&, const shared_task<U>&) noexcept;

		void destroy() noexcept
		{
			if (m_coroutine)
			{
				if (!m_coroutine.promise().try_detach())
				{
					m_coroutine.destroy();
				}
			}
		}

		std::experimental::coroutine_handle<promise_type> m_coroutine;

	};

	template<typename T>
	bool operator==(const shared_task<T>& lhs, const shared_task<T>& rhs) noexcept
	{
		return lhs.m_coroutine == rhs.m_coroutine;
	}

	template<typename T>
	bool operator!=(const shared_task<T>& lhs, const shared_task<T>& rhs) noexcept
	{
		return !(lhs == rhs);
	}

	template<typename T>
	void swap(shared_task<T>& a, shared_task<T>& b) noexcept
	{
		a.swap(b);
	}

	template<typename T>
	shared_task<T> make_shared_task(task<T> t)
	{
		co_return co_await std::move(t);
	}

#if defined(_MSC_VER) && _MSC_FULL_VER <= 191025019
	// HACK: Work around bug in MSVC handling of 'co_return <expr>' for void-returning expression.
	// It doesn't actually evaluate <expr> before calling <promise>.return_void().
	inline shared_task<void> make_shared_task(task<void> t)
	{
		co_await t;
	}
#endif
}

#endif
