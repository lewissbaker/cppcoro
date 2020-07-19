///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_SEQUENCE_BARRIER_HPP_INCLUDED
#define CPPCORO_SEQUENCE_BARRIER_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/awaitable_traits.hpp>
#include <cppcoro/sequence_traits.hpp>
#include <cppcoro/detail/manual_lifetime.hpp>

#include <atomic>
#include <cassert>
#include <cstdint>
#include <limits>
#include <optional>
#include <cppcoro/coroutine.hpp>

namespace cppcoro
{
	template<typename SEQUENCE, typename TRAITS>
	class sequence_barrier_wait_operation_base;

	template<typename SEQUENCE, typename TRAITS, typename SCHEDULER>
	class sequence_barrier_wait_operation;

	/// A sequence barrier is a synchronisation primitive that allows a single-producer
	/// and multiple-consumers to coordinate with respect to a monotonically increasing
	/// sequence number.
	///
	/// A single producer advances the sequence number by publishing new sequence numbers in a
	/// monotonically increasing order. One or more consumers can query the last-published
	/// sequence number and can wait until a particular sequence number has been published.
	///
	/// A sequence barrier can be used to represent a cursor into a thread-safe producer/consumer
	/// ring-buffer.
	///
	/// See the LMAX Disruptor pattern for more background:
	/// https://lmax-exchange.github.io/disruptor/files/Disruptor-1.0.pdf
	template<
		typename SEQUENCE = std::size_t,
		typename TRAITS = sequence_traits<SEQUENCE>>
	class sequence_barrier
	{
		static_assert(
			std::is_integral_v<SEQUENCE>,
			"sequence_barrier requires an integral sequence type");

		using awaiter_t = sequence_barrier_wait_operation_base<SEQUENCE, TRAITS>;

	public:

		/// Construct a sequence barrier with the specified initial sequence number
		/// as the initial value 'last_published()'.
		sequence_barrier(SEQUENCE initialSequence = TRAITS::initial_sequence) noexcept
			: m_lastPublished(initialSequence)
			, m_awaiters(nullptr)
		{}

		~sequence_barrier()
		{
			// Shouldn't be destructing a sequence barrier if there are still waiters.
			assert(m_awaiters.load(std::memory_order_relaxed) == nullptr);
		}

		/// Query the sequence number that was most recently published by the producer.
		///
		/// You can assume that all sequence numbers prior to the returned sequence number
		/// have also been published. This means you can safely access all elements with
		/// sequence numbers up to and including the returned sequence number without any
		/// further synchronisation.
		SEQUENCE last_published() const noexcept
		{
			return m_lastPublished.load(std::memory_order_acquire);
		}

		/// Wait until a particular sequence number has been published.
		///
		/// If the specified sequence number is not yet published then the awaiting coroutine
		/// will be suspended and later resumed inside the call to publish() that publishes
		/// the specified sequence number.
		///
		/// \param targetSequence
		/// The sequence number to wait for.
		///
		/// \return
		/// An awaitable that when co_await'ed will suspend the awaiting coroutine until
		/// the specified target sequence number has been published.
		/// The result of the co_await expression will be the last-known published sequence
		/// number. This is guaranteed not to precede \p targetSequence but may be a sequence
		/// number after \p targetSequence, which indicates that more elements have been
		/// published than you were waiting for.
		template<typename SCHEDULER>
		[[nodiscard]]
		sequence_barrier_wait_operation<SEQUENCE, TRAITS, SCHEDULER> wait_until_published(
			SEQUENCE targetSequence,
			SCHEDULER& scheduler) const noexcept;

		/// Publish the specified sequence number to consumers.
		///
		/// This publishes all sequence numbers up to and including the specified sequence
		/// number. This will resume any coroutine that was suspended waiting for a sequence
		/// number that was published by this operation.
		///
		/// \param sequence
		/// The sequence number to publish. This number must not precede the current
		/// last_published() value. ie. the published sequence numbers must be monotonically
		/// increasing.
		void publish(SEQUENCE sequence) noexcept;

	private:

		friend class sequence_barrier_wait_operation_base<SEQUENCE, TRAITS>;

