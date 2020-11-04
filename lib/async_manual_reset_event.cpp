///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/async_manual_reset_event.hpp>

#include <cppcoro/config.hpp>

#include <cassert>

cppcoro::async_manual_reset_event::async_manual_reset_event(bool initiallySet) noexcept
	: m_state(initiallySet ? static_cast<void*>(this) : nullptr)
{}

cppcoro::async_manual_reset_event::~async_manual_reset_event()
{
	// There should be no coroutines still awaiting the event.
	assert(
		m_state.load(std::memory_order_relaxed) == nullptr ||
		m_state.load(std::memory_order_relaxed) == static_cast<void*>(this));
}

bool cppcoro::async_manual_reset_event::is_set() const noexcept
{
	return m_state.load(std::memory_order_acquire) == static_cast<const void*>(this);
}

cppcoro::async_manual_reset_event_operation
cppcoro::async_manual_reset_event::operator co_await() const noexcept
{
	return async_manual_reset_event_operation{ *this };
}

void cppcoro::async_manual_reset_event::set() noexcept
{
	void* const setState = static_cast<void*>(this);

	// Needs 'release' semantics so that prior writes are visible to event awaiters
	// that synchronise either via 'is_set()' or 'operator co_await()'.
	// Needs 'acquire' semantics in case there are any waiters so that we see
	// prior writes to the waiting coroutine's state and to the contents of
	// the queued async_manual_reset_event_operation objects.
	void* oldState = m_state.exchange(setState, std::memory_order_acq_rel);
	if (oldState != setState)
	{
		auto* current = static_cast<async_manual_reset_event_operation*>(oldState);
		while (current != nullptr)
		{
			auto* next = current->m_next;
			current->m_awaiter.resume();
			current = next;
		}
	}
}

void cppcoro::async_manual_reset_event::reset() noexcept
{
	void* oldState = static_cast<void*>(this);
	m_state.compare_exchange_strong(oldState, nullptr, std::memory_order_relaxed);
}

cppcoro::async_manual_reset_event_operation::async_manual_reset_event_operation(
	const async_manual_reset_event& event) noexcept
	: m_event(event)
{
}

bool cppcoro::async_manual_reset_event_operation::await_ready() const noexcept
{
	return m_event.is_set();
}

bool cppcoro::async_manual_reset_event_operation::await_suspend(
	cppcoro::coroutine_handle<> awaiter) noexcept
{
	m_awaiter = awaiter;

	const void* const setState = static_cast<const void*>(&m_event);

	void* oldState = m_event.m_state.load(std::memory_order_acquire);
	do
	{
		if (oldState == setState)
		{
			// State is now 'set' no need to suspend.
			return false;
		}

		m_next = static_cast<async_manual_reset_event_operation*>(oldState);
	} while (!m_event.m_state.compare_exchange_weak(
		oldState,
		static_cast<void*>(this),
		std::memory_order_release,
		std::memory_order_acquire));

	// Successfully queued this waiter to the list.
	return true;
}
