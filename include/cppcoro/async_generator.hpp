///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_ASYNC_GENERATOR_HPP_INCLUDED
#define CPPCORO_ASYNC_GENERATOR_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/fmap.hpp>

#include <exception>
#include <atomic>
#include <iterator>
#include <type_traits>
#include <cppcoro/coroutine.hpp>
#include <functional>
#include <cassert>

namespace cppcoro
{
	template<typename T>
	class async_generator;

#if CPPCORO_COMPILER_SUPPORTS_SYMMETRIC_TRANSFER

	namespace detail
	{
		template<typename T>
		class async_generator_iterator;
		class async_generator_yield_operation;
		class async_generator_advance_operation;

		class async_generator_promise_base
		{
		public:

			async_generator_promise_base() noexcept
				: m_exception(nullptr)
			{
				// Other variables left intentionally uninitialised as they're
				// only referenced in certain states by which time they should
				// have been initialised.
			}

			async_generator_promise_base(const async_generator_promise_base& other) = delete;
			async_generator_promise_base& operator=(const async_generator_promise_base& other) = delete;

			cppcoro::suspend_always initial_suspend() const noexcept
			{
				return {};
			}

			async_generator_yield_operation final_suspend() noexcept;

			void unhandled_exception() noexcept
			{
				m_exception = std::current_exception();
			}

			void return_void() noexcept
			{
			}

			/// Query if the generator has reached the end of the sequence.
			///
			/// Only valid to call after resuming from an awaited advance operation.
			/// i.e. Either a begin() or iterator::operator++() operation.
			bool finished() const noexcept
			{
				return m_currentValue == nullptr;
			}

			void rethrow_if_unhandled_exception()
			{
				if (m_exception)
				{
					std::rethrow_exception(std::move(m_exception));
				}
			}

		protected:

			async_generator_yield_operation internal_yield_value() noexcept;

		private:

			friend class async_generator_yield_operation;
			friend class async_generator_advance_operation;

			std::exception_ptr m_exception;

			cppcoro::coroutine_handle<> m_consumerCoroutine;

		protected:

			void* m_currentValue;
		};

		class async_generator_yield_operation final
		{
		public:

			async_generator_yield_operation(cppcoro::coroutine_handle<> consumer) noexcept
				: m_consumer(consumer)
			{}

			bool await_ready() const noexcept
			{
				return false;
			}

			cppcoro::coroutine_handle<>
			await_suspend([[maybe_unused]] cppcoro::coroutine_handle<> producer) noexcept
			{
				return m_consumer;
			}

			void await_resume() noexcept {}

		private:

			cppcoro::coroutine_handle<> m_consumer;

		};

		inline async_generator_yield_operation async_generator_promise_base::final_suspend() noexcept
		{
			m_currentValue = nullptr;
			return internal_yield_value();
		}

		inline async_generator_yield_operation async_generator_promise_base::internal_yield_value() noexcept
		{
			return async_generator_yield_operation{ m_consumerCoroutine };
		}

		class async_generator_advance_operation
		{
		protected:

			async_generator_advance_operation(std::nullptr_t) noexcept
				: m_promise(nullptr)
				, m_producerCoroutine(nullptr)
			{}

			async_generator_advance_operation(
				async_generator_promise_base& promise,
				cppcoro::coroutine_handle<> producerCoroutine) noexcept
				: m_promise(std::addressof(promise))
				, m_producerCoroutine(producerCoroutine)
			{
			}

		public:

			bool await_ready() const noexcept { return false; }

			cppcoro::coroutine_handle<>
				await_suspend(cppcoro::coroutine_handle<> consumerCoroutine) noexcept
			{
				m_promise->m_consumerCoroutine = consumerCoroutine;
				return m_producerCoroutine;
			}

		protected:

			async_generator_promise_base* m_promise;
			cppcoro::coroutine_handle<> m_producerCoroutine;

		};

		template<typename T>
		class async_generator_promise final : public async_generator_promise_base
		{
			using value_type = std::remove_reference_t<T>;

		public:

