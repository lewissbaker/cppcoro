///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_ASYNC_AUTO_RESET_EVENT_HPP_INCLUDED
#define CPPCORO_ASYNC_AUTO_RESET_EVENT_HPP_INCLUDED

#include <cppcoro/coroutine.hpp>
#include <atomic>
#include <cstdint>

namespace cppcoro
{
	class async_auto_reset_event_operation;

	/// An async auto-reset event is a coroutine synchronisation abstraction
	/// that allows one or more coroutines to wait until some thread calls
	/// set() on the event.
	///
	/// When a coroutine awaits a 'set' event the event is automatically
	/// reset back to the 'not set' state, thus the name 'auto reset' event.
	class async_auto_reset_event
	{
	public:

		/// Initialise the event to either 'set' or 'not set' state.
		async_auto_reset_event(bool initiallySet = false) noexcept;

		~async_auto_reset_event();

		/// Wait for the event to enter the 'set' state.
		///
		/// If the event is already 'set' then the event is set to the 'not set'
		/// state and the awaiting coroutine continues without suspending.
		/// Otherwise, the coroutine is suspended and later resumed when some
		/// thread calls 'set()'.
		///
		/// Note that the coroutine may be resumed inside a call to 'set()'
		/// or inside another thread's call to 'operator co_await()'.
		async_auto_reset_event_operation operator co_await() const noexcept;

		/// Set the state of the event to 'set'.
		///
		/// If there are pending coroutines awaiting the event then one
		/// pending coroutine is resumed and the state is immediately
		/// set back to the 'not set' state.
		///
		/// This operation is a no-op if the event was already 'set'.
		void set() noexcept;

		/// Set the state of the event to 'not-set'.
		///
		/// This is a no-op if the state was already 'not set'.
		void reset() noexcept;

	private:

		friend class async_auto_reset_event_operation;

		void resume_waiters(std::uint64_t initialState) const noexcept;

		// Bits 0-31  - Set count
		// Bits 32-63 - Waiter count
		mutable std::atomic<std::uint64_t> m_state;

		mutable std::atomic<async_auto_reset_event_operation*> m_newWaiters;

		mutable async_auto_reset_event_operation* m_waiters;

	};

	class async_auto_reset_event_operation
	{
	public:

		async_auto_reset_event_operation() noexcept;

		explicit async_auto_reset_event_operation(const async_auto_reset_event& event) noexcept;

		async_auto_reset_event_operation(const async_auto_reset_event_operation& other) noexcept;

		bool await_ready() const noexcept { return m_event == nullptr; }
		bool await_suspend(cppcoro::coroutine_handle<> awaiter) noexcept;
		void await_resume() const noexcept {}

	private:

		friend class async_auto_reset_event;

		const async_auto_reset_event* m_event;
		async_auto_reset_event_operation* m_next;
		cppcoro::coroutine_handle<> m_awaiter;
		std::atomic<std::uint32_t> m_refCount;

	};
}

#endif
