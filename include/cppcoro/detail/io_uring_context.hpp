///////////////////////////////////////////////////////////////////////////////
// Copyright (c) GIG <bigig@live.ru>
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef IO_URING_CONTEXT_HPP_INCLUDED
#define IO_URING_CONTEXT_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/detail/linux.hpp>

#include <mutex>

struct io_uring_sqe;
struct io_uring_cqe;

namespace cppcoro
{
	namespace detail
	{
		namespace linux
		{
			class io_uring_context
			{
			public:

				io_uring_context(std::uint32_t concurrencyHint);
				~io_uring_context();

				bool submit_one(const io_uring_sqe& sqe);
				//bool cancel(std::uint64_t userData);

				bool get_single_event(io_uring_cqe& cqe, bool waitForEvent);

			private:

				safe_file_descriptor m_ringFd;

				std::mutex m_sqMutex;
				std::mutex m_cqMutex;

				struct io_sq_ring
				{
					void* ringPtr;
					std::size_t ringSize;
					unsigned* head;
					unsigned* tail;
					unsigned* ringMask;
					unsigned* ringEntries;
					unsigned* flags;
					unsigned* array;
					io_uring_sqe* sqes;
				} m_sqRing;

				struct io_cq_ring
				{
					void* ringPtr;
					std::size_t ringSize;
					unsigned* head;
					unsigned* tail;
					unsigned* ringMask;
					unsigned* ringEntries;
					io_uring_cqe* cqes;
				} m_cqRing;

			};
		}
	}
}

#endif
