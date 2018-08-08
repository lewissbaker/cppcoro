///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/static_thread_pool.hpp>

#include "auto_reset_event.hpp"
#include "spin_mutex.hpp"
#include "spin_wait.hpp"

#include <cassert>
#include <mutex>
#include <chrono>

namespace
{
	namespace local
	{
		// Keep each thread's local queue under 1MB
		constexpr std::size_t max_local_queue_size = 1024 * 1024 / sizeof(void*);
		constexpr std::size_t initial_local_queue_size = 256;
	}
}

namespace cppcoro
{
	thread_local static_thread_pool::thread_state* static_thread_pool::s_currentState = nullptr;
	thread_local static_thread_pool* static_thread_pool::s_currentThreadPool = nullptr;

	class static_thread_pool::thread_state
	{
	public:

		explicit thread_state()
			: m_localQueue(
				std::make_unique<std::atomic<schedule_operation*>[]>(
					local::initial_local_queue_size))
			, m_mask(local::initial_local_queue_size - 1)
			, m_head(0)
			, m_tail(0)
			, m_isSleeping(false)
		{
		}

		bool try_wake_up()
		{
			if (m_isSleeping.load(std::memory_order_seq_cst))
			{
				if (m_isSleeping.exchange(false, std::memory_order_seq_cst))
				{
					try
					{
						m_wakeUpEvent.set();
					}
					catch (...)
					{
						// TODO: What do we do here?
					}
					return true;
				}
			}

			return false;
		}

		void notify_intent_to_sleep() noexcept
		{
			m_isSleeping.store(true, std::memory_order_relaxed);
		}

		void sleep_until_woken() noexcept
		{
			try
			{
				m_wakeUpEvent.wait();
			}
			catch (...)
			{
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(1ms);
			}
		}

		bool approx_has_any_queued_work() const noexcept
		{
			return difference(
				m_head.load(std::memory_order_relaxed),
				m_tail.load(std::memory_order_relaxed)) > 0;
		}

		bool has_any_queued_work() noexcept
		{
			std::scoped_lock lock{ m_remoteMutex };
			auto tail = m_tail.load(std::memory_order_relaxed);
			auto head = m_head.load(std::memory_order_seq_cst);
			return difference(head, tail) > 0;
		}

		bool try_local_enqueue(schedule_operation*& operation) noexcept
		{
			// Head is only ever written-to by the current thread so we
			// are safe to use relaxed memory order when reading it.
			auto head = m_head.load(std::memory_order_relaxed);

			// It is possible this method may be running concurrently with
			// try_remote_steal() which may have just speculatively incremented m_tail
			// trying to steal the last item in the queue but has not yet read the
			// queue item. So we need to make sure we don't write to the last available
			// space (at slot m_tail - 1) as this may still contain a pointer to an
			// operation that has not yet been executed.
			//
			// Note that it's ok to read stale values from m_tail since new values
			// won't ever decrease the number of available slots by more than 1.
			// Reading a stale value can just mean that sometimes the queue appears
			// empty when it may actually have slots free.
			//
			// Here m_mask is equal to buffersize - 1 so we can only write to a slot
			// if the number of items consumed in the queue (head - tail) is less than
			// the mask.
			auto tail = m_tail.load(std::memory_order_relaxed);
			if (difference(head, tail) < static_cast<offset_t>(m_mask))
			{
				// There is space left in the local buffer.
				m_localQueue[head & m_mask].store(operation, std::memory_order_relaxed);
				m_head.store(head + 1, std::memory_order_seq_cst);
				return true;
			}

			if (m_mask == local::max_local_queue_size)
			{
				// No space in the buffer and we don't want to grow
				// it any further.
				return false;
			}

			// Allocate the new buffer before taking out the lock so that
			// we ensure we hold the lock for as short a time as possible.
			const size_t newSize = (m_mask + 1) * 2;

			std::unique_ptr<std::atomic<schedule_operation*>[]> newLocalQueue{
				new (std::nothrow) std::atomic<schedule_operation*>[newSize]
			};
			if (!newLocalQueue)
			{
				// Unable to allocate more memory.
				return false;
			}

			if (!m_remoteMutex.try_lock())
			{
				// Don't wait to acquire the lock if we can't get it immediately.
				// Fail and let it be enqueued to the global queue.
				// TODO: Should we have a per-thread overflow queue instead?
				return false;
			}

			std::scoped_lock lock{ std::adopt_lock, m_remoteMutex };

			// We can now re-read tail, guaranteed that we are not seeing a stale version.
			tail = m_tail.load(std::memory_order_relaxed);

			// Copy the existing operations.
			const size_t newMask = newSize - 1;
			for (size_t i = tail; i != head; ++i)
			{
				newLocalQueue[i & newMask].store(
					m_localQueue[i & m_mask].load(std::memory_order_relaxed),
					std::memory_order_relaxed);
			}

			// Finally, write the new operation to the queue.
			newLocalQueue[head & newMask].store(operation, std::memory_order_relaxed);

			m_head.store(head + 1, std::memory_order_relaxed);
			m_localQueue = std::move(newLocalQueue);
			m_mask = newMask;
			return true;
		}

