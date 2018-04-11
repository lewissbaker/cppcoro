#pragma once

#include <mqueue.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <uuid/uuid.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <utility>

namespace cppcoro
{
	namespace detail
	{
		namespace linux
		{
			using fd_t = int;
			
			enum message_type
			{
				CALLBACK_TYPE,
				RESUME_TYPE
			};

						class safe_fd
			{
			public:

				safe_fd()
					: m_fd(-1)
				{}

				explicit safe_fd(fd_t fd)
					: m_fd(fd)
				{}

				safe_fd(const safe_fd& other) = delete;

				safe_fd(safe_fd&& other) noexcept
					: m_fd(other.m_fd)
				{
					other.m_fd = -1;
				}

				~safe_fd()
				{
					close();
				}

				safe_fd& operator=(safe_fd fd) noexcept
				{
					swap(fd);
					return *this;
				}

				constexpr fd_t fd() const { return m_fd; }

				/// Calls close() and sets the fd to -1.
				void close() noexcept;

				void swap(safe_fd& other) noexcept
				{
					std::swap(m_fd, other.m_fd);
				}

				bool operator==(const safe_fd& other) const
				{
					return m_fd == other.m_fd;
				}

				bool operator!=(const safe_fd& other) const
				{
					return m_fd != other.m_fd;
				}

				bool operator==(fd_t fd) const
				{
					return m_fd == fd;
				}

				bool operator!=(fd_t fd) const
				{
					return m_fd != fd;
				}

			private:

				fd_t m_fd;

			};

			struct message
			{
				enum message_type m_type;
				void* m_ptr;
			};

			struct io_state : linux::message
			{
				using callback_type = void(io_state* state);
				callback_type* m_callback;
			};

			class message_queue
			{
			private:
				mqd_t m_mqdt;
				char m_qname[NAME_MAX];
				safe_fd m_epollfd;
				struct epoll_event m_ev;
				message_queue();
			public:
				message_queue(size_t queue_length);
				~message_queue();
				bool enqueue_message(void* message, message_type type);
				bool dequeue_message(void*& message, message_type& type, bool wait);
			};

			safe_fd create_event_fd();
			safe_fd create_timer_fd();
			safe_fd create_epoll_fd();
		}
	}
}
