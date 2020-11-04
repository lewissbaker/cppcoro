///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/async_auto_reset_event.hpp>

#include <cppcoro/config.hpp>

#include <cassert>
#include <algorithm>

namespace
{
	namespace local
	{
		// Some helpers for manipulating the 'm_state' value.

		constexpr std::uint64_t set_increment = 1;
		constexpr std::uint64_t waiter_increment = std::uint64_t(1) << 32;

		constexpr std::uint32_t get_set_count(std::uint64_t state)
		{
			return static_cast<std::uint32_t>(state);
		}

		constexpr std::uint32_t get_waiter_count(std::uint64_t state)
		{
			return static_cast<std::uint32_t>(state >> 32);
		}

		constexpr std::uint32_t get_resumable_waiter_count(std::uint64_t state)
		{
			return std::min(get_set_count(state), get_waiter_count(state));
		}
	}
}

cppcoro::async_auto_reset_event::async_auto_reset_event(bool initiallySet) noexcept
	: m_state(initiallySet ? local::set_increment : 0)
	, m_newWaiters(nullptr)
	, m_waiters(nullptr)
{
}

cppcoro::async_auto_reset_event::~async_auto_reset_event()
{
	assert(m_newWaiters.load(std::memory_order_relaxed) == nullptr);
	assert(m_waiters == nullptr);
}

cppcoro::async_auto_reset_event_operation
cppcoro::async_auto_reset_event::operator co_await() const noexcept
{
	std::uint64_t oldState = m_state.load(std::memory_order_relaxed);
	if (local::get_set_count(oldState) > local::get_waiter_count(oldState))
	{
		// Try to synchronously acquire the event.
		if (m_state.compare_exchange_strong(
			oldState,
			oldState - local::set_increment,
			std::memory_order_acquire,
			std::memory_order_relaxed))
		{
			// Acquired the event, return an operation object that
			// won't suspend.
			return async_auto_reset_event_operation{};
		}
	}

	return async_auto_reset_event_operation{ *this };
}

void cppcoro::async_auto_reset_event::set() noexcept
{
	std::uint64_t oldState = m_state.load(std::memory_order_relaxed);
	do
	{
		if (local::get_set_count(oldState) > local::get_waiter_count(oldState))
		{
			// Already set.
			return;
		}

		// Increment the set-count
	} while (!m_state.compare_exchange_weak(
		oldState,
		oldState + local::set_increment,
		std::memory_order_acq_rel,
		std::memory_order_acquire));

	// Did we transition from non-zero waiters and zero set-count
	// to non-zero set-count?
	// If so then we acquired the lock and are responsible for resuming waiters.
	if (oldState != 0 && local::get_set_count(oldState) == 0)
	{
		// We acquired the lock.
		resume_waiters(oldState + local::set_increment);
	}
}

void cppcoro::async_auto_reset_event::reset() noexcept
{
	std::uint64_t oldState = m_state.load(std::memory_order_relaxed);
	while (local::get_set_count(oldState) > local::get_waiter_count(oldState))
	{
		if (m_state.compare_exchange_weak(
			oldState,
			oldState - local::set_increment,
			std::memory_order_relaxed))
		{
			// Successfully reset.
			return;
		}
	}

	// Not set. Nothing to do.
}