			async_generator_promise() noexcept = default;

			async_generator<T> get_return_object() noexcept;

			async_generator_yield_operation yield_value(value_type& value) noexcept
			{
				m_currentValue = std::addressof(value);
				return internal_yield_value();
			}

			async_generator_yield_operation yield_value(value_type&& value) noexcept
			{
				return yield_value(value);
			}

			T& value() const noexcept
			{
				return *static_cast<T*>(m_currentValue);
			}

		};

		template<typename T>
		class async_generator_promise<T&&> final : public async_generator_promise_base
		{
		public:

			async_generator_promise() noexcept = default;

			async_generator<T> get_return_object() noexcept;

			async_generator_yield_operation yield_value(T&& value) noexcept
			{
				m_currentValue = std::addressof(value);
				return internal_yield_value();
			}

			T&& value() const noexcept
			{
				return std::move(*static_cast<T*>(m_currentValue));
			}

		};

		template<typename T>
		class async_generator_increment_operation final : public async_generator_advance_operation
		{
		public:

			async_generator_increment_operation(async_generator_iterator<T>& iterator) noexcept
				: async_generator_advance_operation(iterator.m_coroutine.promise(), iterator.m_coroutine)
				, m_iterator(iterator)
			{}

			async_generator_iterator<T>& await_resume();

		private:

			async_generator_iterator<T>& m_iterator;

		};

		template<typename T>
		class async_generator_iterator final
		{
			using promise_type = async_generator_promise<T>;
			using handle_type = cppcoro::coroutine_handle<promise_type>;

		public:

			using iterator_category = std::input_iterator_tag;
			// Not sure what type should be used for difference_type as we don't
			// allow calculating difference between two iterators.
			using difference_type = std::ptrdiff_t;
			using value_type = std::remove_reference_t<T>;
			using reference = std::add_lvalue_reference_t<T>;
			using pointer = std::add_pointer_t<value_type>;

			async_generator_iterator(std::nullptr_t) noexcept
				: m_coroutine(nullptr)
			{}

			async_generator_iterator(handle_type coroutine) noexcept
				: m_coroutine(coroutine)
			{}

			async_generator_increment_operation<T> operator++() noexcept
			{
				return async_generator_increment_operation<T>{ *this };
			}

			reference operator*() const noexcept
			{
				return m_coroutine.promise().value();
			}

			bool operator==(const async_generator_iterator& other) const noexcept
			{
				return m_coroutine == other.m_coroutine;
			}

			bool operator!=(const async_generator_iterator& other) const noexcept
			{
				return !(*this == other);
			}

		private:

			friend class async_generator_increment_operation<T>;

			handle_type m_coroutine;

		};

		template<typename T>
		async_generator_iterator<T>& async_generator_increment_operation<T>::await_resume()
		{
			if (m_promise->finished())
			{
				// Update iterator to end()
				m_iterator = async_generator_iterator<T>{ nullptr };
				m_promise->rethrow_if_unhandled_exception();
			}

			return m_iterator;
		}

		template<typename T>
		class async_generator_begin_operation final : public async_generator_advance_operation
		{
			using promise_type = async_generator_promise<T>;
			using handle_type = cppcoro::coroutine_handle<promise_type>;

		public:

			async_generator_begin_operation(std::nullptr_t) noexcept
				: async_generator_advance_operation(nullptr)
			{}

			async_generator_begin_operation(handle_type producerCoroutine) noexcept
				: async_generator_advance_operation(producerCoroutine.promise(), producerCoroutine)
			{}

			bool await_ready() const noexcept
			{
				return m_promise == nullptr || async_generator_advance_operation::await_ready();
			}

			async_generator_iterator<T> await_resume()
			{
				if (m_promise == nullptr)
				{
					// Called begin() on the empty generator.
					return async_generator_iterator<T>{ nullptr };
				}
				else if (m_promise->finished())
				{
					// Completed without yielding any values.
					m_promise->rethrow_if_unhandled_exception();
					return async_generator_iterator<T>{ nullptr };
				}

				return async_generator_iterator<T>{
					handle_type::from_promise(*static_cast<promise_type*>(m_promise))
				};
			}
		};
	}

