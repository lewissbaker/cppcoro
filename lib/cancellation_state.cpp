///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include "cancellation_state.hpp"

#include "cppcoro/config.hpp"

#include <cppcoro/cancellation_registration.hpp>

#include <cassert>
#include <cstdlib>

namespace cppcoro
{
	namespace detail
	{
		struct cancellation_registration_list_chunk
		{
			static cancellation_registration_list_chunk* allocate(std::uint32_t entryCount);
			static void free(cancellation_registration_list_chunk* chunk) noexcept;

			std::atomic<cancellation_registration_list_chunk*> m_nextChunk;
			cancellation_registration_list_chunk* m_prevChunk;
			std::atomic<std::int32_t> m_approximateFreeCount;
			std::uint32_t m_entryCount;
			std::atomic<cancellation_registration*> m_entries[1];
		};

		struct cancellation_registration_list
		{
			static cancellation_registration_list* allocate();
			static void free(cancellation_registration_list* bucket) noexcept;

			std::atomic<cancellation_registration_list_chunk*> m_approximateTail;
			cancellation_registration_list_chunk m_headChunk;
		};

		struct cancellation_registration_result
		{
			cancellation_registration_result(
				cancellation_registration_list_chunk* chunk,
				std::uint32_t entryIndex)
				: m_chunk(chunk)
				, m_entryIndex(entryIndex)
			{}

			cancellation_registration_list_chunk* m_chunk;
			std::uint32_t m_entryIndex;
		};

		struct cancellation_registration_state
		{
			static cancellation_registration_state* allocate();
			static void free(cancellation_registration_state* list) noexcept;

			cancellation_registration_result add_registration(
				cppcoro::cancellation_registration* registration);

			std::thread::id m_notificationThreadId;

			// Store N separate lists and randomly apportion threads to a given
			// list to reduce chance of contention.
			std::uint32_t m_listCount;
			std::atomic<cancellation_registration_list*> m_lists[1];
		};
	}
}

cppcoro::detail::cancellation_registration_list_chunk*
cppcoro::detail::cancellation_registration_list_chunk::allocate(std::uint32_t entryCount)
{
	auto* chunk = static_cast<cancellation_registration_list_chunk*>(std::malloc(
		sizeof(cancellation_registration_list_chunk) +
		(entryCount - 1) * sizeof(cancellation_registration_list_chunk::m_entries[0])));
	if (chunk == nullptr)
	{
		throw std::bad_alloc{};
	}

	::new (&chunk->m_nextChunk) std::atomic<cancellation_registration_list_chunk*>(nullptr);
	chunk->m_prevChunk = nullptr;
	::new (&chunk->m_approximateFreeCount) std::atomic<int32_t>(static_cast<std::int32_t>(entryCount - 1));
	chunk->m_entryCount = entryCount;
	for (std::uint32_t i = 0; i < entryCount; ++i)
	{
		::new (&chunk->m_entries[i]) std::atomic<cancellation_registration*>(nullptr);
	}

	return chunk;
}

void cppcoro::detail::cancellation_registration_list_chunk::free(
	cancellation_registration_list_chunk* chunk) noexcept
{
	std::free(chunk);
}

cppcoro::detail::cancellation_registration_list*
cppcoro::detail::cancellation_registration_list::allocate()
{
	constexpr std::uint32_t initialChunkSize = 16;

	const std::size_t bufferSize =
		sizeof(cancellation_registration_list) +
		(initialChunkSize - 1) * sizeof(cancellation_registration_list_chunk::m_entries[0]);

	auto* bucket = static_cast<cancellation_registration_list*>(std::malloc(bufferSize));
	if (bucket == nullptr)
	{
		throw std::bad_alloc{};
	}

	::new (&bucket->m_approximateTail) std::atomic<cancellation_registration_list_chunk*>(&bucket->m_headChunk);
	::new (&bucket->m_headChunk.m_nextChunk) std::atomic<cancellation_registration_list_chunk*>(nullptr);
	bucket->m_headChunk.m_prevChunk = nullptr;
	::new (&bucket->m_headChunk.m_approximateFreeCount)
		std::atomic<int32_t>(static_cast<std::int32_t>(initialChunkSize - 1));
	bucket->m_headChunk.m_entryCount = initialChunkSize;
	for (std::uint32_t i = 0; i < initialChunkSize; ++i)
	{
		::new (&bucket->m_headChunk.m_entries[i]) std::atomic<cancellation_registration*>(nullptr);
	}

	return bucket;
}