		void add_awaiter(awaiter_t* awaiter) const noexcept;

#if CPPCORO_COMPILER_MSVC
# pragma warning(push)
# pragma warning(disable : 4324) // C4324: structure was padded due to alignment specifier
#endif

		// First cache-line is written to by the producer only
		alignas(CPPCORO_CPU_CACHE_LINE)
		std::atomic<SEQUENCE> m_lastPublished;

		// Second cache-line is written to by both the producer and consumers
		alignas(CPPCORO_CPU_CACHE_LINE)
		mutable std::atomic<awaiter_t*> m_awaiters;

#if CPPCORO_COMPILER_MSVC
# pragma warning(pop)
#endif

	};

	template<typename SEQUENCE, typename TRAITS>
	class sequence_barrier_wait_operation_base
	{
	public:

		explicit sequence_barrier_wait_operation_base(
			const sequence_barrier<SEQUENCE, TRAITS>& barrier,
			SEQUENCE targetSequence) noexcept
			: m_barrier(barrier)
			, m_targetSequence(targetSequence)
			, m_lastKnownPublished(barrier.last_published())
			, m_readyToResume(false)
		{}

		sequence_barrier_wait_operation_base(
			const sequence_barrier_wait_operation_base& other) noexcept
			: m_barrier(other.m_barrier)
			, m_targetSequence(other.m_targetSequence)
			, m_lastKnownPublished(other.m_lastKnownPublished)
			, m_readyToResume(false)
		{}

		bool await_ready() const noexcept
		{
			return !TRAITS::precedes(m_lastKnownPublished, m_targetSequence);
		}

		bool await_suspend(cppcoro::coroutine_handle<> awaitingCoroutine) noexcept
		{
			m_awaitingCoroutine = awaitingCoroutine;
			m_barrier.add_awaiter(this);
			return !m_readyToResume.exchange(true, std::memory_order_acquire);
		}

		SEQUENCE await_resume() noexcept
		{
			return m_lastKnownPublished;
		}

	protected:

		friend class sequence_barrier<SEQUENCE, TRAITS>;

		void resume() noexcept
		{
			// This synchronises with the exchange(true, std::memory_order_acquire) in await_suspend().
			if (m_readyToResume.exchange(true, std::memory_order_release))
			{
				resume_impl();
			}
		}

		virtual void resume_impl() noexcept = 0;

		const sequence_barrier<SEQUENCE, TRAITS>& m_barrier;
		const SEQUENCE m_targetSequence;
		SEQUENCE m_lastKnownPublished;
		sequence_barrier_wait_operation_base* m_next;
		cppcoro::coroutine_handle<> m_awaitingCoroutine;
		std::atomic<bool> m_readyToResume;

	};

	template<typename SEQUENCE, typename TRAITS, typename SCHEDULER>
	class sequence_barrier_wait_operation : public sequence_barrier_wait_operation_base<SEQUENCE, TRAITS>
	{
		using schedule_operation = decltype(std::declval<SCHEDULER&>().schedule());

	public:
		sequence_barrier_wait_operation(
			const sequence_barrier<SEQUENCE, TRAITS>& barrier,
			SEQUENCE targetSequence,
			SCHEDULER& scheduler) noexcept
			: sequence_barrier_wait_operation_base<SEQUENCE, TRAITS>(barrier, targetSequence)
			, m_scheduler(scheduler)
		{}

		sequence_barrier_wait_operation(
			const sequence_barrier_wait_operation& other) noexcept
			: sequence_barrier_wait_operation_base<SEQUENCE, TRAITS>(other)
			, m_scheduler(other.m_scheduler)
		{}

		~sequence_barrier_wait_operation()
		{
			if (m_isScheduleAwaiterCreated)
			{
				m_scheduleAwaiter.destruct();
			}
			if (m_isScheduleOperationCreated)
			{
				m_scheduleOperation.destruct();
			}
		}

		decltype(auto) await_resume() noexcept(noexcept(m_scheduleAwaiter->await_resume()))
		{
			if (m_isScheduleAwaiterCreated)
			{
				m_scheduleAwaiter->await_resume();
			}

			return sequence_barrier_wait_operation_base<SEQUENCE, TRAITS>::await_resume();
		}

