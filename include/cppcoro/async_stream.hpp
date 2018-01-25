///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_ASYNC_STREAM_HPP_INCLUDED
#define CPPCORO_ASYNC_STREAM_HPP_INCLUDED

#include <cppcoro/broken_promise.hpp>

#include <experimental/coroutine>
#include <atomic>
#include <exception>
#include <cassert>

namespace cppcoro
{
	template<typename T> class async_stream_subscription;
	template<typename T> class async_stream;
	template<typename T> class async_stream_task;
	template<typename T> class async_stream_iterator;
	template<typename T> class async_stream_increment_operation;
	template<typename T> class async_stream_begin_operation;

	template<typename T>
	class async_stream_promise
	{
		using handle_t = std::experimental::coroutine_handle<async_stream_promise<T>>;

		struct final_suspend_awaitable
		{
			bool await_ready() noexcept { return false; }

			void await_suspend(std::experimental::coroutine_handle<async_stream_promise> coro) noexcept
			{
				auto& promise = coro.promise();

				if (promise.m_streamContinuation)
				{
					// Resume the coroutine that is awaiting the next element.
					promise.m_streamContinuation.resume();
				}

				promise.m_taskContinuation.resume();
			}

			void await_resume() noexcept {}
		};

		struct yield_value_awaitable
		{
			yield_value_awaitable(async_stream_promise& promise) noexcept
				: m_promise(promise)
			{}

			bool await_ready() noexcept
			{
				// Continue without suspending if consumer has detached.
				return static_cast<bool>(!m_promise.m_streamContinuation);
			}

			void await_suspend(std::experimental::coroutine_handle<>) noexcept
			{
				m_promise.m_streamContinuation.resume();
			}

			[[nodiscard]]
			bool await_resume() noexcept
			{
				// true - if the consumer has asked for another value (by attaching a stream continuation)
				// false - if the consumer has detached and does not want another value.
				return static_cast<bool>(m_promise.m_streamContinuation);
			}

		private:

			async_stream_promise& m_promise;

		};

		friend struct final_suspend_awaitable;
		friend class async_stream<T>;
		friend class async_stream_task<T>;
		friend class async_stream_iterator<T>;
		friend class async_stream_increment_operation<T>;
		friend class async_stream_begin_operation<T>;
		
	public:

		async_stream_promise() noexcept
			: m_awaiterFlags(0)
			, m_refCount(2)
		{}

		handle_t get_return_object() noexcept
		{
			return handle_t::from_promise(*this);
		}

		std::experimental::suspend_always initial_suspend() noexcept { return {}; }
		final_suspend_awaitable final_suspend() noexcept { return {}; }

		yield_value_awaitable yield_value(T& value)
		{
			m_value = std::addressof(value);
			return { *this };
		}

		yield_value_awaitable yield_value(T&& value)
		{
			m_value = std::addressof(value);
			return { *this };
		}

		void return_void() noexcept
		{
			m_value = nullptr;
		}

		void unhandled_exception() noexcept
		{
			m_value = nullptr;
			m_exception = std::current_exception();
		}

		void rethrow_if_exception()
		{
			assert(m_value == nullptr);
			if (m_exception)
			{
				std::rethrow_exception(m_exception);
			}
		}

	private:

		static constexpr std::uint8_t task_awaiter_flag = 1;
		static constexpr std::uint8_t stream_awaiter_flag = 2;

		std::atomic<std::uint8_t> m_awaiterFlags;
		std::atomic<std::uint8_t> m_refCount;
		std::experimental::coroutine_handle<> m_taskContinuation;
		std::experimental::coroutine_handle<> m_streamContinuation;
		T* m_value;
		std::exception_ptr m_exception;

	};

	template<typename T>
	class async_stream_iterator;

	template<typename T>
	class async_stream_increment_operation
	{
		using promise_t = async_stream_promise<T>;
		using handle_t = std::experimental::coroutine_handle<promise_t>;

	public:

		async_stream_increment_operation(handle_t coroutine, async_stream_iterator<T>* iterator) noexcept
			: m_handle(coroutine)
			, m_iterator(iterator)
		{}

		bool await_ready() const noexcept { return false; }

		void await_suspend(std::experimental::coroutine_handle<> awaitingCoroutine) noexcept
		{
			m_handle.promise().m_streamContinuation = awaitingCoroutine;
			m_handle.resume();
		}

		async_stream_iterator<T>& await_resume()
		{
			if (m_handle.promise().m_value == nullptr)
			{
				m_handle.promise().rethrow_if_exception();
			}
			return *m_iterator;
		}

	private:

		handle_t m_handle;
		async_stream_iterator<T>* m_iterator;

	};

