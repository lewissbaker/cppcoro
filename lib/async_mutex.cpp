///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/async_mutex.hpp>

#include <cassert>

cppcoro::async_mutex::async_mutex() noexcept
	: m_state(not_locked)
	, m_waiters(nullptr)
{}

cppcoro::async_mutex::~async_mutex()
{
	[[maybe_unused]] auto state = m_state.load(std::memory_order_relaxed);
	assert(state == not_locked || state == locked_no_waiters);
	assert(m_waiters == nullptr);
}

bool cppcoro::async_mutex::try_lock() noexcept
{
	// Try to atomically transition from nullptr (not-locked) -> this (locked-no-waiters).
	auto oldState = not_locked;
	return m_state.compare_exchange_strong(
		oldState,
		locked_no_waiters,
		std::memory_order_acquire,
		std::memory_order_relaxed);
}

cppcoro::async_mutex_lock_operation cppcoro::async_mutex::lock_async() noexcept
{
	return async_mutex_lock_operation{ *this };
}

cppcoro::async_mutex_scoped_lock_operation cppcoro::async_mutex::scoped_lock_async() noexcept
{
	return async_mutex_scoped_lock_operation{ *this };
}

void cppcoro::async_mutex::unlock()
{
	assert(m_state.load(std::memory_order_relaxed) != not_locked);

	async_mutex_lock_operation* waitersHead = m_waiters;
	if (waitersHead == nullptr)
	{
		auto oldState = locked_no_waiters;
		const bool releasedLock = m_state.compare_exchange_strong(
			oldState,
			not_locked,
			std::memory_order_release,
			std::memory_order_relaxed);
		if (releasedLock)
		{
			return;
		}

		// At least one new waiter.
		// Acquire the list of new waiter operations atomically.
		oldState = m_state.exchange(locked_no_waiters, std::memory_order_acquire);

		assert(oldState != locked_no_waiters && oldState != not_locked);

		// Transfer the list to m_waiters, reversing the list in the process so
		// that the head of the list is the first to be resumed.
		auto* next = reinterpret_cast<async_mutex_lock_operation*>(oldState);
		do
		{
			auto* temp = next->m_next;
			next->m_next = waitersHead; 
			waitersHead = next;
			next = temp;
		} while (next != nullptr);
	}

	assert(waitersHead != nullptr);

	m_waiters = waitersHead->m_next;

	// Resume the waiter.
	// This will pass the ownership of the lock on to that operation/coroutine.
	waitersHead->m_awaiter.resume();
}

bool cppcoro::async_mutex_lock_operation::await_suspend(cppcoro::coroutine_handle<> awaiter) noexcept
{
	m_awaiter = awaiter;

	std::uintptr_t oldState = m_mutex.m_state.load(std::memory_order_acquire);
	while (true)
	{
		if (oldState == async_mutex::not_locked)
		{
			if (m_mutex.m_state.compare_exchange_weak(
				oldState,
				async_mutex::locked_no_waiters,
				std::memory_order_acquire,
				std::memory_order_relaxed))
			{
				// Acquired lock, don't suspend.
				return false;
			}
		}
		else
		{
			// Try to push this operation onto the head of the waiter stack.
			m_next = reinterpret_cast<async_mutex_lock_operation*>(oldState);
			if (m_mutex.m_state.compare_exchange_weak(
				oldState,
				reinterpret_cast<std::uintptr_t>(this),
				std::memory_order_release,
				std::memory_order_relaxed))
			{
				// Queued operation to waiters list, suspend now.
				return true;
			}
		}
	}
}