	template<typename T>
	class [[nodiscard]] async_generator
	{
	public:

		using promise_type = detail::async_generator_promise<T>;
		using iterator = detail::async_generator_iterator<T>;

		async_generator() noexcept
			: m_coroutine(nullptr)
		{}

		explicit async_generator(promise_type& promise) noexcept
			: m_coroutine(cppcoro::coroutine_handle<promise_type>::from_promise(promise))
		{}

		async_generator(async_generator&& other) noexcept
			: m_coroutine(other.m_coroutine)
		{
			other.m_coroutine = nullptr;
		}

		~async_generator()
		{
			if (m_coroutine)
			{
				m_coroutine.destroy();
			}
		}

		async_generator& operator=(async_generator&& other) noexcept
		{
			async_generator temp(std::move(other));
			swap(temp);
			return *this;
		}

		async_generator(const async_generator&) = delete;
		async_generator& operator=(const async_generator&) = delete;

		auto begin() noexcept
		{
			if (!m_coroutine)
			{
				return detail::async_generator_begin_operation<T>{ nullptr };
			}

			return detail::async_generator_begin_operation<T>{ m_coroutine };
		}

		auto end() noexcept
		{
			return iterator{ nullptr };
		}

		void swap(async_generator& other) noexcept
		{
			using std::swap;
			swap(m_coroutine, other.m_coroutine);
		}

	private:

		cppcoro::coroutine_handle<promise_type> m_coroutine;

	};

	template<typename T>
	void swap(async_generator<T>& a, async_generator<T>& b) noexcept
	{
		a.swap(b);
	}

	namespace detail
	{
		template<typename T>
		async_generator<T> async_generator_promise<T>::get_return_object() noexcept
		{
			return async_generator<T>{ *this };
		}
	}
#else // !CPPCORO_COMPILER_SUPPORTS_SYMMETRIC_TRANSFER

	namespace detail
	{
		template<typename T>
		class async_generator_iterator;
		class async_generator_yield_operation;
		class async_generator_advance_operation;

		class async_generator_promise_base
		{
		public:

			async_generator_promise_base() noexcept
				: m_state(state::value_ready_producer_suspended)
				, m_exception(nullptr)
			{
				// Other variables left intentionally uninitialised as they're
				// only referenced in certain states by which time they should
				// have been initialised.
			}

			async_generator_promise_base(const async_generator_promise_base& other) = delete;
			async_generator_promise_base& operator=(const async_generator_promise_base& other) = delete;

			cppcoro::suspend_always initial_suspend() const noexcept
			{
				return {};
			}

			async_generator_yield_operation final_suspend() noexcept;

			void unhandled_exception() noexcept
			{
				// Don't bother capturing the exception if we have been cancelled
				// as there is no consumer that will see it.
				if (m_state.load(std::memory_order_relaxed) != state::cancelled)
				{
					m_exception = std::current_exception();
				}
			}

			void return_void() noexcept
			{
			}

			/// Query if the generator has reached the end of the sequence.
			///
			/// Only valid to call after resuming from an awaited advance operation.
			/// i.e. Either a begin() or iterator::operator++() operation.
			bool finished() const noexcept
			{
				return m_currentValue == nullptr;
			}

			void rethrow_if_unhandled_exception()
			{
				if (m_exception)
				{
					std::rethrow_exception(std::move(m_exception));
				}
			}

			/// Request that the generator cancel generation of new items.
			///
			/// \return
			/// Returns true if the request was completed synchronously and the associated
			/// producer coroutine is now available to be destroyed. In which case the caller
			/// is expected to call destroy() on the coroutine_handle.
			/// Returns false if the producer coroutine was not at a suitable suspend-point.
			/// The coroutine will be destroyed when it next reaches a co_yield or co_return
			/// statement.
			bool request_cancellation() noexcept
			{
				const auto previousState = m_state.exchange(state::cancelled, std::memory_order_acq_rel);

				// Not valid to destroy async_generator<T> object if consumer coroutine still suspended
				// in a co_await for next item.
				assert(previousState != state::value_not_ready_consumer_suspended);

				// A coroutine should only ever be cancelled once, from the destructor of the
				// owning async_generator<T> object.
				assert(previousState != state::cancelled);

				return previousState == state::value_ready_producer_suspended;
			}

