///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include "cancellation_state.hpp"

#include <cppcoro/cancellation_registration.hpp>

#include <cassert>
#include <cstdlib>

struct cppcoro::detail::cancellation_state::registration_list_chunk
{
	static registration_list_chunk* allocate(std::uint32_t entryCount);
	static void free(registration_list_chunk* chunk) noexcept;

	std::atomic<registration_list_chunk*> m_nextChunk;
	registration_list_chunk* m_prevChunk;
	std::uint32_t m_entryCount;
	std::atomic<cancellation_registration*> m_entries[1];
};

struct cppcoro::detail::cancellation_state::registration_list_bucket
{
	static registration_list_bucket* allocate();
	static void free(registration_list_bucket* bucket) noexcept;

	std::atomic<registration_list_chunk*> m_approximateTail;
	registration_list_chunk m_headChunk;
};

struct cppcoro::detail::cancellation_state::registration_list
{
	static registration_list* allocate();
	static void free(registration_list* list) noexcept;

	void add_registration(cppcoro::cancellation_registration* registration);

	std::thread::id m_notificationThreadId;
	std::uint32_t m_bucketCount;
	std::atomic<registration_list_bucket*> m_buckets[1];
};

cppcoro::detail::cancellation_state::registration_list_chunk*
cppcoro::detail::cancellation_state::registration_list_chunk::allocate(std::uint32_t entryCount)
{
	auto* chunk = static_cast<registration_list_chunk*>(std::malloc(
		sizeof(registration_list_chunk) +
		(entryCount - 1) * sizeof(registration_list_chunk::m_entries[0])));
	if (chunk == nullptr)
	{
		throw std::bad_alloc{};
	}

	chunk->m_nextChunk.store(nullptr, std::memory_order_release);
	chunk->m_prevChunk = nullptr;
	chunk->m_entryCount = entryCount;
	for (std::uint32_t i = 0; i < entryCount; ++i)
	{
		chunk->m_entries[i].store(nullptr, std::memory_order_relaxed);
	}

	return chunk;
}

void cppcoro::detail::cancellation_state::registration_list_chunk::free(registration_list_chunk* bucket) noexcept
{
	std::free(static_cast<void*>(bucket));
}

cppcoro::detail::cancellation_state::registration_list_bucket*
cppcoro::detail::cancellation_state::registration_list_bucket::allocate()
{
	constexpr std::uint32_t initialChunkSize = 16;

	auto* bucket = static_cast<registration_list_bucket*>(std::malloc(
		sizeof(registration_list_bucket) +
		(initialChunkSize - 1) * sizeof(registration_list_chunk::m_entries[0])));
	if (bucket == nullptr)
	{
		throw std::bad_alloc{};
	}

	bucket->m_approximateTail = &bucket->m_headChunk;
	bucket->m_headChunk.m_nextChunk.store(nullptr, std::memory_order_relaxed);
	bucket->m_headChunk.m_prevChunk = nullptr;
	bucket->m_headChunk.m_entryCount = initialChunkSize;
	for (std::uint32_t i = 0; i < initialChunkSize; ++i)
	{
		bucket->m_headChunk.m_entries[i].store(nullptr, std::memory_order_relaxed);
	}

	return bucket;
}

void cppcoro::detail::cancellation_state::registration_list_bucket::free(registration_list_bucket* bucket) noexcept
{
	std::free(static_cast<void*>(bucket));
}

cppcoro::detail::cancellation_state::registration_list*
cppcoro::detail::cancellation_state::registration_list::allocate()
{
	constexpr std::uint32_t maxBucketCount = 16;

	auto bucketCount = std::thread::hardware_concurrency();
	if (bucketCount > maxBucketCount)
	{
		bucketCount = maxBucketCount;
	}
	else if (bucketCount == 0)
	{
		bucketCount = 1;
	}

	auto* list = static_cast<registration_list*>(std::malloc(
		sizeof(registration_list) +
		(bucketCount - 1) * sizeof(registration_list::m_buckets[0])));
	if (list == nullptr)
	{
		throw std::bad_alloc{};
	}

	list->m_bucketCount = bucketCount;
	for (std::uint32_t i = 0; i < bucketCount; ++i)
	{
		list->m_buckets[i].store(nullptr, std::memory_order_relaxed);
	}

	return list;
}

void cppcoro::detail::cancellation_state::registration_list::free(registration_list* list) noexcept
{
	std::free(static_cast<void*>(list));
}