		schedule_operation* try_local_pop() noexcept
		{
			// Cheap, approximate, no memory-barrier check for emptiness
			auto head = m_head.load(std::memory_order_relaxed);
			auto tail = m_tail.load(std::memory_order_relaxed);
			if (difference(head, tail) <= 0)
			{
				// Empty
				return nullptr;
			}

			// 3 classes of interleaving of try_local_pop() and try_remote_steal()
			// - local pop completes before remote steal (easy)
			// - remote steal completes before local pop (easy)
			// - both are executed concurrently, both see each other's writes (harder)

			// Speculatively try to acquire the head item of the work queue by
			// decrementing the head cursor. This may race with a concurrent call
			// to try_remote_steal() that is also trying to speculatively increment
			// the tail cursor to steal from the other end of the queue. In the case
			// that they both try to dequeue the last/only item in the queue then we
			// need to fall back to locking to decide who wins

			auto newHead = head - 1;
			m_head.store(newHead, std::memory_order_seq_cst);

			tail = m_tail.load(std::memory_order_seq_cst);

			if (difference(newHead, tail) < 0)
			{
				// There was a race to get the last item.
				// We don't know whether the remote steal saw our write
				// and decided to back off or not, so we acquire the mutex
				// so that we wait until the remote steal has completed so
				// we can see what decision it made.
				std::lock_guard lock{ m_remoteMutex };

				// Use relaxed since the lock guarantees visibility of the writes
				// that the remote steal thread performed.
				tail = m_tail.load(std::memory_order_relaxed);

				if (difference(newHead, tail) < 0)
				{
					// The other thread didn't see our write and stole the last item.
					// We need to restore the head back to it's old value.
					// We hold the mutex so can just use relaxed memory order for this.
					m_head.store(head, std::memory_order_relaxed);
					return nullptr;
				}
			}

			// We successfully acquired an item from the queue.
			return m_localQueue[newHead & m_mask].load(std::memory_order_relaxed);
		}

		schedule_operation* try_steal(bool* lockUnavailable = nullptr) noexcept
		{
			if (lockUnavailable == nullptr)
			{
				m_remoteMutex.lock();
			}
			else if (!m_remoteMutex.try_lock())
			{
				*lockUnavailable = true;
				return nullptr;
			}

			std::scoped_lock lock{ std::adopt_lock, m_remoteMutex };

			auto tail = m_tail.load(std::memory_order_relaxed);
			auto head = m_head.load(std::memory_order_seq_cst);
			if (difference(head, tail) <= 0)
			{
				return nullptr;
			}

			// It looks like there are items in the queue.
			// We'll speculatively try to steal one by incrementing
			// the tail cursor. As this may be running concurrently
			// with try_local_pop() which is also speculatively trying
			// to remove an item from the other end of the queue we
			// need to re-read  the 'head' cursor afterwards to see
			// if there was a potential race to dequeue the last item.
			// Use seq_cst memory order both here and in try_local_pop()
			// to ensure that either we will see their write to head or
			// they will see our write to tail or we will both see each
			// other's writes.
			m_tail.store(tail + 1, std::memory_order_seq_cst);
			head = m_head.load(std::memory_order_seq_cst);

			if (difference(head, tail) > 0)
			{
				// There was still an item in the queue after incrementing tail.
				// We managed to steal an item from the bottom of the stack.
				return m_localQueue[tail & m_mask].load(std::memory_order_relaxed);
			}
			else
			{
				// Otherwise we failed to steal the last item.
				// Restore the old tail position.
				m_tail.store(tail, std::memory_order_seq_cst);
				return nullptr;
			}
		}