		protected:

			async_generator_yield_operation internal_yield_value() noexcept;

		private:

			friend class async_generator_yield_operation;
			friend class async_generator_advance_operation;

			// State transition diagram
			//   VNRCA - value_not_ready_consumer_active
			//   VNRCS - value_not_ready_consumer_suspended
			//   VRPA  - value_ready_producer_active
			//   VRPS  - value_ready_producer_suspended
			//
			//       A         +---  VNRCA --[C]--> VNRCS   yield_value()
			//       |         |     |  A           |  A       |   .
			//       |        [C]   [P] |          [P] |       |   .
			//       |         |     | [C]          | [C]      |   .
			//       |         |     V  |           V  |       |   .
			//  operator++/    |     VRPS <--[P]--- VRPA       V   |
			//  begin()        |      |              |             |
			//                 |     [C]            [C]            |
			//                 |      +----+     +---+             |
			//                 |           |     |                 |
			//                 |           V     V                 V
			//                 +--------> cancelled         ~async_generator()
			//
			// [C] - Consumer performs this transition
			// [P] - Producer performs this transition
			enum class state
			{
				value_not_ready_consumer_active,
				value_not_ready_consumer_suspended,
				value_ready_producer_active,
				value_ready_producer_suspended,
				cancelled
			};

			std::atomic<state> m_state;

			std::exception_ptr m_exception;

			cppcoro::coroutine_handle<> m_consumerCoroutine;

		protected:

			void* m_currentValue;
		};

		class async_generator_yield_operation final
		{
			using state = async_generator_promise_base::state;

		public:

			async_generator_yield_operation(async_generator_promise_base& promise, state initialState) noexcept
				: m_promise(promise)
				, m_initialState(initialState)
			{}

			bool await_ready() const noexcept
			{
				return m_initialState == state::value_not_ready_consumer_suspended;
			}

			bool await_suspend(cppcoro::coroutine_handle<> producer) noexcept;

			void await_resume() noexcept {}

		private:
			async_generator_promise_base& m_promise;
			state m_initialState;
		};

		inline async_generator_yield_operation async_generator_promise_base::final_suspend() noexcept
		{
			m_currentValue = nullptr;
			return internal_yield_value();
		}

		inline async_generator_yield_operation async_generator_promise_base::internal_yield_value() noexcept
		{
			state currentState = m_state.load(std::memory_order_acquire);
			assert(currentState != state::value_ready_producer_active);
			assert(currentState != state::value_ready_producer_suspended);

			if (currentState == state::value_not_ready_consumer_suspended)
			{
				// Only need relaxed memory order since we're resuming the
				// consumer on the same thread.
				m_state.store(state::value_ready_producer_active, std::memory_order_relaxed);

				// Resume the consumer.
				// It might ask for another value before returning, in which case it'll
				// transition to value_not_ready_consumer_suspended and we can return from
				// yield_value without suspending, otherwise we should try to suspend
				// the producer in which case the consumer will wake us up again
				// when it wants the next value.
				m_consumerCoroutine.resume();

				// Need to use acquire semantics here since it's possible that the
				// consumer might have asked for the next value on a different thread
				// which executed concurrently with the call to m_consumerCoro on the
				// current thread above.
				currentState = m_state.load(std::memory_order_acquire);
			}

			return async_generator_yield_operation{ *this, currentState };
		}

