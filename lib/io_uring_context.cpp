///////////////////////////////////////////////////////////////////////////////
// Copyright (c) GIG <bigig@live.ru>
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#include <cppcoro/detail/io_uring_context.hpp>

#include "io_uring.hpp"
#include <sys/mman.h>
#include <atomic>

template<typename T>
static inline void atomic_store_release(T* obj, const T& value)
{
	std::atomic_store_explicit(reinterpret_cast<std::atomic<T>*>(obj),
		value, std::memory_order_release);
}
template<typename T>
static inline T atomic_load_acquire(const T* obj)
{
	return std::atomic_load_explicit(reinterpret_cast<const std::atomic<T>*>(obj),
		std::memory_order_acquire);
}

template<typename T = unsigned>
static inline T* get_var_ptr(void* ptr, unsigned offset)
{
	return reinterpret_cast<T*>(static_cast<char*>(ptr) + offset);
}

cppcoro::detail::linux::io_uring_context::io_uring_context(std::uint32_t concurrencyHint)
{
	(void)concurrencyHint;

	m_sqRing.ringPtr = nullptr;
	m_sqRing.sqes = nullptr;

	io_uring_params params{};
	m_ringFd = safe_file_descriptor{ io_uring_setup(1, &params) };
	if (m_ringFd.get() < 0)
	{
		throw std::system_error
		{
			errno,
			std::system_category(),
			"Error creating io_uring context: io_uring_setup"
		};
	}

	std::size_t sqSize = params.sq_off.array + params.sq_entries * sizeof(unsigned);
	std::size_t cqSize = params.cq_off.cqes + params.cq_entries * sizeof(io_uring_cqe);

	std::size_t ringSize = std::max(sqSize, cqSize);

	void* ringPtr = mmap(0, ringSize, PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_POPULATE, m_ringFd.get(), IORING_OFF_SQ_RING);
	if (ringPtr == MAP_FAILED)
	{
		throw std::system_error
		{
			errno,
			std::system_category(),
			"Error creating io_uring context: mmap"
		};
	}

	// No need to check IORING_FEAT_SINGLE_MMAP because we are anyway going to
	// use features that require kernel >= 5.4.
	m_sqRing.ringPtr = ringPtr;
	m_sqRing.ringSize = ringSize;
	m_cqRing.ringPtr = ringPtr;
	m_cqRing.ringSize = ringSize;

	m_sqRing.head = get_var_ptr(ringPtr, params.sq_off.head);
	m_sqRing.tail = get_var_ptr(ringPtr, params.sq_off.tail);
	m_sqRing.ringMask = get_var_ptr(ringPtr, params.sq_off.ring_mask);
	m_sqRing.ringEntries = get_var_ptr(ringPtr, params.sq_off.ring_entries);
	m_sqRing.flags = get_var_ptr(ringPtr, params.sq_off.flags);
	m_sqRing.array = get_var_ptr(ringPtr, params.sq_off.array);

	m_cqRing.head = get_var_ptr(ringPtr, params.cq_off.head);
	m_cqRing.tail = get_var_ptr(ringPtr, params.cq_off.tail);
	m_cqRing.ringMask = get_var_ptr(ringPtr, params.cq_off.ring_mask);
	m_cqRing.ringEntries = get_var_ptr(ringPtr, params.cq_off.ring_entries);
	m_cqRing.cqes = get_var_ptr<io_uring_cqe>(ringPtr, params.cq_off.cqes);

	void* sqes = mmap(0, params.sq_entries * sizeof(io_uring_sqe),
		PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
		m_ringFd.get(), IORING_OFF_SQES);
	if (sqes == MAP_FAILED)
	{
		throw std::system_error
		{
			errno,
			std::system_category(),
			"Error creating io_uring context: mmap"
		};
	}

	m_sqRing.sqes = static_cast<io_uring_sqe*>(sqes);
}

cppcoro::detail::linux::io_uring_context::~io_uring_context()
{
	if (m_sqRing.sqes != nullptr)
	{
		munmap(m_sqRing.sqes, *m_sqRing.ringEntries * sizeof(io_uring_sqe));
	}
	if (m_sqRing.ringPtr != nullptr)
	{
		munmap(m_sqRing.ringPtr, m_sqRing.ringSize);
	}
}

bool cppcoro::detail::linux::io_uring_context::submit_one(const io_uring_sqe& inSqe)
{
	std::unique_lock lock(m_sqMutex);

	unsigned tail = *m_sqRing.tail;
	unsigned index = tail & *m_sqRing.ringMask;
	m_sqRing.sqes[index] = inSqe;
	m_sqRing.array[index] = index;

	atomic_store_release(m_sqRing.tail, ++tail);

	if (io_uring_enter(m_ringFd.get(), 1, 0, IORING_ENTER_SQ_WAKEUP, nullptr) < 0)
	{
		if (errno == EAGAIN || errno == EBUSY)
		{
			*m_sqRing.tail = --tail;
			return false;
		}

		throw std::system_error
		{
			errno,
			std::system_category(),
			"Error submitting operation: io_uring_enter"
		};
	}

	return true;
}

bool cppcoro::detail::linux::io_uring_context::get_single_event(
	io_uring_cqe& cqe, bool waitForEvent)
{
	std::unique_lock lock(m_cqMutex);

	unsigned head = *m_cqRing.head;
	if (head == atomic_load_acquire(m_cqRing.tail))
	{
		if (!waitForEvent)
			return false;

		if (io_uring_enter(m_ringFd.get(), 0, 1, IORING_ENTER_GETEVENTS, nullptr) < 0)
		{
			throw std::system_error
			{
				errno,
				std::system_category(),
				"Error waiting for event: io_uring_enter"
			};
		}
	}

	unsigned index = head & *m_cqRing.ringMask;
	cqe = m_cqRing.cqes[index];

	atomic_store_release(m_cqRing.head, ++head);

	return true;
}