void cppcoro::detail::cancellation_registration_list::free(cancellation_registration_list* list) noexcept
{
	std::free(list);
}

cppcoro::detail::cancellation_registration_state*
cppcoro::detail::cancellation_registration_state::allocate()
{
	constexpr std::uint32_t maxListCount = 16;

	auto listCount = std::thread::hardware_concurrency();
	if (listCount > maxListCount)
	{
		listCount = maxListCount;
	}
	else if (listCount == 0)
	{
		listCount = 1;
	}

	const std::size_t bufferSize =
		sizeof(cancellation_registration_state) +
		(listCount - 1) * sizeof(cancellation_registration_state::m_lists[0]);

	auto* state = static_cast<cancellation_registration_state*>(std::malloc(bufferSize));
	if (state == nullptr)
	{
		throw std::bad_alloc{};
	}

	state->m_listCount = listCount;
	for (std::uint32_t i = 0; i < listCount; ++i)
	{
		::new (&state->m_lists[i]) std::atomic<cancellation_registration_list*>(nullptr);
	}

	return state;
}

void cppcoro::detail::cancellation_registration_state::free(cancellation_registration_state* state) noexcept
{
	std::free(state);
}

cppcoro::detail::cancellation_registration_result
cppcoro::detail::cancellation_registration_state::add_registration(
	cppcoro::cancellation_registration* registration)
{
	// Pick a list to add to based on the current thread to reduce the
	// chance of contention with multiple threads concurrently registering
	// callbacks.
	const auto threadIdHashCode = std::hash<std::thread::id>{}(std::this_thread::get_id());
	auto& listPtr = m_lists[threadIdHashCode % m_listCount];

	auto* list = listPtr.load(std::memory_order_acquire);
	if (list == nullptr)
	{
		auto* newList = cancellation_registration_list::allocate();

		// Pre-claim the first slot.
		registration->m_chunk = &newList->m_headChunk;
		registration->m_entryIndex = 0;
		::new (&newList->m_headChunk.m_entries[0]) std::atomic<cancellation_registration*>(registration);

		if (listPtr.compare_exchange_strong(
			list,
			newList,
			std::memory_order_seq_cst,
			std::memory_order_acquire))
		{
			return cancellation_registration_result(&newList->m_headChunk, 0);
		}
		else
		{
			cancellation_registration_list::free(newList);
		}
	}

	while (true)
	{
		// Navigate to the end of the chain of chunks and work backwards looking for a free slot.
		auto* const originalLastChunk = list->m_approximateTail.load(std::memory_order_acquire);

		auto* lastChunk = originalLastChunk;
		for (auto* next = lastChunk->m_nextChunk.load(std::memory_order_acquire);
			next != nullptr;
			next = next->m_nextChunk.load(std::memory_order_acquire))
		{
			lastChunk = next;
		}

		// Work around false-warning raised by MSVC static analysis complaining that
		// warning C28182: Dereferencing NULL pointer. 'lastChunk' contains the same NULL value as 'chunk' did.
		// on statement initialising 'elementCount' below.
		CPPCORO_ASSUME(lastChunk != nullptr);

		if (lastChunk != originalLastChunk)
		{
			// Update the cache of last chunk pointer so that subsequent
			// registration requests can start there instead.
			// Doesn't matter if these writes race as it will eventually
			// converge to the true last chunk.
			list->m_approximateTail.store(lastChunk, std::memory_order_release);
		}

		for (auto* chunk = lastChunk;
			chunk != nullptr;
			chunk = chunk->m_prevChunk)
		{
			auto freeCount = chunk->m_approximateFreeCount.load(std::memory_order_relaxed);

			// If it looks like there are no free slots then decrement the count again
			// to force it to re-search every so-often, just in case the count has gotten
			// out-of-sync with the true free count and is reporting none free even though
			// there are some (or possibly all) free slots.
			if (freeCount < 1)
			{
				--freeCount;
				chunk->m_approximateFreeCount.store(freeCount, std::memory_order_relaxed);
			}

			constexpr std::int32_t forcedSearchThreshold = -10;
			if (freeCount > 0 || freeCount < forcedSearchThreshold)
			{
				const std::uint32_t entryCount = chunk->m_entryCount;
				const std::uint32_t indexMask = entryCount - 1;
				const std::uint32_t startIndex = entryCount - freeCount;

				registration->m_chunk = chunk;

				for (std::uint32_t i = 0; i < entryCount; ++i)
				{
					const std::uint32_t entryIndex = (startIndex + i) & indexMask;
					auto& entry = chunk->m_entries[entryIndex];

					// Do a cheap initial read of the entry value to see if the
					// entry is likely free. This can potentially read stale values
					// and so may lead to falsely thinking it's free or falsely
					// thinking it's occupied. But approximate is good enough here.
					auto* entryValue = entry.load(std::memory_order_relaxed);
					if (entryValue == nullptr)
					{
						registration->m_entryIndex = entryIndex;

						if (entry.compare_exchange_strong(
							entryValue,
							registration,
							std::memory_order_seq_cst,
							std::memory_order_relaxed))
						{
							// Successfully claimed the slot.
							const std::int32_t newFreeCount = freeCount < 0 ? 0 : freeCount - 1;
							chunk->m_approximateFreeCount.store(newFreeCount, std::memory_order_relaxed);
							return cancellation_registration_result(chunk, entryIndex);
						}
					}
				}

				// Read through all elements of chunk with no success.
				// Clear free-count back to 0.
				chunk->m_approximateFreeCount.store(0, std::memory_order_relaxed);
			}
		}

		// We've traversed through all of the chunks and found no free slots.
		// So try and allocate a new chunk and append it to the list.

		constexpr std::uint32_t maxElementCount = 1024;

		const std::uint32_t elementCount =
			lastChunk->m_entryCount < maxElementCount ?
			lastChunk->m_entryCount * 2 : maxElementCount;

		// May throw std::bad_alloc if out of memory.
		auto* newChunk = cancellation_registration_list_chunk::allocate(elementCount);
		newChunk->m_prevChunk = lastChunk;

		// Pre-allocate first slot.
		registration->m_chunk = newChunk;
		registration->m_entryIndex = 0;
		::new (&newChunk->m_entries[0]) std::atomic<cancellation_registration*>(registration);

		cancellation_registration_list_chunk* oldNext = nullptr;
		if (lastChunk->m_nextChunk.compare_exchange_strong(
			oldNext,
			newChunk,
			std::memory_order_seq_cst,
			std::memory_order_relaxed))
		{
			list->m_approximateTail.store(newChunk, std::memory_order_release);
			return cancellation_registration_result(newChunk, 0);
		}

		// Some other thread published a new chunk to the end of the list
		// concurrently. Free our chunk and go around the loop again, hopefully
		// allocating a slot from the chunk the other thread just allocated.
		cancellation_registration_list_chunk::free(newChunk);
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

	auto* registrationState = m_registrationState.load(std::memory_order_relaxed);
	if (registrationState != nullptr)
	{
		for (std::uint32_t i = 0; i < registrationState->m_listCount; ++i)
		{
			auto* list = registrationState->m_lists[i].load(std::memory_order_relaxed);
			if (list != nullptr)
			{
				auto* chunk = list->m_headChunk.m_nextChunk.load(std::memory_order_relaxed);
				cancellation_registration_list::free(list);

				while (chunk != nullptr)
				{
					auto* next = chunk->m_nextChunk.load(std::memory_order_relaxed);
					cancellation_registration_list_chunk::free(chunk);
					chunk = next;
				}
			}
		}

		cancellation_registration_state::free(registrationState);
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

	auto* const registrationState = m_registrationState.load(std::memory_order_seq_cst);
	if (registrationState != nullptr)
	{
		// Note that there should be no data-race in writing to this value here
		// as another thread will only read it if they are trying to deregister
		// a callback and that fails because we have acquired the pointer to
		// the registration inside the loop below. In this case the atomic
		// exchange that acquires the pointer below acts as a release-operation
		// that synchronises with the failed exchange operation in deregister_callback()
		// which has acquire semantics and thus will have visibility of the write to
		// the m_notificationThreadId value.
		registrationState->m_notificationThreadId = std::this_thread::get_id();

		for (std::uint32_t listIndex = 0, listCount = registrationState->m_listCount;
			listIndex < listCount;
			++listIndex)
		{
			auto* list = registrationState->m_lists[listIndex].load(std::memory_order_seq_cst);
			if (list == nullptr)
			{
				continue;
			}

			auto* chunk = &list->m_headChunk;
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

bool cppcoro::detail::cancellation_state::try_register_callback(
	cancellation_registration* registration)
{
	if (is_cancellation_requested())
	{
		return false;
	}

	auto* registrationState = m_registrationState.load(std::memory_order_acquire);
	if (registrationState == nullptr)
	{
		// Could throw std::bad_alloc
		auto* newRegistrationState = cancellation_registration_state::allocate();

		// Need to use 'sequentially consistent' on the write here to ensure that if
		// we subsequently read a value from m_state at the end of this function that
		// doesn't have the cancellation_requested_flag bit set that a subsequent call
		// in another thread to request_cancellation() will see this write.
		if (m_registrationState.compare_exchange_strong(
			registrationState,
			newRegistrationState,
			std::memory_order_seq_cst,
			std::memory_order_acquire))
		{
			registrationState = newRegistrationState;
		}
		else
		{
			cancellation_registration_state::free(newRegistrationState);
		}
	}

	// Could throw std::bad_alloc
	auto result = registrationState->add_registration(registration);

	// Need to check status again to handle the case where
	// another thread calls request_cancellation() concurrently
	// but doesn't see our write to the registration list.
	//
	// Note, we don't call IsCancellationRequested() here since that
	// only provides 'acquire' memory semantics and we need 'seq_cst'
	// semantics.
	if ((m_state.load(std::memory_order_seq_cst) & cancellation_requested_flag) != 0)
	{
		// Cancellation was requested concurrently with adding the
		// registration to the list. Try to remove the registration.
		// If successful we return false to indicate that the callback
		// has not been registered and the caller should execute the
		// callback. If it fails it means that the thread that requested
		// cancellation will execute our callback and we need to wait
		// until it finishes before returning.
		auto& entry = result.m_chunk->m_entries[result.m_entryIndex];

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
			entry.compare_exchange_strong(oldValue, nullptr, std::memory_order_relaxed);
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
	auto* chunk = registration->m_chunk;
	auto& entry = chunk->m_entries[registration->m_entryIndex];

	// Use 'acquire' memory order on failure case so that we synchronise with the write
	// to the slot inside request_cancellation() that acquired the registration such that
	// we have visibility of its prior write to m_notifyingThreadId.
	//
	// Could use 'relaxed' memory order on success case as if this succeeds it means that
	// no thread will have written to the registration object.
	auto* oldValue = registration;
	bool deregisteredSuccessfully = entry.compare_exchange_strong(
		oldValue,
		nullptr,
		std::memory_order_acquire);
	if (deregisteredSuccessfully)
	{
		// Increment free-count if it won't make it larger than entry count.
		const std::int32_t oldFreeCount = chunk->m_approximateFreeCount.load(std::memory_order_relaxed);
		if (oldFreeCount < static_cast<std::int32_t>(chunk->m_entryCount))
		{
			const std::int32_t newFreeCount = oldFreeCount < 0 ? 1 : oldFreeCount + 1;
			chunk->m_approximateFreeCount.store(newFreeCount, std::memory_order_relaxed);
		}
	}
	else
	{
		// A thread executing request_cancellation() has acquired this callback and
		// is executing it. Need to wait until it finishes executing before we return
		// and the registration object is destructed.
		//
		// However, we also need to handle the case where the registration is being
		// removed from within a callback which would otherwise deadlock waiting
		// for the callbacks to finish executing.

		// Use relaxed memory order here as we should already have visibility
		// of the write to m_registrationState from when the registration was first
		// registered.
		auto* registrationState = m_registrationState.load(std::memory_order_relaxed);
		if (std::this_thread::get_id() != registrationState->m_notificationThreadId)
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
	, m_registrationState(nullptr)
{
}