		inline bool async_generator_yield_operation::await_suspend(
			cppcoro::coroutine_handle<> producer) noexcept
		{
			state currentState = m_initialState;
			if (currentState == state::value_not_ready_consumer_active)
			{
				bool producerSuspended = m_promise.m_state.compare_exchange_strong(
					currentState,
					state::value_ready_producer_suspended,
					std::memory_order_release,
					std::memory_order_acquire);
				if (producerSuspended)
				{
					return true;
				}

				if (currentState == state::value_not_ready_consumer_suspended)
				{
					// Can get away with using relaxed memory semantics here since we're
					// resuming the consumer on the current thread.
					m_promise.m_state.store(state::value_ready_producer_active, std::memory_order_relaxed);

					m_promise.m_consumerCoroutine.resume();

					// The consumer might have asked for another value before returning, in which case
					// it'll transition to value_not_ready_consumer_suspended and we can return without
					// suspending, otherwise we should try to suspend the producer, in which case the
					// consumer will wake us up again when it wants the next value.
					//
					// Need to use acquire semantics here since it's possible that the consumer might
					// have asked for the next value on a different thread which executed concurrently
					// with the call to m_consumerCoro.resume() above.
					currentState = m_promise.m_state.load(std::memory_order_acquire);
					if (currentState == state::value_not_ready_consumer_suspended)
					{
						return false;
					}
				}
			}

			// By this point the consumer has been resumed if required and is now active.

			if (currentState == state::value_ready_producer_active)
			{
				// Try to suspend the producer.
				// If we failed to suspend then it's either because the consumer destructed, transitioning
				// the state to cancelled, or requested the next item, transitioning the state to value_not_ready_consumer_suspended.
				const bool suspendedProducer = m_promise.m_state.compare_exchange_strong(
					currentState,
					state::value_ready_producer_suspended,
					std::memory_order_release,
					std::memory_order_acquire);
				if (suspendedProducer)
				{
					return true;
				}

				if (currentState == state::value_not_ready_consumer_suspended)
				{
					// Consumer has asked for the next value.
					return false;
				}
			}

			assert(currentState == state::cancelled);

			// async_generator object has been destroyed and we're now at a
			// co_yield/co_return suspension point so we can just destroy
			// the coroutine.
			producer.destroy();

			return true;
		}

		class async_generator_advance_operation
		{
			using state = async_generator_promise_base::state;

		protected:

			async_generator_advance_operation(std::nullptr_t) noexcept
				: m_promise(nullptr)
				, m_producerCoroutine(nullptr)
			{}

			async_generator_advance_operation(
				async_generator_promise_base& promise,
				cppcoro::coroutine_handle<> producerCoroutine) noexcept
				: m_promise(std::addressof(promise))
				, m_producerCoroutine(producerCoroutine)
			{
				state initialState = promise.m_state.load(std::memory_order_acquire);
				if (initialState == state::value_ready_producer_suspended)
				{
					// Can use relaxed memory order here as we will be resuming the producer
					// on the same thread.
					promise.m_state.store(state::value_not_ready_consumer_active, std::memory_order_relaxed);

					producerCoroutine.resume();

					// Need to use acquire memory order here since it's possible that the
					// coroutine may have transferred execution to another thread and
					// completed on that other thread before the call to resume() returns.
					initialState = promise.m_state.load(std::memory_order_acquire);
				}

				m_initialState = initialState;
			}

		public:

			bool await_ready() const noexcept
			{
				return m_initialState == state::value_ready_producer_suspended;
			}