	private:

		void resume_impl() noexcept override
		{
			try
			{
				m_scheduleOperation.construct(m_scheduler.schedule());
				m_isScheduleOperationCreated = true;

				m_scheduleAwaiter.construct(detail::get_awaiter(
					static_cast<schedule_operation&&>(*m_scheduleOperation)));
				m_isScheduleAwaiterCreated = true;

				if (!m_scheduleAwaiter->await_ready())
				{
					using await_suspend_result_t = decltype(m_scheduleAwaiter->await_suspend(this->m_awaitingCoroutine));
					if constexpr (std::is_void_v<await_suspend_result_t>)
					{
						m_scheduleAwaiter->await_suspend(this->m_awaitingCoroutine);
						return;
					}
					else if constexpr (std::is_same_v<await_suspend_result_t, bool>)
					{
						if (m_scheduleAwaiter->await_suspend(this->m_awaitingCoroutine))
						{
							return;
						}
					}
					else
					{
						// Assume it returns a coroutine_handle.
						m_scheduleAwaiter->await_suspend(this->m_awaitingCoroutine).resume();
						return;
					}
				}
			}
			catch (...)
			{
				// Ignore failure to reschedule and resume inline?
				// Should we catch the exception and rethrow from await_resume()?
				// Or should we require that 'co_await scheduler.schedule()' is noexcept?
			}

			// Resume outside the catch-block.
			this->m_awaitingCoroutine.resume();
		}

		SCHEDULER& m_scheduler;
		// Can't use std::optional<T> here since T could be a reference.
		detail::manual_lifetime<schedule_operation> m_scheduleOperation;
		detail::manual_lifetime<typename awaitable_traits<schedule_operation>::awaiter_t> m_scheduleAwaiter;
		bool m_isScheduleOperationCreated = false;
		bool m_isScheduleAwaiterCreated = false;
	};

	template<typename SEQUENCE, typename TRAITS>
	template<typename SCHEDULER>
	[[nodiscard]]
	sequence_barrier_wait_operation<SEQUENCE, TRAITS, SCHEDULER> sequence_barrier<SEQUENCE, TRAITS>::wait_until_published(
		SEQUENCE targetSequence,
		SCHEDULER& scheduler) const noexcept
	{
		return sequence_barrier_wait_operation<SEQUENCE, TRAITS, SCHEDULER>(*this, targetSequence, scheduler);
	}

	template<typename SEQUENCE, typename TRAITS>
	void sequence_barrier<SEQUENCE, TRAITS>::publish(SEQUENCE sequence) noexcept
	{
		m_lastPublished.store(sequence, std::memory_order_seq_cst);

		// Cheaper check to see if there are any awaiting coroutines.
		auto* awaiters = m_awaiters.load(std::memory_order_seq_cst);
		if (awaiters == nullptr)
		{
			return;
		}

		// Acquire the list of awaiters.
		// Note we may be racing with add_awaiter() which could also acquire the list of waiters
		// so we need to check again whether we won the race and acquired the list.
		awaiters = m_awaiters.exchange(nullptr, std::memory_order_acquire);
		if (awaiters == nullptr)
		{
			return;
		}

		// Check the list of awaiters for ones that are now satisfied by the sequence number
		// we just published. Awaiters are added to either the 'awaitersToResume' list or to
		// the 'awaitersToRequeue' list.
		awaiter_t* awaitersToResume;
		awaiter_t** awaitersToResumeTail = &awaitersToResume;

		awaiter_t* awaitersToRequeue;
		awaiter_t** awaitersToRequeueTail = &awaitersToRequeue;

		do
		{
			if (TRAITS::precedes(sequence, awaiters->m_targetSequence))
			{
				// Target sequence not reached. Append to 'requeue' list.
				*awaitersToRequeueTail = awaiters;
				awaitersToRequeueTail = &awaiters->m_next;
			}
			else
			{
				// Target sequence reached. Append to 'resume' list.
				*awaitersToResumeTail = awaiters;
				awaitersToResumeTail = &awaiters->m_next;
			}
			awaiters = awaiters->m_next;
		} while (awaiters != nullptr);

		// Null-terminate the two lists.
		*awaitersToRequeueTail = nullptr;
		*awaitersToResumeTail = nullptr;

		if (awaitersToRequeue != nullptr)
		{
			awaiter_t* oldHead = nullptr;
			while (!m_awaiters.compare_exchange_weak(
				oldHead,
				awaitersToRequeue,
				std::memory_order_release,
				std::memory_order_relaxed))
			{
				*awaitersToRequeueTail = oldHead;
			}
		}

		while (awaitersToResume != nullptr)
		{
			auto* next = awaitersToResume->m_next;
			awaitersToResume->m_lastKnownPublished = sequence;
			awaitersToResume->resume();
			awaitersToResume = next;
		}
	}

