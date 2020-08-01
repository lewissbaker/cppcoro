///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_LINUX_URING_OPERATION_HPP_INCLUDED
#define CPPCORO_DETAIL_LINUX_URING_OPERATION_HPP_INCLUDED

#include <cppcoro/cancellation_registration.hpp>
#include <cppcoro/cancellation_token.hpp>
#include <cppcoro/operation_cancelled.hpp>

#include <cppcoro/detail/linux.hpp>
#include <cppcoro/io_service.hpp>

#include <optional>
#include <system_error>
#include <cppcoro/stdcoro.hpp>
#include <cassert>

namespace cppcoro
{
    namespace detail
    {
        class uring_operation_base
        {
        public:

            uring_operation_base(size_t offset) noexcept
                : m_errorCode(0)
                , m_numberOfBytesTransferred(0)
			    , m_offset(offset)
            {}

            std::size_t get_result()
            {
                if (m_errorCode != 0)
                {
                    throw std::system_error{
                        static_cast<int>(m_errorCode),
                        std::system_category()
                    };
                }

                return m_numberOfBytesTransferred;
            }

            int m_errorCode;
            size_t m_numberOfBytesTransferred;
			size_t m_offset;
            stdcoro::coroutine_handle<> m_awaitingCoroutine;
        };

        template<typename OPERATION>
        class uring_operation
            : protected uring_operation_base
        {
        protected:

            uring_operation(size_t offset) noexcept
				: uring_operation_base(offset)
            {}

        public:

            bool await_ready() const noexcept { return false; }

            CPPCORO_NOINLINE
            bool await_suspend(stdcoro::coroutine_handle<> awaitingCoroutine)
            {
                static_assert(std::is_base_of_v<uring_operation, OPERATION>);

                m_awaitingCoroutine = awaitingCoroutine;
                return static_cast<OPERATION*>(this)->try_start();
            }

            decltype(auto) await_resume()
            {
                return static_cast<OPERATION*>(this)->get_result();
            }
        };

        template<typename OPERATION>
        class uring_operation_cancellable
            : protected uring_operation_base
        {
            // ERROR_OPERATION_ABORTED value from <errno.h>
            static constexpr int error_operation_aborted = ECANCELED;

        protected:

            uring_operation_cancellable(cancellation_token&& ct) noexcept
                : uring_operation_base(0)
				, m_cancellationToken(std::move(ct))
            {
                m_errorCode = error_operation_aborted;
            }

            uring_operation_cancellable(size_t offset, cancellation_token&& ct) noexcept
                : uring_operation_base(offset)
                , m_cancellationToken(std::move(ct))
            {
                m_errorCode = error_operation_aborted;
            }

        public:

            bool await_ready() const noexcept
            {
                return m_state.load(std::memory_order_relaxed) == state::completed;
            }

            CPPCORO_NOINLINE
            bool await_suspend(stdcoro::coroutine_handle<> awaitingCoroutine)
            {
                static_assert(std::is_base_of_v<uring_operation_cancellable, OPERATION>);

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
                m_state.store(state::started, std::memory_order_relaxed);

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
                if (m_errorCode == error_operation_aborted)
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

            std::atomic<state> m_state;
            cppcoro::cancellation_token m_cancellationToken;
            stdcoro::coroutine_handle<> m_awaitingCoroutine;

        };
    }
}

#endif // CPPCORO_DETAIL_LINUX_URING_OPERATION_HPP_INCLUDED