			bool await_suspend(cppcoro::coroutine_handle<> consumerCoroutine) noexcept
			{
				m_promise->m_consumerCoroutine = consumerCoroutine;

				auto currentState = m_initialState;
				if (currentState == state::value_ready_producer_active)
				{
					// A potential race between whether consumer or producer coroutine
					// suspends first. Resolve the race using a compare-exchange.
					if (m_promise->m_state.compare_exchange_strong(
						currentState,
						state::value_not_ready_consumer_suspended,
						std::memory_order_release,
						std::memory_order_acquire))
					{
						return true;
					}

					assert(currentState == state::value_ready_producer_suspended);

					m_promise->m_state.store(state::value_not_ready_consumer_active, std::memory_order_relaxed);

					m_producerCoroutine.resume();

					currentState = m_promise->m_state.load(std::memory_order_acquire);
					if (currentState == state::value_ready_producer_suspended)
					{
						// Producer coroutine produced a value synchronously.
						return false;
					}
				}

				assert(currentState == state::value_not_ready_consumer_active);

				// Try to suspend consumer coroutine, transitioning to value_not_ready_consumer_suspended.
				// This could be racing with producer making the next value available and suspending
				// (transition to value_ready_producer_suspended) so we use compare_exchange to decide who
				// wins the race.
				// If compare_exchange succeeds then consumer suspended (and we return true).
				// If it fails then producer yielded next value and suspended and we can return
				// synchronously without suspended (ie. return false).
				return m_promise->m_state.compare_exchange_strong(
					currentState,
					state::value_not_ready_consumer_suspended,
					std::memory_order_release,
					std::memory_order_acquire);
			}

		protected:

			async_generator_promise_base* m_promise;
			cppcoro::coroutine_handle<> m_producerCoroutine;

		private:

			state m_initialState;

		};

		template<typename T>
		class async_generator_promise final : public async_generator_promise_base
		{
			using value_type = std::remove_reference_t<T>;

		public:

			async_generator_promise() noexcept = default;

			async_generator<T> get_return_object() noexcept;

			async_generator_yield_operation yield_value(value_type& value) noexcept
			{
				m_currentValue = std::addressof(value);
				return internal_yield_value();
			}

			async_generator_yield_operation yield_value(value_type&& value) noexcept
			{
				return yield_value(value);
			}

			T& value() const noexcept
			{
				return *static_cast<T*>(m_currentValue);
			}

		};

		template<typename T>
		class async_generator_promise<T&&> final : public async_generator_promise_base
		{
		public:

			async_generator_promise() noexcept = default;

			async_generator<T> get_return_object() noexcept;

			async_generator_yield_operation yield_value(T&& value) noexcept
			{
				m_currentValue = std::addressof(value);
				return internal_yield_value();
			}

			T&& value() const noexcept
			{
				return std::move(*static_cast<T*>(m_currentValue));
			}

		};

		template<typename T>
		class async_generator_increment_operation final : public async_generator_advance_operation
		{
		public:

			async_generator_increment_operation(async_generator_iterator<T>& iterator) noexcept
				: async_generator_advance_operation(iterator.m_coroutine.promise(), iterator.m_coroutine)
				, m_iterator(iterator)
			{}

			async_generator_iterator<T>& await_resume();

		private:

			async_generator_iterator<T>& m_iterator;

		};

		template<typename T>
		class async_generator_iterator final
		{
			using promise_type = async_generator_promise<T>;
			using handle_type = cppcoro::coroutine_handle<promise_type>;

		public:

			using iterator_category = std::input_iterator_tag;
			// Not sure what type should be used for difference_type as we don't
			// allow calculating difference between two iterators.
			using difference_type = std::ptrdiff_t;
			using value_type = std::remove_reference_t<T>;
			using reference = std::add_lvalue_reference_t<T>;
			using pointer = std::add_pointer_t<value_type>;

			async_generator_iterator(std::nullptr_t) noexcept
				: m_coroutine(nullptr)
			{}

			async_generator_iterator(handle_type coroutine) noexcept
				: m_coroutine(coroutine)
			{}

			async_generator_increment_operation<T> operator++() noexcept
			{
				return async_generator_increment_operation<T>{ *this };
			}

			reference operator*() const noexcept
			{
				return m_coroutine.promise().value();
			}

			bool operator==(const async_generator_iterator& other) const noexcept
			{
				return m_coroutine == other.m_coroutine;
			}

			bool operator!=(const async_generator_iterator& other) const noexcept
			{
				return !(*this == other);
			}

		private:

			friend class async_generator_increment_operation<T>;

			handle_type m_coroutine;

		};

		template<typename T>
		async_generator_iterator<T>& async_generator_increment_operation<T>::await_resume()
		{
			if (m_promise->finished())
			{
				// Update iterator to end()
				m_iterator = async_generator_iterator<T>{ nullptr };
				m_promise->rethrow_if_unhandled_exception();
			}

			return m_iterator;
		}