void cppcoro::detail::cancellation_state::registration_list::add_registration(cppcoro::cancellation_registration* registration)
{
	// Pick a bucket to add to based on the current thread to reduce the
	// chance of contention with multiple threads concurrently registering
	// callbacks.
	const auto threadIdHashCode = std::hash<std::thread::id>{}(std::this_thread::get_id());
	auto& bucketPtr = m_buckets[threadIdHashCode % m_bucketCount];

	auto* bucket = bucketPtr.load(std::memory_order_acquire);
	if (bucket == nullptr)
	{
		auto* newBucket = registration_list_bucket::allocate();

		// Pre-claim the first slot.
		auto& slot = newBucket->m_headChunk.m_entries[0];
		slot.store(registration, std::memory_order_relaxed);
		registration->m_registrationSlot = &slot;

		if (bucketPtr.compare_exchange_strong(
			bucket,
			newBucket,
			std::memory_order_seq_cst,
			std::memory_order_acquire))
		{
			return;
		}
		else
		{
			registration_list_bucket::free(newBucket);
		}
	}

	while (true)
	{
		// Navigate to the end of the chain of chunks and work backwards looking for a free slot.
		auto* const originalLastChunk = bucket->m_approximateTail.load(std::memory_order_acquire);

		auto* lastChunk = originalLastChunk;
		for (auto* next = lastChunk->m_nextChunk.load(std::memory_order_acquire);
			next != nullptr;
			next = next->m_nextChunk.load(std::memory_order_acquire))
		{
			lastChunk = next;
		}

		if (lastChunk != originalLastChunk)
		{
			// Update the cache of last chunk pointer so that subsequent
			// registration requests can start there instead.
			// Doesn't matter if these writes race as it will eventually
			// converge to the true last chunk.
			bucket->m_approximateTail.store(lastChunk, std::memory_order_release);
		}

		for (auto* chunk = lastChunk;
			chunk != nullptr;
			chunk = chunk->m_prevChunk)
		{
			for (std::uint32_t i = 0, entryCount = chunk->m_entryCount; i < entryCount; ++i)
			{
				auto& slot = chunk->m_entries[i];

				// Do a cheap initial read of the slot value to see if the
				// slot is likely free. This can potentially read stale values
				// and so may lead to falsely thinking it's free or falsely
				// thinking it's occupied. But approximate is good enough here.
				auto* slotValue = slot.load(std::memory_order_relaxed);
				if (slotValue == nullptr)
				{
					registration->m_registrationSlot = &slot;

					if (slot.compare_exchange_strong(
						slotValue,
						registration,
						std::memory_order_seq_cst,
						std::memory_order_relaxed))
					{
						// Successfully claimed the slot.
						return;
					}
				}
			}
		}

		// We've traversed through all of the chunks and found no free slots.
		// So try and allocate a new chunk and append it to the list.

		constexpr std::uint32_t maxElementCount = 1024;

		const std::uint32_t elementCount =
			lastChunk->m_entryCount < maxElementCount ?
			lastChunk->m_entryCount * 2 : maxElementCount;

		// May throw std::bad_alloc if out of memory.
		auto* newChunk = registration_list_chunk::allocate(elementCount);
		newChunk->m_prevChunk = lastChunk;

		// Pre-allocate first slot.
		auto& slot = newChunk->m_entries[0];
		registration->m_registrationSlot = &slot;
		slot.store(registration, std::memory_order_relaxed);

		registration_list_chunk* oldNext = nullptr;
		if (lastChunk->m_nextChunk.compare_exchange_strong(
			oldNext,
			newChunk,
			std::memory_order_seq_cst,
			std::memory_order_relaxed))
		{
			bucket->m_approximateTail.store(newChunk, std::memory_order_release);
			return;
		}

		// Some other thread published a new chunk to the end of the list
		// concurrently. Free our chunk and go around the loop again, hopefully
		// allocating a slot from the chunk the other thread just allocated.
		registration_list_chunk::free(newChunk);
	}
}

cppcoro::detail::cancellation_state* cppcoro::detail::cancellation_state::create()
{
	return new cancellation_state();
}