	private:

		using offset_t = std::make_signed_t<std::size_t>;

		static constexpr offset_t difference(size_t a, size_t b)
		{
			return static_cast<offset_t>(a - b);
		}

		std::unique_ptr<std::atomic<schedule_operation*>[]> m_localQueue;
		std::size_t m_mask;

#if CPPCORO_COMPILER_MSVC
# pragma warning(push)
# pragma warning(disable : 4324)
#endif

		//alignas(std::hardware_destructive_interference_size)
		std::atomic<std::size_t> m_head;

		//alignas(std::hardware_destructive_interference_size)
		std::atomic<std::size_t> m_tail;

		//alignas(std::hardware_destructive_interference_size)
		std::atomic<bool> m_isSleeping;
		spin_mutex m_remoteMutex;

#if CPPCORO_COMPILER_MSVC
# pragma warning(pop)
#endif

		auto_reset_event m_wakeUpEvent;

	};

	void static_thread_pool::schedule_operation::await_suspend(
		std::experimental::coroutine_handle<> awaitingCoroutine) noexcept
	{
		m_awaitingCoroutine = awaitingCoroutine;
		m_threadPool->schedule_impl(this);
	}

	static_thread_pool::static_thread_pool()
		: static_thread_pool(std::thread::hardware_concurrency())
	{
	}

	static_thread_pool::static_thread_pool(std::uint32_t threadCount)
		: m_threadCount(threadCount > 0 ? threadCount : 1)
		, m_threadStates(std::make_unique<thread_state[]>(m_threadCount))
		, m_stopRequested(false)
		, m_globalQueueHead(nullptr)
		, m_globalQueueTail(nullptr)
		, m_sleepingThreadCount(0)
	{
		m_threads.reserve(threadCount);
		try
		{
			for (std::uint32_t i = 0; i < m_threadCount; ++i)
			{
				m_threads.emplace_back([this, i] { this->run_worker_thread(i); });
			}
		}
		catch (...)
		{
			try
			{
				shutdown();
			}
			catch (...)
			{
				std::terminate();
			}

			throw;
		}
	}

	static_thread_pool::~static_thread_pool()
	{
		shutdown();
	}

	void static_thread_pool::run_worker_thread(std::uint32_t threadIndex) noexcept
	{
		auto& localState = m_threadStates[threadIndex];
		s_currentState = &localState;
		s_currentThreadPool = this;

		auto tryGetRemote = [&]()
		{
			// Try to get some new work first from the global queue
			// then if that queue is empty then try to steal from
			// the local queues of other worker threads.
			// We try to get new work from the global queue first
			// before stealing as stealing from other threads has
			// the side-effect of those threads running out of work
			// sooner and then having to steal work which increases
			// contention.
			auto* op = try_global_dequeue();
			if (op == nullptr)
			{
				op = try_steal_from_other_thread(threadIndex);
			}
			return op;
		};

		while (true)
		{
			// Process operations from the local queue.
			schedule_operation* op;

			while (true)
			{
				op = localState.try_local_pop();
				if (op == nullptr)
				{
					op = tryGetRemote();
					if (op == nullptr)
					{
						break;
					}
				}

				op->m_awaitingCoroutine.resume();
			}

			// No more operations in the local queue or remote queue.
			//
			// We spin for a little while waiting for new items
			// to be enqueued. This avoids the expensive operation
			// of putting the thread to sleep and waking it up again
			// in the case that an external thread is queueing new work

			cppcoro::spin_wait spinWait;
			while (true)
			{
				for (int i = 0; i < 30; ++i)
				{
					if (is_shutdown_requested())
					{
						return;
					}

					spinWait.spin_one();

					if (approx_has_any_queued_work_for(threadIndex))
					{
						op = tryGetRemote();
						if (op != nullptr)
						{
							// Now that we've executed some work we can
							// return to normal processing since this work
							// might have queued some more work to the local
							// queue which we should process first.
							goto normal_processing;
						}
					}
				}

				// We didn't find any work after spinning for a while, let's
				// put ourselves to sleep and wait to be woken up.

				// First, let other threads know we're going to sleep.
				notify_intent_to_sleep(threadIndex);

				// As notifying the other threads that we're sleeping may have
				// raced with other threads enqueueing more work, we need to
				// re-check whether there is any more work to be done so that
				// we don't get into a situation where we go to sleep and another
				// thread has enqueued some work and doesn't know to wake us up.

				if (has_any_queued_work_for(threadIndex))
				{
					op = tryGetRemote();
					if (op != nullptr)
					{
						// Try to clear the intent to sleep so that some other thread
						// that subsequently enqueues some work won't mistakenly try
						// to wake this threadup when we are already running as there
						// might have been some other thread that it could have woken
						// up instead which could have resulted in increased parallelism.
						//
						// However, it's possible that some other thread may have already
						// tried to wake us up, in which case the auto_reset_event used to
						// wake up this thread may already be in the 'set' state. Leaving
						// it in this state won't really hurt. It'll just mean we might get
						// a spurious wake-up next time we try to go to sleep.
						try_clear_intent_to_sleep(threadIndex);

						goto normal_processing;
					}
				}

				if (is_shutdown_requested())
				{
					return;
				}

				localState.sleep_until_woken();
			}

		normal_processing:
			assert(op != nullptr);
			op->m_awaitingCoroutine.resume();
		}
	}

