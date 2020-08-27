///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_LINUX_ASYNC_OPERATION_HPP_INCLUDED
#define CPPCORO_DETAIL_LINUX_ASYNC_OPERATION_HPP_INCLUDED

#include <cppcoro/cancellation_registration.hpp>
#include <cppcoro/cancellation_token.hpp>
#include <cppcoro/operation_cancelled.hpp>

#include <cppcoro/detail/io_uring_context.hpp>

#include <optional>
#include <system_error>
#include <experimental/coroutine>
#include <cassert>

namespace cppcoro
{
	namespace detail
	{
		class linux_async_operation_base
			: protected detail::linux::io_state
		{
		public:

			linux_async_operation_base(
				detail::linux::io_state::callback_type* callback,
				detail::linux::io_uring_context* ctx) noexcept
				: detail::linux::io_state(callback)
				, m_aioContext(ctx)
			{}

			std::size_t get_result()
			{
				if (m_res < 0)
				{
					throw std::system_error{
						-m_res,
						std::system_category()
					};
				}

				return m_res;
			}

			std::int32_t m_res;

			detail::linux::io_uring_context* m_aioContext;

		};

		template<typename OPERATION>
		class linux_async_operation
			: protected linux_async_operation_base
		{
		protected:

			linux_async_operation(
				detail::linux::io_uring_context* ctx) noexcept
				: linux_async_operation_base(
					&linux_async_operation::on_operation_completed,
					ctx)
			{}

		public:

			bool await_ready() const noexcept { return false; }

			CPPCORO_NOINLINE
			bool await_suspend(std::experimental::coroutine_handle<> awaitingCoroutine)
			{
				static_assert(std::is_base_of_v<linux_async_operation, OPERATION>);

				m_awaitingCoroutine = awaitingCoroutine;
				return static_cast<OPERATION*>(this)->try_start();
			}

			decltype(auto) await_resume()
			{
				return static_cast<OPERATION*>(this)->get_result();
			}

		private:

			static void on_operation_completed(
				detail::linux::io_state* ioState,
				std::int32_t res) noexcept
			{
				auto* operation = static_cast<linux_async_operation*>(ioState);
				operation->m_res = res;
				operation->m_awaitingCoroutine.resume();
			}

			std::experimental::coroutine_handle<> m_awaitingCoroutine;

		};