cppcoro::detail::cancellation_state::~cancellation_state()
{
	assert((m_state.load(std::memory_order_relaxed) & cancellation_ref_count_mask) == 0);

	// Use relaxed memory order in reads here since we should already have visibility
	// to all writes as the ref-count decrement that preceded the call to the destructor
	// has acquire-release semantics.

	auto* registrations = m_registrations.load(std::memory_order_relaxed);
	if (registrations != nullptr)
	{
		for (std::uint32_t i = 0; i < registrations->m_bucketCount; ++i)
		{
			auto* bucket = registrations->m_buckets[i].load(std::memory_order_relaxed);
			if (bucket != nullptr)
			{
				auto* chunk = bucket->m_headChunk.m_nextChunk.load(std::memory_order_relaxed);
				registration_list_bucket::free(bucket);

				while (chunk != nullptr)
				{
					auto* next = chunk->m_nextChunk.load(std::memory_order_relaxed);
					registration_list_chunk::free(chunk);
					chunk = next;
				}
			}
		}

		registration_list::free(registrations);
	}
}

void cppcoro::detail::cancellation_state::add_token_ref() noexcept
{
	m_state.fetch_add(cancellation_token_ref_increment, std::memory_order_relaxed);
}

void cppcoro::detail::cancellation_state::release_token_ref() noexcept
{
	const std::uint64_t oldState = m_state.fetch_sub(cancellation_token_ref_increment, std::memory_order_acq_rel);
	if ((oldState & cancellation_ref_count_mask) == cancellation_token_ref_increment)
	{
		delete this;
	}
}

void cppcoro::detail::cancellation_state::add_source_ref() noexcept
{
	m_state.fetch_add(cancellation_source_ref_increment, std::memory_order_relaxed);
}

void cppcoro::detail::cancellation_state::release_source_ref() noexcept
{
	const std::uint64_t oldState = m_state.fetch_sub(cancellation_source_ref_increment, std::memory_order_acq_rel);
	if ((oldState & cancellation_ref_count_mask) == cancellation_source_ref_increment)
	{
		delete this;
	}
}

bool cppcoro::detail::cancellation_state::can_be_cancelled() const noexcept
{
	return (m_state.load(std::memory_order_acquire) & can_be_cancelled_mask) != 0;
}

bool cppcoro::detail::cancellation_state::is_cancellation_requested() const noexcept
{
	return (m_state.load(std::memory_order_acquire) & cancellation_requested_flag) != 0;
}

bool cppcoro::detail::cancellation_state::is_cancellation_notification_complete() const noexcept
{
	return (m_state.load(std::memory_order_acquire) & cancellation_notification_complete_flag) != 0;
}

void cppcoro::detail::cancellation_state::request_cancellation()
{
	const auto oldState = m_state.fetch_or(cancellation_requested_flag, std::memory_order_seq_cst);
	if ((oldState & cancellation_requested_flag) != 0)
	{
		// Some thread has already called request_cancellation().
		return;
	}

	// We are the first caller of request_cancellation.
	// Need to execute any registered callbacks to notify them of cancellation.

	// NOTE: We need to use sequentially-consistent operations here to ensure
	// that if there is a concurrent call to try_register_callback() on another
	// thread that either the other thread will read the prior write to m_state
	// after they write to a registration slot or we will read their write to the
	// registration slot after the prior write to m_state.

	auto* const registrations = m_registrations.load(std::memory_order_seq_cst);
	if (registrations != nullptr)
	{
		// Note that there should be no data-race in writing to this value here
		// as another thread will only read it if they are trying to deregister
		// a callback and that fails because we have acquired the pointer to
		// the registration inside the loop below. In this case the atomic
		// exchange that acquires the pointer below acts as a release-operation
		// that synchronises with the failed exchange operation in deregister_callback()
		// which has acquire semantics and thus will have visibility of the write to
		// the m_notificationThreadId value.
		registrations->m_notificationThreadId = std::this_thread::get_id();

		for (std::uint32_t bucketIndex = 0, bucketCount = registrations->m_bucketCount;
			bucketIndex < bucketCount;
			++bucketIndex)
		{
			auto* bucket = registrations->m_buckets[bucketIndex].load(std::memory_order_seq_cst);
			if (bucket == nullptr)
			{
				continue;
			}

			auto* chunk = &bucket->m_headChunk;
			do
			{
				for (std::uint32_t entryIndex = 0, entryCount = chunk->m_entryCount;
					entryIndex < entryCount;
					++entryIndex)
				{
					auto& entry = chunk->m_entries[entryIndex];

					// Quick read-only operation to check if any registration
					// is present.
					auto* registration = entry.load(std::memory_order_seq_cst);
					if (registration != nullptr)
					{
						// Try to acquire ownership of the registration by replacing its
						// slot with nullptr atomically. This resolves the race between
						// a concurrent call to deregister_callback() from the registration's
						// destructor.
						registration = entry.exchange(nullptr, std::memory_order_seq_cst);
						if (registration != nullptr)
						{
							try
							{
								registration->m_callback();
							}
							catch (...)
							{
								// TODO: What should behaviour of unhandled exception in a callback be here?
								std::terminate();
							}
						}
					}
				}

				chunk = chunk->m_nextChunk.load(std::memory_order_seq_cst);
			} while (chunk != nullptr);
		}

		m_state.fetch_add(cancellation_notification_complete_flag, std::memory_order_release);
	}
}