	void static_thread_pool::shutdown()
	{
		m_stopRequested.store(true, std::memory_order_relaxed);

		for (std::uint32_t i = 0; i < m_threads.size(); ++i)
		{
			auto& threadState = m_threadStates[i];

			// We should not be shutting down the thread pool if there is any
			// outstanding work in the queue. It is up to the application to
			// ensure all enqueued work has completed first.
			assert(!threadState.has_any_queued_work());

			threadState.try_wake_up();
		}

		for (auto& t : m_threads)
		{
			t.join();
		}
	}

	void static_thread_pool::schedule_impl(schedule_operation* operation) noexcept
	{
		if (s_currentThreadPool != this ||
			!s_currentState->try_local_enqueue(operation))
		{
			remote_enqueue(operation);
		}

		wake_one_thread();
	}

	void static_thread_pool::remote_enqueue(schedule_operation* operation) noexcept
	{
		auto* tail = m_globalQueueTail.load(std::memory_order_relaxed);
		do
		{
			operation->m_next = tail;
		} while (!m_globalQueueTail.compare_exchange_weak(
			tail,
			operation,
			std::memory_order_seq_cst,
			std::memory_order_relaxed));
	}

	bool static_thread_pool::has_any_queued_work_for(std::uint32_t threadIndex) noexcept
	{
		if (m_globalQueueTail.load(std::memory_order_seq_cst) != nullptr)
		{
			return true;
		}

		if (m_globalQueueHead.load(std::memory_order_seq_cst) != nullptr)
		{
			return true;
		}

		for (std::uint32_t i = 0; i < m_threadCount; ++i)
		{
			if (i == threadIndex) continue;
			if (m_threadStates[i].has_any_queued_work())
			{
				return true;
			}
		}

		return false;
	}

	bool static_thread_pool::approx_has_any_queued_work_for(std::uint32_t threadIndex) const noexcept
	{
		// Cheap, approximate, read-only implementation that checks whether any work has
		// been queued in the system somewhere. We try to avoid writes here so that we
		// don't bounce cache-lines around between threads/cores unnecessarily when
		// multiple threads are all spinning waiting for work.

		if (m_globalQueueTail.load(std::memory_order_relaxed) != nullptr)
		{
			return true;
		}

		if (m_globalQueueHead.load(std::memory_order_relaxed) != nullptr)
		{
			return true;
		}

		for (std::uint32_t i = 0; i < m_threadCount; ++i)
		{
			if (i == threadIndex) continue;
			if (m_threadStates[i].approx_has_any_queued_work())
			{
				return true;
			}
		}

		return false;
	}

	bool static_thread_pool::is_shutdown_requested() const noexcept
	{
		return m_stopRequested.load(std::memory_order_relaxed);
	}

	void static_thread_pool::notify_intent_to_sleep(std::uint32_t threadIndex) noexcept
	{
		// First mark the thread as asleep
		m_threadStates[threadIndex].notify_intent_to_sleep();

		// Then publish the fact that a thread is asleep by incrementing the count
		// of threads that are asleep.
		m_sleepingThreadCount.fetch_add(1, std::memory_order_seq_cst);
	}

