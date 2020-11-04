///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_ASYNC_MUTEX_HPP_INCLUDED
#define CPPCORO_ASYNC_MUTEX_HPP_INCLUDED

#include <cppcoro/coroutine.hpp>
#include <atomic>
#include <cstdint>
#include <mutex> // for std::adopt_lock_t

namespace cppcoro
{
	class async_mutex_lock;
	class async_mutex_lock_operation;
	class async_mutex_scoped_lock_operation;

	/// \brief
	/// A mutex that can be locked asynchronously using 'co_await'.
	///
	/// Ownership of the mutex is not tied to any particular thread.
	/// This allows the coroutine owning the lock to transition from
	/// one thread to another while holding a lock.
	///
	/// Implementation is lock-free, using only std::atomic values for
	/// synchronisation. Awaiting coroutines are suspended without blocking
	/// the current thread if the lock could not be acquired synchronously.
	class async_mutex
	{
	public:

		/// \brief
		/// Construct to a mutex that is not currently locked.
		async_mutex() noexcept;

		/// Destroys the mutex.
		///
		/// Behaviour is undefined if there are any outstanding coroutines
		/// still waiting to acquire the lock.
		~async_mutex();

		/// \brief
		/// Attempt to acquire a lock on the mutex without blocking.
		///
		/// \return
		/// true if the lock was acquired, false if the mutex was already locked.
		/// The caller is responsible for ensuring unlock() is called on the mutex
		/// to release the lock if the lock was acquired by this call.
		bool try_lock() noexcept;

		/// \brief
		/// Acquire a lock on the mutex asynchronously.
		///
		/// If the lock could not be acquired synchronously then the awaiting
		/// coroutine will be suspended and later resumed when the lock becomes
		/// available. If suspended, the coroutine will be resumed inside the
		/// call to unlock() from the previous lock owner.
		///
		/// \return
		/// An operation object that must be 'co_await'ed to wait until the
		/// lock is acquired. The result of the 'co_await m.lock_async()'
		/// expression has type 'void'.
		async_mutex_lock_operation lock_async() noexcept;

		/// \brief
		/// Acquire a lock on the mutex asynchronously, returning an object that
		/// will call unlock() automatically when it goes out of scope.
		///
		/// If the lock could not be acquired synchronously then the awaiting
		/// coroutine will be suspended and later resumed when the lock becomes
		/// available. If suspended, the coroutine will be resumed inside the
		/// call to unlock() from the previous lock owner.
		///
		/// \return
		/// An operation object that must be 'co_await'ed to wait until the
		/// lock is acquired. The result of the 'co_await m.scoped_lock_async()'
		/// expression returns an 'async_mutex_lock' object that will call
		/// this->mutex() when it destructs.
		async_mutex_scoped_lock_operation scoped_lock_async() noexcept;

		/// \brief
		/// Unlock the mutex.
		///
		/// Must only be called by the current lock-holder.
		///
		/// If there are lock operations waiting to acquire the
		/// mutex then the next lock operation in the queue will
		/// be resumed inside this call.
		void unlock();

	private:

		friend class async_mutex_lock_operation;

		static constexpr std::uintptr_t not_locked = 1;

		// assume == reinterpret_cast<std::uintptr_t>(static_cast<void*>(nullptr))
		static constexpr std::uintptr_t locked_no_waiters = 0;

		// This field provides synchronisation for the mutex.
		//
		// It can have three kinds of values:
		// - not_locked
		// - locked_no_waiters
		// - a pointer to the head of a singly linked list of recently
		//   queued async_mutex_lock_operation objects. This list is
		//   in most-recently-queued order as new items are pushed onto
		//   the front of the list.
		std::atomic<std::uintptr_t> m_state;

		// Linked list of async lock operations that are waiting to acquire
		// the mutex. These operations will acquire the lock in the order
		// they appear in this list. Waiters in this list will acquire the
		// mutex before waiters added to the m_newWaiters list.
		async_mutex_lock_operation* m_waiters;

	};

	/// \brief
	/// An object that holds onto a mutex lock for its lifetime and
	/// ensures that the mutex is unlocked when it is destructed.
	///
	/// It is equivalent to a std::lock_guard object but requires
	/// that the result of co_await async_mutex::lock_async() is
	/// passed to the constructor rather than passing the async_mutex
	/// object itself.
	class async_mutex_lock
	{
	public:

		explicit async_mutex_lock(async_mutex& mutex, std::adopt_lock_t) noexcept
			: m_mutex(&mutex)
		{}

		async_mutex_lock(async_mutex_lock&& other) noexcept
			: m_mutex(other.m_mutex)
		{
			other.m_mutex = nullptr;
		}

		async_mutex_lock(const async_mutex_lock& other) = delete;
		async_mutex_lock& operator=(const async_mutex_lock& other) = delete;

		// Releases the lock.
		~async_mutex_lock()
		{
			if (m_mutex != nullptr)
			{
				m_mutex->unlock();
			}
		}

	private:

		async_mutex* m_mutex;

	};

	class async_mutex_lock_operation
	{
	public:

		explicit async_mutex_lock_operation(async_mutex& mutex) noexcept
			: m_mutex(mutex)
		{}

		bool await_ready() const noexcept { return false; }
		bool await_suspend(cppcoro::coroutine_handle<> awaiter) noexcept;
		void await_resume() const noexcept {}

	protected:

		friend class async_mutex;

		async_mutex& m_mutex;

	private:

		async_mutex_lock_operation* m_next;
		cppcoro::coroutine_handle<> m_awaiter;

	};

	class async_mutex_scoped_lock_operation : public async_mutex_lock_operation
	{
	public:

		using async_mutex_lock_operation::async_mutex_lock_operation;

		[[nodiscard]]
		async_mutex_lock await_resume() const noexcept
		{
			return async_mutex_lock{ m_mutex, std::adopt_lock };
		}

	};
}

#endif
