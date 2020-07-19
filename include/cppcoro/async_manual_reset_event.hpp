///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_ASYNC_MANUAL_RESET_EVENT_HPP_INCLUDED
#define CPPCORO_ASYNC_MANUAL_RESET_EVENT_HPP_INCLUDED

#include <cppcoro/coroutine.hpp>
#include <atomic>
#include <cstdint>

namespace cppcoro
{
	class async_manual_reset_event_operation;

	/// An async manual-reset event is a coroutine synchronisation abstraction
	/// that allows one or more coroutines to wait until some thread calls
	/// set() on the event.
	///
	/// When a coroutine awaits a 'set' event the coroutine continues without
	/// suspending. Otherwise, if it awaits a 'not set' event the coroutine is
	/// suspended and is later resumed inside the call to 'set()'.
	///
	/// \seealso async_auto_reset_event
	class async_manual_reset_event
	{
	public:

		/// Initialise the event to either 'set' or 'not set' state.
		///
		/// \param initiallySet
		/// If 'true' then initialises the event to the 'set' state, otherwise
		/// initialises the event to the 'not set' state.
		async_manual_reset_event(bool initiallySet = false) noexcept;

		~async_manual_reset_event();

		/// Wait for the event to enter the 'set' state.
		///
		/// If the event is already 'set' then the coroutine continues without
		/// suspending.
		///
		/// Otherwise, the coroutine is suspended and later resumed when some
		/// thread calls 'set()'. The coroutine will be resumed inside the next
		/// call to 'set()'.
		async_manual_reset_event_operation operator co_await() const noexcept;

		/// Query if the event is currently in the 'set' state.
		bool is_set() const noexcept;

		/// Set the state of the event to 'set'.
		///
		/// If there are pending coroutines awaiting the event then all
		/// pending coroutines are resumed within this call.
		/// Any coroutines that subsequently await the event will continue
		/// without suspending.
		///
		/// This operation is a no-op if the event was already 'set'.
		void set() noexcept;

		/// Set the state of the event to 'not-set'.
		///
		/// Any coroutines that subsequently await the event will suspend
		/// until some thread calls 'set()'.
		///
		/// This is a no-op if the state was already 'not set'.
		void reset() noexcept;

	private:

		friend class async_manual_reset_event_operation;

		// This variable has 3 states:
		// - this    - The state is 'set'.
		// - nullptr - The state is 'not set' with no waiters.
		// - other   - The state is 'not set'.
		//             Points to an 'async_manual_reset_event_operation' that is
		//             the head of a linked-list of waiters.
		mutable std::atomic<void*> m_state;

	};

	class async_manual_reset_event_operation
	{
	public:

		explicit async_manual_reset_event_operation(const async_manual_reset_event& event) noexcept;

		bool await_ready() const noexcept;
		bool await_suspend(cppcoro::coroutine_handle<> awaiter) noexcept;
		void await_resume() const noexcept {}

	private:

		friend class async_manual_reset_event;

		const async_manual_reset_event& m_event;
		async_manual_reset_event_operation* m_next;
		cppcoro::coroutine_handle<> m_awaiter;

	};
}

#endif