	void static_thread_pool::try_clear_intent_to_sleep(std::uint32_t threadIndex) noexcept
	{
		// First try to claim that we are waking up one of the threads.
		std::uint32_t oldSleepingCount = m_sleepingThreadCount.load(std::memory_order_relaxed);
		do
		{
			if (oldSleepingCount == 0)
			{
				// No more sleeping threads.
				// Someone must have woken us up.
				return;
			}
		} while (!m_sleepingThreadCount.compare_exchange_weak(
			oldSleepingCount,
			oldSleepingCount - 1,
			std::memory_order_acquire,
			std::memory_order_relaxed));

		// Then preferentially try to wake up our thread.
		// If some other thread has already requested that this thread wake up
		// then we will wake up another thread - the one that should have been woken
		// up by the thread that woke this thread up.
		if (!m_threadStates[threadIndex].try_wake_up())
		{
			for (std::uint32_t i = 0; i < m_threadCount; ++i)
			{
				if (i == threadIndex) continue;
				if (m_threadStates[i].try_wake_up())
				{
					return;
				}
			}
		}
	}

	static_thread_pool::schedule_operation*
	static_thread_pool::try_global_dequeue() noexcept
	{
		std::scoped_lock lock{ m_globalQueueMutex };

		auto* head = m_globalQueueHead.load(std::memory_order_relaxed);
		if (head == nullptr)
		{
			// Use seq-cst memory order so that when we check for an item in the
			// global queue after signalling an intent to sleep that either we
			// will see their enqueue or they will see our signal to sleep and
			// wake us up.
			if (m_globalQueueTail.load(std::memory_order_seq_cst) == nullptr)
			{
				return nullptr;
			}

			// Acquire the entire set of queued operations in a single operation.
			auto* tail = m_globalQueueTail.exchange(nullptr, std::memory_order_acquire);
			if (tail == nullptr)
			{
				return nullptr;
			}

			// Reverse the list 
			do
			{
				auto* next = std::exchange(tail->m_next, head);
				head = std::exchange(tail, next);
			} while (tail != nullptr);
		}

		m_globalQueueHead = head->m_next;

		return head;
	}

	static_thread_pool::schedule_operation*
	static_thread_pool::try_steal_from_other_thread(std::uint32_t thisThreadIndex) noexcept
	{
		// Try first with non-blocking steal attempts.

		bool anyLocksUnavailable = false;
		for (std::uint32_t otherThreadIndex = 0; otherThreadIndex < m_threadCount; ++otherThreadIndex)
		{
			if (otherThreadIndex == thisThreadIndex) continue;
			auto& otherThreadState = m_threadStates[otherThreadIndex];
			auto* op = otherThreadState.try_steal(&anyLocksUnavailable);
			if (op != nullptr)
			{
				return op;
			}
		}

		if (anyLocksUnavailable)
		{
			// We didn't check all of the other threads for work to steal yet.
			// Try again, this time waiting to acquire the locks.
			for (std::uint32_t otherThreadIndex = 0; otherThreadIndex < m_threadCount; ++otherThreadIndex)
			{
				if (otherThreadIndex == thisThreadIndex) continue;
				auto& otherThreadState = m_threadStates[otherThreadIndex];
				auto* op = otherThreadState.try_steal();
				if (op != nullptr)
				{
					return op;
				}
			}
		}

		return nullptr;
	}

	void static_thread_pool::wake_one_thread() noexcept
	{
		// First try to claim responsibility for waking up one thread.
		// This first read must be seq_cst to ensure that either we have
		// visibility of another thread going to sleep or they have
		// visibility of our prior enqueue of an item.
		std::uint32_t oldSleepingCount = m_sleepingThreadCount.load(std::memory_order_seq_cst);
		do
		{
			if (oldSleepingCount == 0)
			{
				// No sleeping threads.
				// Someone must have woken us up.
				return;
			}
		} while (!m_sleepingThreadCount.compare_exchange_weak(
			oldSleepingCount,
			oldSleepingCount - 1,
			std::memory_order_acquire,
			std::memory_order_relaxed));

		// Now that we have claimed responsibility for waking a thread up
		// we need to find a sleeping thread and wake it up. We should be
		// guaranteed of finding a thread to wake-up here, but not necessarily
		// in a single pass due to threads potentially waking themselves up
		// in try_clear_intent_to_sleep().
		while (true)
		{
			for (std::uint32_t i = 0; i < m_threadCount; ++i)
			{
				if (m_threadStates[i].try_wake_up())
				{
					return;
				}
			}
		}
	}
}