bool cppcoro::detail::cancellation_state::try_register_callback(cancellation_registration* registration)
{
	if (is_cancellation_requested())
	{
		return false;
	}

	auto* registrationList = m_registrations.load(std::memory_order_acquire);
	if (registrationList == nullptr)
	{
		// Could throw std::bad_alloc
		auto* newRegistrationList = registration_list::allocate();

		// Need to use 'sequentially consistent' on the write here to ensure that if
		// we subsequently read a value from m_state at the end of this function that
		// doesn't have the cancellation_requested_flag bit set that a subsequent call
		// in another thread to request_cancellation() will see this write.
		if (m_registrations.compare_exchange_strong(
			registrationList,
			newRegistrationList,
			std::memory_order_seq_cst,
			std::memory_order_acquire))
		{
			registrationList = newRegistrationList;
		}
		else
		{
			registration_list::free(newRegistrationList);
		}
	}

	// Could throw std::bad_alloc
	registrationList->add_registration(registration);

	// Need to check status again to handle the case where
	// another thread calls request_cancellation() concurrently
	// but doesn't see our write to the registration list.
	if ((m_state.load(std::memory_order_seq_cst) & cancellation_requested_flag) != 0)
	{
		// Cancellation was requested concurrently with adding the
		// registration to the list. Try to remove the registration.
		// If successful we return false to indicate that the callback
		// has not been registered and the caller should execute the
		// callback. If it fails it means that the thread that requested
		// cancellation will execute our callback and we need to wait
		// until it finishes before returning.
		auto& slot = *registration->m_registrationSlot;

		// Need to use compare_exchange here rather than just exchange since
		// it may be possible that the thread calling request_cancellation()
		// acquired our registration and executed the callback, freeing up
		// the slot and then a third thread registers a new registration
		// that gets allocated to this slot.
		//
		// Can use relaxed memory order here since in the case that this succeeds
		// no other thread will have written to the cancellation_registration record
		// so we can safely read from the record without synchronisation.
		auto* oldValue = registration;
		const bool deregisteredSuccessfully =
			slot.compare_exchange_strong(oldValue, nullptr, std::memory_order_relaxed);
		if (deregisteredSuccessfully)
		{
			return false;
		}

		// Otherwise, the cancelling thread has taken ownership for executing
		// the callback and we can just act as if the registration succeeded.
	}

	return true;
}

void cppcoro::detail::cancellation_state::deregister_callback(cancellation_registration* registration) noexcept
{
	// Could use 'relaxed' memory order on success case as if this succeeds it means that
	// no thread will have written to the registration object.
	// Use 'acquire' memory order on failure case so that we synchronise with the write
	// to the slot inside request_cancellation() that acquired the registration such that
	// we have visibility of its prior write to m_notifyingThreadId.
	auto* oldValue = registration;
	bool deregisteredSuccessfully = registration->m_registrationSlot->compare_exchange_strong(
		oldValue,
		nullptr,
		std::memory_order_acquire);
	if (!deregisteredSuccessfully)
	{
		// A thread executing request_cancellation() has acquired this callback and
		// is executing it. Need to wait until it finishes executing before we return
		// and the registration object is destructed.
		// However, we also need to handle the case where the registration is being
		// removed from within a callback which would otherwise deadlock waiting
		// for the callbacks to finish executing.

		// Use relaxed memory order here as we should already have visibility
		// of the write to m_registrations from when the registration was first
		// registered.
		auto* registrationList = m_registrations.load(std::memory_order_relaxed);

		if (std::this_thread::get_id() != registrationList->m_notificationThreadId)
		{
			// TODO: More efficient busy-wait backoff strategy
			while (!is_cancellation_notification_complete())
			{
				std::this_thread::yield();
			}
		}
	}
}

cppcoro::detail::cancellation_state::cancellation_state() noexcept
	: m_state(cancellation_source_ref_increment)
	, m_registrations(nullptr)
{
}