	template<typename SEQUENCE, typename TRAITS>
	void sequence_barrier<SEQUENCE, TRAITS>::add_awaiter(awaiter_t* awaiter) const noexcept
	{
		SEQUENCE targetSequence = awaiter->m_targetSequence;
		awaiter_t* awaitersToRequeue = awaiter;
		awaiter_t** awaitersToRequeueTail = &awaiter->m_next;

		SEQUENCE lastKnownPublished;
		awaiter_t* awaitersToResume;
		awaiter_t** awaitersToResumeTail = &awaitersToResume;

		do
		{
			// Enqueue the awaiter(s)
			{
				auto* oldHead = m_awaiters.load(std::memory_order_relaxed);
				do
				{
					*awaitersToRequeueTail = oldHead;
				} while (!m_awaiters.compare_exchange_weak(
					oldHead,
					awaitersToRequeue,
					std::memory_order_seq_cst,
					std::memory_order_relaxed));
			}

			// Check that the sequence we were waiting for wasn't published while
			// we were enqueueing the waiter.
			// This needs to be seq_cst memory order to ensure that in the case that the producer
			// publishes a new sequence number concurrently with this call that we either see
			// their write to m_lastPublished after enqueueing our awaiter, or they see our
			// write to m_awaiters after their write to m_lastPublished.
			lastKnownPublished = m_lastPublished.load(std::memory_order_seq_cst);
			if (TRAITS::precedes(lastKnownPublished, targetSequence))
			{
				// None of the the awaiters we enqueued have been satisfied yet.
				break;
			}

			// Reset the requeue list to empty
			awaitersToRequeueTail = &awaitersToRequeue;

			// At least one of the awaiters we just enqueued is now satisfied by a concurrently
			// published sequence number. The producer thread may not have seen our write to m_awaiters
			// so we need to try to re-acquire the list of awaiters to ensure that the waiters that
			// are now satisfied are woken up.
			auto* awaiters = m_awaiters.exchange(nullptr, std::memory_order_acquire);

			auto minDiff = std::numeric_limits<typename TRAITS::difference_type>::max();

			while (awaiters != nullptr)
			{
				const auto diff = TRAITS::difference(awaiters->m_targetSequence, lastKnownPublished);
				if (diff > 0)
				{
					*awaitersToRequeueTail = awaiters;
					awaitersToRequeueTail = &awaiters->m_next;
					minDiff = diff < minDiff ? diff : minDiff;
				}
				else
				{
					*awaitersToResumeTail = awaiters;
					awaitersToResumeTail = &awaiters->m_next;
				}

				awaiters = awaiters->m_next;
			}

			// Null-terminate the list of awaiters to requeue.
			*awaitersToRequeueTail = nullptr;

			// Calculate the earliest target sequence required by any of the awaiters to requeue.
			targetSequence = static_cast<SEQUENCE>(lastKnownPublished + minDiff);

		} while (awaitersToRequeue != nullptr);

		// Null-terminate the list of awaiters to resume
		*awaitersToResumeTail = nullptr;

		// Resume the awaiters that are ready
		while (awaitersToResume != nullptr)
		{
			auto* next = awaitersToResume->m_next;
			awaitersToResume->m_lastKnownPublished = lastKnownPublished;
			awaitersToResume->resume();
			awaitersToResume = next;
		}
	}
}

#endif
