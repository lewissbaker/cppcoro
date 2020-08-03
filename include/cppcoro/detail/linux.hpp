///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_LINUX_HPP_INCLUDED
#define CPPCORO_DETAIL_LINUX_HPP_INCLUDED

#include <cppcoro/config.hpp>

#include <cstdint>
#include <cstdio>
#include <utility>

#include <fcntl.h>
#include <mqueue.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

#include <liburing.h>

#include <string_view>

namespace cppcoro
{
	namespace detail
	{
		namespace lnx
		{
			using fd_t = int;

			enum message_type
			{
				CALLBACK_TYPE,
				RESUME_TYPE
			};

			void check_required_kernel(int major, int minor, std::string_view message);

			class safe_fd
			{
			public:
				safe_fd()
					: m_fd(-1)
				{
				}

				explicit safe_fd(fd_t fd)
					: m_fd(fd)
				{
				}

				safe_fd(const safe_fd& other) = delete;

				safe_fd(safe_fd&& other) noexcept
					: m_fd(other.m_fd)
				{
					other.m_fd = -1;
				}

				~safe_fd() { close(); }

				safe_fd& operator=(safe_fd fd) noexcept
				{
					swap(fd);
					return *this;
				}

				constexpr fd_t fd() const { return m_fd; }
                constexpr fd_t handle() const { return m_fd; }

				/// Calls close() and sets the fd to -1.
				void close() noexcept;

				void swap(safe_fd& other) noexcept { std::swap(m_fd, other.m_fd); }

				bool operator==(const safe_fd& other) const { return m_fd == other.m_fd; }

				bool operator!=(const safe_fd& other) const { return m_fd != other.m_fd; }

				bool operator==(fd_t fd) const { return m_fd == fd; }

				bool operator!=(fd_t fd) const { return m_fd != fd; }

			private:
				fd_t m_fd;
			};

			struct message
			{
				enum message_type m_type;
				void* m_ptr;
				int m_result;
			};

			struct io_state : lnx::message
			{
				using callback_type = void(io_state* state);
				callback_type* m_callback;
			};

			class uring_queue {
			public:
                explicit uring_queue(size_t queue_length = 32, uint32_t flags = 0);
                ~uring_queue() noexcept;
				bool dequeue(void*& message, message_type& type, bool wait);
				io_uring *handle() { return &ring_; }

			private:
                struct io_uring ring_{};
			};

			safe_fd create_event_fd();
			safe_fd create_timer_fd();
			safe_fd create_epoll_fd();
		}  // namespace linux

		using safe_handle = lnx::safe_fd;
        using dword_t = int;
		struct sock_buf {
            sock_buf(void *buf, size_t sz) : buffer(buf), size(sz) {}
			void * buffer;
			size_t size;
		};
		using handle_t = lnx::fd_t;
    }  // namespace detail
}  // namespace cppcoro

#endif