	struct async_stream_sentinel
	{
	};

	template<typename T>
	class async_stream_iterator
	{
		using promise_t = async_stream_promise<T>;
		using handle_t = std::experimental::coroutine_handle<promise_t>;

	public:

		using iterator_category = std::forward_iterator_tag;
		using value_type = std::remove_reference_t<T>;
		using reference = std::add_lvalue_reference_t<T>;
		using pointer = std::add_pointer_t<T>;
		using difference_type = std::ptrdiff_t;

		async_stream_iterator(handle_t coroutine) noexcept
			: m_coroutine(coroutine)
		{}

		[[nodiscard]]
		async_stream_increment_operation<T> operator++() noexcept
		{
			return async_stream_increment_operation<T>{ m_coroutine, this };
		}

		reference operator*() const noexcept
		{
			return *m_coroutine.promise().m_value;
		}

		pointer operator->() const noexcept
		{
			return m_coroutine.promise().m_value;
		}

		bool operator==(async_stream_sentinel) const noexcept
		{
			// We're at the end if there are no more values.
			return m_coroutine.promise().m_value == nullptr;
		}

		bool operator!=(async_stream_sentinel other) const noexcept
		{
			return !(*this == other);
		}

		bool operator==(const async_stream_iterator& other) const noexcept
		{
			return m_coroutine == other.m_coroutine;
		}

		bool operator!=(const async_stream_iterator& other) const noexcept
		{
			return !(*this == other);
		}

	private:

		handle_t m_coroutine;

	};

	template<typename T>
	class async_stream_begin_operation
	{
		using promise_t = async_stream_promise<T>;
		using handle_t = std::experimental::coroutine_handle<promise_t>;

	public:

		async_stream_begin_operation(handle_t coroutine) noexcept
			: m_coroutine(coroutine)
		{}

		bool await_ready() const noexcept
		{
			return !m_coroutine;
		}

		bool await_suspend(std::experimental::coroutine_handle<> awaitingCoroutine) noexcept
		{
			auto& p = m_coroutine.promise();
			p.m_streamContinuation = awaitingCoroutine;
			if ((p.m_awaiterFlags.fetch_or(
				 promise_t::stream_awaiter_flag,
				 std::memory_order_acq_rel) & promise_t::task_awaiter_flag) != 0)
			{
				// The async_task has already been awaited or detached.
				if (!p.m_taskContinuation)
				{
					// The async_task was destructed without being awaited.
					// We can't start the coroutine so we'll resume
					// the awaiter immediately and throw broken_promise.
					m_coroutine = handle_t{};
					return false;
				}

				m_coroutine.resume();
			}

			return true;
		}

		async_stream_iterator<T> await_resume()
		{
			if (!m_coroutine)
			{
				throw broken_promise{};
			}
			else if (m_coroutine.promise().m_value == nullptr)
			{
				// At end of stream. Check for an exception.
				m_coroutine.promise().rethrow_if_exception();
			}

			return async_stream_iterator<T>{ m_coroutine };
		}

	private:

		handle_t m_coroutine;

	};

	template<typename T>
	class async_stream
	{
		using promise_t = async_stream_promise<T>;
		using handle_t = std::experimental::coroutine_handle<promise_t>;

	public:

		using value_type = std::remove_reference_t<T>;

		async_stream(handle_t coroutine) noexcept
			: m_coroutine(coroutine)
		{}

		async_stream(async_stream&& other) noexcept
			: m_coroutine(std::exchange(other.m_coroutine, {}))
		{}

		~async_stream()
		{
			if (m_coroutine)
			{
				auto& p = m_coroutine.promise();
				if (!p.m_streamContinuation)
				{
					// stream.begin() was not awaited.
					// Therefore the coroutine body was never started.
					if (p.m_awaiterFlags.fetch_or(
						async_stream_promise<T>::stream_awaiter_flag,
						std::memory_order_acq_rel) == async_stream_promise<T>::task_awaiter_flag)
					{
						if (p.m_taskContinuation)
						{
							p.m_taskContinuation.resume();
						}
					}
				}
				else
				{
					p.m_streamContinuation = {};

					// stream.begin() was awaited and we subsequently resumed and
					// were able to destruct the stream. This implies that the async_stream_task
					// was also awaited. We are also assuming that the coroutine is currently
					// suspended since we should not be destructing the stream if there was
					// a pending await on begin() or operator++().
					if (p.m_value != nullptr)
					{
						// Not at final_suspend(), so must be at a co_yield.
						// Resume to allow the coroutine to run to completion.
						m_coroutine.resume();
					}
				}

				if (p.m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
				{
					m_coroutine.destroy();
				}
			}
		}

		async_stream_begin_operation<T> begin() noexcept
		{
			return async_stream_begin_operation<T>{ m_coroutine };
		}

		async_stream_sentinel end() noexcept
		{
			return {};
		}

	private:

		handle_t m_coroutine;

	};

