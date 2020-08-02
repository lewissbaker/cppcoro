///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Microsoft
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#include <cppcoro/detail/linux.hpp>
#include <system_error>
#include <unistd.h>
#include <cstring>
#include <cassert>

#define UUID_STRING_SIZE 36

namespace cppcoro
{
    namespace detail
    {
        namespace lnx
    {
        message_queue::message_queue(size_t queue_length)
        {
            m_mqdt = -1;
            uuid_t unique_name;
            const char* cppcoro_qname_prefix = "/cppcoro-";

            if(NAME_MAX < UUID_STRING_SIZE + strlen(cppcoro_qname_prefix) + 1)
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
                attr.mq_msgsize = sizeof(cppcoro::detail::lnx::message);
                attr.mq_curmsgs = 0;

                m_mqdt = mq_open(m_qname, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, S_IRWXU, &attr);

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

            m_epollfd = safe_fd{create_epoll_fd()};
            m_ev.data.fd = m_mqdt;
            m_ev.events = EPOLLIN;

            if(epoll_ctl(m_epollfd.fd(), EPOLL_CTL_ADD, m_mqdt, &m_ev) == -1)
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
            int nfds = epoll_wait(m_epollfd.fd(), &ev, 1, wait?-1:0);

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

        uring_queue::uring_queue(size_t queue_length, uint32_t flags)
		{
            auto err = io_uring_queue_init(queue_length, &ring_, flags);
			if (err < 0) {
                throw std::system_error
                    {
                        static_cast<int>(-err),
                        std::system_category(),
                        "Error initializing uring"
                    };
			}
		}

        uring_queue::~uring_queue() noexcept {
            io_uring_queue_exit(&ring_);
        }

        bool uring_queue::dequeue(void*& msg, message_type& type, bool wait) {
            io_uring_cqe *cqe;
            int ret;
			if (wait)
                ret = io_uring_wait_cqe(&ring_, &cqe);
            else ret = io_uring_peek_cqe(&ring_, &cqe);
            if (ret == -EAGAIN) {
				return false;
            } else if (ret < 0) {
                throw std::system_error{-ret,
                                        std::system_category(),
                                        std::string{"io_uring_peek_cqe failed"}};
            } else {
				io_uring_cqe_seen(&ring_, cqe);
				auto res = cqe->res;
                message *msg_ptr = reinterpret_cast<message*>(io_uring_cqe_get_data(cqe));
                msg = msg_ptr->m_ptr;
                type = msg_ptr->m_type;
				msg_ptr->m_result = res;
				return true;  // completed
			}
		}

        safe_fd create_event_fd()
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

            return safe_fd{fd};
        }

        safe_fd create_timer_fd()
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

            return safe_fd{fd};
        }

        safe_fd create_epoll_fd()
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

            return safe_fd{fd};
        }

        void safe_fd::close() noexcept
        {
            if(m_fd != -1)
            {
                ::close(m_fd);
                m_fd = -1;
            }
        }
    }
}
}
