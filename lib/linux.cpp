#include <cppcoro/detail/linux.hpp>
#include <system_error>
#include <unistd.h>

#define UUID_STRING_SIZE 36

namespace cppcoro
{
	namespace detail
	{
		namespace linux
		{
			message_queue::message_queue(size_t queue_length)
			{
				m_mqdt = -1;
				uuid_t unique_name;
				const char* cppcoro_qname_prefix = "/cppcoro-";

				if(NAME_MAX < UUID_STRING_SIZE + sizeof(cppcoro_qname_prefix) + 1)
				{
					throw std::system_error
					{
						static_cast<int>(EINVAL),
							std::system_category(),
							"Error creating message queue: system name max length too small"
							};
				}

				strncpy(m_qname, cppcoro_qname_prefix, NAME_MAX);

				for(;;)
				{
					uuid_generate(unique_name);
					uuid_unparse(unique_name, m_qname + sizeof(cppcoro_qname_prefix));

					struct mq_attr attr;
					attr.mq_flags = O_NONBLOCK;
					attr.mq_maxmsg = queue_length;
					attr.mq_msgsize = sizeof(cppcoro::detail::linux::message);
					attr.mq_curmsgs = 0;

					m_mqdt = mq_open((const char*)m_qname, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, S_IRWXU, &attr);

					if( m_mqdt == -1 && errno == EEXIST)
					{
						continue;
					}

					if( m_mqdt == -1)
					{
						throw std::system_error
						{
							static_cast<int>(errno),
								std::system_category(),
								"Error creating io_service: message queue open"
								};
					}

					break;
				}

				m_epollfd = create_epoll_fd();
				m_ev.data.fd = m_mqdt;
				m_ev.events = EPOLLIN;

				if(epoll_ctl(m_epollfd, EPOLL_CTL_ADD, m_mqdt, &m_ev) == -1)
				{
					throw std::system_error
					{
						static_cast<int>(errno),
							std::system_category(),
							"Error creating io_service: epoll ctl mqdt"
							};
				}
			}

			message_queue::~message_queue()
			{
				close(m_epollfd);
				assert(mq_close(m_mqdt) == 0);
				assert(mq_unlink(m_qname) == 0);
			}

			bool message_queue::enqueue_message(void* msg, message_type type)
			{
				message qmsg;
				qmsg.m_type = type;
				qmsg.m_ptr = msg;
				int status = mq_send(m_mqdt, (const char*)&qmsg, sizeof(message), 0);
				return status==-1?false:true;
			}

			bool message_queue::dequeue_message(void*& msg, message_type& type, bool wait)
			{
				struct epoll_event ev = {0};
				int nfds = epoll_wait(m_epollfd, &ev, 1, wait?-1:0);

				if(nfds == -1)
				{
					throw std::system_error
					{
						static_cast<int>(errno),
							std::system_category(),
							"Error in epoll_wait run loop"
							};
				}

				if(nfds == 0 && !wait)
				{
					return false;
				}

				if(nfds == 0 && wait)
				{
					throw std::system_error
					{
						static_cast<int>(errno),
							std::system_category(),
							"Error in epoll_wait run loop"
							};
				}

				message qmsg;
				ssize_t status = mq_receive(m_mqdt, (char*)&qmsg, sizeof(message), NULL);

				if(status == -1)
				{
					throw std::system_error
					{
						static_cast<int>(errno),
							std::system_category(),
							"Error retrieving message from message queue: mq_receive"
							};
				}

				msg = qmsg.m_ptr;
				type = qmsg.m_type;
				return true;
			}

			int create_event_fd()
			{
				int fd = eventfd(0, EFD_SEMAPHORE | EFD_NONBLOCK | EFD_CLOEXEC);

				if(fd == -1)
				{
					throw std::system_error
					{
						static_cast<int>(errno),
							std::system_category(),
							"Error creating io_service: event fd create"
							};
				}

				return fd;
			}

			int create_timer_fd()
			{
				int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

				if(fd == -1)
				{
					throw std::system_error
					{
						static_cast<int>(errno),
							std::system_category(),
							"Error creating io_service: timer fd create"
							};
				}

				return fd;
			}

			int create_epoll_fd()
			{
				int fd = epoll_create1(EPOLL_CLOEXEC);

				if(fd == -1)
				{
					throw std::system_error
					{
						static_cast<int>(errno),
							std::system_category(),
							"Error creating timer thread: epoll create"
							};
				}

				return fd;
			}
		}
	}
}