		template<typename OPERATION>
		class linux_async_operation_cancellable
			: protected linux_async_operation_base
		{
			static constexpr int error_operation_cancelled = ECANCELED;

		protected:

			linux_async_operation_cancellable(
					detail::linux::io_uring_context* ctx,
					cancellation_token&& ct) noexcept
				: linux_async_operation_base(
					  &linux_async_operation_cancellable::on_operation_completed,
					  ctx)
				, m_state(ct.is_cancellation_requested() ? state::completed : state::not_started)
				, m_cancellationToken(std::move(ct))
			{
				m_res = -error_operation_cancelled;
			}

			linux_async_operation_cancellable(
				linux_async_operation_cancellable&& other) noexcept
				: linux_async_operation_base(std::move(other))
				, m_state(other.m_state.load(std::memory_order_relaxed))
				, m_cancellationToken(std::move(other.m_cancellationToken))
			{
				assert(m_res == other.m_res);
			}

		public:

			bool await_ready() const noexcept
			{
				return m_state.load(std::memory_order_relaxed) == state::completed;
			}

			CPPCORO_NOINLINE
			bool await_suspend(std::experimental::coroutine_handle<> awaitingCoroutine)
			{
				static_assert(std::is_base_of_v<linux_async_operation_cancellable, OPERATION>);

				m_awaitingCoroutine = awaitingCoroutine;

				// TRICKY: Register cancellation callback before starting the operation
				// in case the callback registration throws due to insufficient
				// memory. We need to make sure that the logic that occurs after
				// starting the operation is noexcept, otherwise we run into the
				// problem of not being able to cancel the started operation and
				// the dilemma of what to do with the exception.
				//
				// However, doing this means that the cancellation callback may run
				// prior to returning below so in the case that cancellation may
				// occur we defer setting the state to 'started' until after
				// the operation has finished starting. The cancellation callback
				// will only attempt to request cancellation of the operation with
				// CancelIoEx() once the state has been set to 'started'.
				const bool canBeCancelled = m_cancellationToken.can_be_cancelled();
				if (canBeCancelled)
				{
					m_cancellationCallback.emplace(
						std::move(m_cancellationToken),
						[this] { this->on_cancellation_requested(); });
				}
				else
				{
					m_state.store(state::started, std::memory_order_relaxed);
				}

				// Now start the operation.
				const bool willCompleteAsynchronously = static_cast<OPERATION*>(this)->try_start();
				if (!willCompleteAsynchronously)
				{
					// Operation completed synchronously, resume awaiting coroutine immediately.
					return false;
				}

				if (canBeCancelled)
				{
					// Need to flag that the operation has finished starting now.

					// However, the operation may have completed concurrently on
					// another thread, transitioning directly from not_started -> complete.
					// Or it may have had the cancellation callback execute and transition
					// from not_started -> cancellation_requested. We use a compare-exchange
					// to determine a winner between these potential racing cases.
					state oldState = state::not_started;
					if (!m_state.compare_exchange_strong(
						oldState,
						state::started,
						std::memory_order_release,
						std::memory_order_acquire))
					{
						if (oldState == state::cancellation_requested)
						{
							// Request the operation be cancelled.
							// Note that it may have already completed on a background
							// thread by now so this request for cancellation may end up
							// being ignored.
							static_cast<OPERATION*>(this)->cancel();

							if (!m_state.compare_exchange_strong(
								oldState,
								state::started,
								std::memory_order_release,
								std::memory_order_acquire))
							{
								assert(oldState == state::completed);
								return false;
							}
						}
						else
						{
							assert(oldState == state::completed);
							return false;
						}
					}
				}

				return true;
			}

			decltype(auto) await_resume()
			{
				// Free memory used by the cancellation callback now that the operation
				// has completed rather than waiting until the operation object destructs.
				// eg. If the operation is passed to when_all() then the operation object
				// may not be destructed until all of the operations complete.
				m_cancellationCallback.reset();

				if (m_res == -error_operation_cancelled)
				{
					throw operation_cancelled{};
				}

				return static_cast<OPERATION*>(this)->get_result();
			}

		private:

			enum class state
			{
				not_started,
				started,
				cancellation_requested,
				completed
			};

			void on_cancellation_requested() noexcept
			{
				auto oldState = m_state.load(std::memory_order_acquire);
				if (oldState == state::not_started)
				{
					// This callback is running concurrently with await_suspend().
					// The call to start the operation may not have returned yet so
					// we can't safely request cancellation of it. Instead we try to
					// notify the await_suspend() thread by transitioning the state
					// to state::cancellation_requested so that the await_suspend()
					// thread can request cancellation after it has finished starting
					// the operation.
					const bool transferredCancelResponsibility =
						m_state.compare_exchange_strong(
							oldState,
							state::cancellation_requested,
							std::memory_order_release,
							std::memory_order_acquire);
					if (transferredCancelResponsibility)
					{
						return;
					}
				}

				// No point requesting cancellation if the operation has already completed.
				if (oldState != state::completed)
				{
					static_cast<OPERATION*>(this)->cancel();
				}
			}

			static void on_operation_completed(
				detail::linux::io_state* ioState,
				std::int32_t res) noexcept
			{
				auto* operation = static_cast<linux_async_operation_cancellable*>(ioState);

				operation->m_res = res;

				auto state = operation->m_state.load(std::memory_order_acquire);
				if (state == state::started)
				{
					operation->m_state.store(state::completed, std::memory_order_relaxed);
					operation->m_awaitingCoroutine.resume();
				}
				else
				{
					// We are racing with await_suspend() call suspending.
					// Try to mark it as completed using an atomic exchange and look
					// at the previous value to determine whether the coroutine suspended
					// first (in which case we resume it now) or we marked it as completed
					// first (in which case await_suspend() will return false and immediately
					// resume the coroutine).
					state = operation->m_state.exchange(
						state::completed,
						std::memory_order_acq_rel);
					if (state == state::started)
					{
						// The await_suspend() method returned (or will return) 'true' and so
						// we need to resume the coroutine.
						operation->m_awaitingCoroutine.resume();
					}
				}
			}

			std::atomic<state> m_state;
			cppcoro::cancellation_token m_cancellationToken;
			std::optional<cppcoro::cancellation_registration> m_cancellationCallback;
			std::experimental::coroutine_handle<> m_awaitingCoroutine;

		};
	}
}

#endif