		template<typename T>
		class async_generator_begin_operation final : public async_generator_advance_operation
		{
			using promise_type = async_generator_promise<T>;
			using handle_type = cppcoro::coroutine_handle<promise_type>;

		public:

			async_generator_begin_operation(std::nullptr_t) noexcept
				: async_generator_advance_operation(nullptr)
			{}

			async_generator_begin_operation(handle_type producerCoroutine) noexcept
				: async_generator_advance_operation(producerCoroutine.promise(), producerCoroutine)
			{}

			bool await_ready() const noexcept
			{
				return m_promise == nullptr || async_generator_advance_operation::await_ready();
			}

			async_generator_iterator<T> await_resume()
			{
				if (m_promise == nullptr)
				{
					// Called begin() on the empty generator.
					return async_generator_iterator<T>{ nullptr };
				}
				else if (m_promise->finished())
				{
					// Completed without yielding any values.
					m_promise->rethrow_if_unhandled_exception();
					return async_generator_iterator<T>{ nullptr };
				}

				return async_generator_iterator<T>{
					handle_type::from_promise(*static_cast<promise_type*>(m_promise))
				};
			}
		};
	}

	template<typename T>
	class async_generator
	{
	public:

		using promise_type = detail::async_generator_promise<T>;
		using iterator = detail::async_generator_iterator<T>;

		async_generator() noexcept
			: m_coroutine(nullptr)
		{}

		explicit async_generator(promise_type& promise) noexcept
			: m_coroutine(cppcoro::coroutine_handle<promise_type>::from_promise(promise))
		{}

		async_generator(async_generator&& other) noexcept
			: m_coroutine(other.m_coroutine)
		{
			other.m_coroutine = nullptr;
		}

		~async_generator()
		{
			if (m_coroutine)
			{
				if (m_coroutine.promise().request_cancellation())
				{
					m_coroutine.destroy();
				}
			}
		}

		async_generator& operator=(async_generator&& other) noexcept
		{
			async_generator temp(std::move(other));
			swap(temp);
			return *this;
		}

		async_generator(const async_generator&) = delete;
		async_generator& operator=(const async_generator&) = delete;

		auto begin() noexcept
		{
			if (!m_coroutine)
			{
				return detail::async_generator_begin_operation<T>{ nullptr };
			}

			return detail::async_generator_begin_operation<T>{ m_coroutine };
		}

		auto end() noexcept
		{
			return iterator{ nullptr };
		}

		void swap(async_generator& other) noexcept
		{
			using std::swap;
			swap(m_coroutine, other.m_coroutine);
		}

	private:

		cppcoro::coroutine_handle<promise_type> m_coroutine;

	};

	template<typename T>
	void swap(async_generator<T>& a, async_generator<T>& b) noexcept
	{
		a.swap(b);
	}

	namespace detail
	{
		template<typename T>
		async_generator<T> async_generator_promise<T>::get_return_object() noexcept
		{
			return async_generator<T>{ *this };
		}
	}
#endif // !CPPCORO_COMPILER_SUPPORTS_SYMMETRIC_TRANSFER

	template<typename FUNC, typename T>
	async_generator<std::invoke_result_t<FUNC&, decltype(*std::declval<typename async_generator<T>::iterator&>())>> fmap(
		FUNC func,
		async_generator<T> source)
	{
		static_assert(
			!std::is_reference_v<FUNC>,
			"Passing by reference to async_generator<T> coroutine is unsafe. "
			"Use std::ref or std::cref to explicitly pass by reference.");

		// Explicitly hand-coding the loop here rather than using range-based
		// for loop since it's difficult to std::forward<???> the value of a
		// range-based for-loop, preserving the value category of operator*
		// return-value.
		auto it = co_await source.begin();
		const auto itEnd = source.end();
		while (it != itEnd)
		{
			co_yield std::invoke(func, *it);
			(void)co_await ++it;
		}
	}
}

#endif