	template<typename T>
	class async_stream_task
	{
		using promise_t = async_stream_promise<T>;
		using handle_t = std::experimental::coroutine_handle<promise_t>;

	public:

		async_stream_task(handle_t coroutine) noexcept
			: m_coroutine(coroutine)
		{}

		async_stream_task(async_stream_task&& other) noexcept
			: m_coroutine(std::exchange(other.m_coroutine, {}))
		{}

		~async_stream_task()
		{
			if (m_coroutine)
			{
				auto& p = m_coroutine.promise();

				if (!p.m_taskContinuation)
				{
					// Task is being destructed without having ever been awaited.
					if (p.m_awaiterFlags.fetch_or(
						async_stream_promise<T>::task_awaiter_flag,
						std::memory_order_acq_rel) == async_stream_promise<T>::stream_awaiter_flag)
					{
						// The corresponding async_stream has either been awaited or destroyed
						if (p.m_streamContinuation)
						{
							// Someone has already awaited stream.begin().
							// Set it up so that `co_await stream.begin()` throws broken_promise{}
							p.m_value = nullptr;
							p.m_exception = std::make_exception_ptr(broken_promise{});
							p.m_streamContinuation.resume();
						}
					}
				}

				if (p.m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
				{
					m_coroutine.destroy();
				}
			}
		}

		async_stream_task& operator=(async_stream_task other) noexcept
		{
			std::swap(m_coroutine, other.m_coroutine);
			return *this;
		}

		bool await_ready() const noexcept
		{
			// Don't suspend if either this task is not associated with
			// a coroutine or if the coroutine has been previously awaited before
			// (it should only have resumed once the coroutine completes).
			return !m_coroutine || static_cast<bool>(m_coroutine.promise().m_taskContinuation);
		}

		bool await_suspend(std::experimental::coroutine_handle<> awaitingCoroutine) noexcept
		{
			auto& p = m_coroutine.promise();
			p.m_taskContinuation = awaitingCoroutine;
			if (p.m_awaiterFlags.fetch_or(async_stream_promise<T>::task_awaiter_flag, std::memory_order_acq_rel) == async_stream_promise<T>::stream_awaiter_flag)
			{
				// The corresponding async_stream has already either been awaited
				// or it has been destroyed and has detached from the coroutine.
				if (!p.m_streamContinuation)
				{
					// The async_stream has been detached.
					// We don't bother starting the coroutine now.
					// Instead we just immediately resume the awaiter of the
					// async_stream_task.
					return false;
				}
				
				// The coroutine can now be started.
				m_coroutine.resume();
			}

			return true;
		}

		void await_resume()
		{
			m_coroutine.promise().rethrow_if_exception();
		}

	private:

		handle_t m_coroutine;

	};

	template<typename T>
	class async_stream_subscription
	{
	public:

		using promise_type = async_stream_promise<T>;
		using handle_t = std::experimental::coroutine_handle<promise_type>;

		async_stream_subscription(handle_t coroutine) noexcept
			: m_task(coroutine)
			, m_stream(coroutine)
		{}

		async_stream_task<T>& task() & { return m_task; }
		async_stream_task<T>&& task() && { return std::move(m_task); }

		async_stream<T>& stream() & { return m_stream; }
		async_stream<T>&& stream() && { return std::move(m_stream); }

		template<size_t I>
		decltype(auto) get() &
		{
			static_assert(I < 2);
			if constexpr (I == 0)
			{
				return stream();
			}
			else
			{
				return task();
			}
		}

		template<size_t I>
		decltype(auto) get() &&
		{
			static_assert(I < 2);
			if constexpr (I == 0)
			{
				return std::move(m_stream);
			}
			else
			{
				return std::move(m_task);
			}
		}

	private:

		async_stream_task<T> m_task;
		async_stream<T> m_stream;

	};
}

namespace std
{
	template<typename T>
	struct tuple_size<cppcoro::async_stream_subscription<T>>
		: std::integral_constant<size_t, 2>
	{
	};

	template<typename T>
	struct tuple_element<0, cppcoro::async_stream_subscription<T>>
	{
		using type = cppcoro::async_stream<T>;
	};

	template<typename T>
	struct tuple_element<1, cppcoro::async_stream_subscription<T>>
	{
		using type = cppcoro::async_stream_task<T>;
	};
}

#endif