void cppcoro::async_auto_reset_event::resume_waiters(
	std::uint64_t initialState) const noexcept
{
	async_auto_reset_event_operation* waitersToResumeList = nullptr;
	async_auto_reset_event_operation** waitersToResumeListEnd = &waitersToResumeList;

	std::uint32_t waiterCountToResume = local::get_resumable_waiter_count(initialState);

	assert(waiterCountToResume > 0);

	do
	{
		// Dequeue 'waiterCountToResume' from m_waiters/m_newWaiters and
		// push them onto 'waitersToResumeList'.
		for (std::uint32_t i = 0; i < waiterCountToResume; ++i)
		{
			if (m_waiters == nullptr)
			{
				// We've run out of of waiters that we can consume without synchronisation
				// Dequeue the list of new waiters atomically.
				auto* newWaiters = m_newWaiters.exchange(nullptr, std::memory_order_acquire);

				// There should always be enough waiters in the list as
				// the waiters are queued before the waiter-count is incremented.
				assert(newWaiters != nullptr);
				CPPCORO_ASSUME(newWaiters != nullptr);

				// Reverse order of new waiters so they are resumed in FIFO.
				// This ensures fairness.
				//
				// The alternative would be to not reverse the list and instead
				// resume waiters in the reverse order they were queued in.
				// This might result in better cache locality (most recently
				// suspended coroutine might still be in cache).
				// It should still provide a bounded wait time as well since we
				// are guaranteed to process all waiters in this list before
				// looking at any waiters newly queued after this point.
				// Something to consider.
				do
				{
					auto* next = newWaiters->m_next;
					newWaiters->m_next = m_waiters;
					m_waiters = newWaiters;
					newWaiters = next;
				} while (newWaiters != nullptr);
			}

			assert(m_waiters != nullptr);

			// Pop the next waiter off the list
			auto* waiterToResume = m_waiters;
			m_waiters = m_waiters->m_next;

			// Push it onto the end of the list of waiters to resume
			waiterToResume->m_next = nullptr;
			*waitersToResumeListEnd = waiterToResume;
			waitersToResumeListEnd = &waiterToResume->m_next;
		}

		// We've now removed 'waiterCountToResume' waiters from the list
		// so we can now decrement both the waiter and set count.
		//
		// However, there might have been more waiters or more calls to
		// set() since we last checked so we need to go around again if
		// there are still waiters that are ready to resume after decrementing
		// both the 'waiter count' and 'set count' by 'waiterCountToResume'.
		const std::uint64_t delta =
			std::uint64_t(waiterCountToResume) |
			std::uint64_t(waiterCountToResume) << 32;

		// Needs to be 'release' as we're releasing the lock and anyone that
		// subsequently acquires the lock needs to see our prior writes to
		// m_waiters.
		// Needs to be 'acquire' in the case that new waiters were added so
		// that we see their prior writes to 'm_newWaiters'.
		const std::uint64_t newState =
			m_state.fetch_sub(delta, std::memory_order_acq_rel) - delta;

		waiterCountToResume = local::get_resumable_waiter_count(newState);
	} while (waiterCountToResume > 0);

	// Now resume all of the waiters we've dequeued.
	// There should be at least one.
	assert(waitersToResumeList != nullptr);
	CPPCORO_ASSUME(waitersToResumeList != nullptr);

	do
	{
		auto* const waiter = waitersToResumeList;

		// Read 'next' before resuming since resuming the waiter is
		// likely to destroy the waiter object.
		auto* const next = waitersToResumeList->m_next;

		// Decrement reference count and see if we decremented the last
		// reference and if so then we are responsible for resuming.
		// If not, then await_suspend() is responsible for resuming by
		// returning 'false' and not suspending.
		if (waiter->m_refCount.fetch_sub(1, std::memory_order_release) == 1)
		{
			waiter->m_awaiter.resume();
		}

		waitersToResumeList = next;
	} while (waitersToResumeList != nullptr);
}

cppcoro::async_auto_reset_event_operation::async_auto_reset_event_operation() noexcept
	: m_event(nullptr)
{}

cppcoro::async_auto_reset_event_operation::async_auto_reset_event_operation(
	const async_auto_reset_event& event) noexcept
	: m_event(&event)
	, m_refCount(2)
{}

cppcoro::async_auto_reset_event_operation::async_auto_reset_event_operation(
	const async_auto_reset_event_operation& other) noexcept
	: m_event(other.m_event)
	, m_refCount(2)
{}

bool cppcoro::async_auto_reset_event_operation::await_suspend(
	cppcoro::coroutine_handle<> awaiter) noexcept
{
	m_awaiter = awaiter;

	// Queue the waiter to the m_newWaiters list.
	async_auto_reset_event_operation* head = m_event->m_newWaiters.load(std::memory_order_relaxed);
	do
	{
		m_next = head;
	} while (!m_event->m_newWaiters.compare_exchange_weak(
		head,
		this,
		std::memory_order_release,
		std::memory_order_relaxed));

	// Increment the waiter count.
	// Needs to be 'release' so that our prior write to m_newWaiters is
	// visible to anyone that acquires the lock.
	// Needs to be 'acquire' in case we acquired the lock so we can see
	// others' writes to m_newWaiters and writes prior to set() calls.
	const std::uint64_t oldState =
		m_event->m_state.fetch_add(local::waiter_increment, std::memory_order_acq_rel);

	if (oldState != 0 && local::get_waiter_count(oldState) == 0)
	{
		// We transitioned from non-zero set and zero waiters to
		// non-zero set and non-zero waiters, so we acquired the lock
		// and thus responsibility for resuming waiters.
		m_event->resume_waiters(oldState + local::waiter_increment);
	}

	// Decrement the ref-count to indicate that this waiter is now safe
	// to resume. We don't want it to resume while we're still accessing the
	// m_event object as resuming it might cause the event object to be
	// destructed.
	//
	// Need 'acquire' semantics here in the case that another thread has
	// concurrently dequeued us and scheduled us for resumption by decrementing
	// the ref-count with 'release' semantics so that we see the writes prior
	// to the 'set()' call that released this waiter.
	return m_refCount.fetch_sub(1, std::memory_order_acquire) != 1;
}
