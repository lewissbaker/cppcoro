///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_SINGLE_CONSUMER_EVENT_HPP_INCLUDED
#define CPPCORO_SINGLE_CONSUMER_EVENT_HPP_INCLUDED

#include <atomic>
#include <cppcoro/coroutine.hpp>

namespace cppcoro
{
	/// \brief
	/// A manual-reset event that supports only a single awaiting
	/// coroutine at a time.
	///
	/// You can co_await the event to suspend the current coroutine until
	/// some thread calls set(). If the event is already set then the
	/// coroutine will not be suspended and will continue execution.
	/// If the event was not yet set then the coroutine will be resumed
	/// on the thread that calls set() within the call to set().
	///
	/// Callers must ensure that only one coroutine is executing a
	/// co_await statement at any point in time.
	class single_consumer_event
	{
	public:

		/// \brief
		/// Construct a new event, initialising to either 'set' or 'not set' state.
		///
		/// \param initiallySet
		/// If true then initialises the event to the 'set' state.
		/// Otherwise, initialised the event to the 'not set' state.
		single_consumer_event(bool initiallySet = false) noexcept
			: m_state(initiallySet ? state::set : state::not_set)
		{}

		/// Query if this event has been set.
		bool is_set() const noexcept
		{
			return m_state.load(std::memory_order_acquire) == state::set;
		}

		/// \brief
		/// Transition this event to the 'set' state if it is not already set.
		///
		/// If there was a coroutine awaiting the event then it will be resumed
		/// inside this call.
		void set()
		{
			const state oldState = m_state.exchange(state::set, std::memory_order_acq_rel);
			if (oldState == state::not_set_consumer_waiting)
			{
				m_awaiter.resume();
			}
		}

		/// \brief
		/// Transition this event to the 'non set' state if it was in the set state.
		void reset() noexcept
		{
			state oldState = state::set;
			m_state.compare_exchange_strong(oldState, state::not_set, std::memory_order_relaxed);
		}

		/// \brief
		/// Wait until the event becomes set.
		///
		/// If the event is already set then the awaiting coroutine will not be suspended
		/// and will continue execution. If the event was not yet set then the coroutine
		/// will be suspended and will be later resumed inside a subsequent call to set()
		/// on the thread that calls set().
		auto operator co_await() noexcept
		{
			class awaiter
			{
			public:

				awaiter(single_consumer_event& event) : m_event(event) {}

				bool await_ready() const noexcept
				{
					return m_event.is_set();
				}

				bool await_suspend(cppcoro::coroutine_handle<> awaiter)
				{
					m_event.m_awaiter = awaiter;

					state oldState = state::not_set;
					return m_event.m_state.compare_exchange_strong(
						oldState,
						state::not_set_consumer_waiting,
						std::memory_order_release,
						std::memory_order_acquire);
				}

				void await_resume() noexcept {}

			private:

				single_consumer_event& m_event;

			};

			return awaiter{ *this };
		}

	private:

		enum class state
		{
			not_set,
			not_set_consumer_waiting,
			set
		};

		// TODO: Merge these two fields into a single std::atomic<std::uintptr_t>
		// by encoding 'not_set' as 0 (nullptr), 'set' as 1 and
		// 'not_set_consumer_waiting' as a coroutine handle pointer.
		std::atomic<state> m_state;
		cppcoro::coroutine_handle<> m_awaiter;

	};
}

#endif
