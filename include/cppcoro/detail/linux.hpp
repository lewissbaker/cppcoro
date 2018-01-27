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

namespace cppcoro
{
  namespace detail
  {
    namespace linux
    {
      enum message_type
	{
	  CALLBACK_TYPE,
	  RESUME_TYPE
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
	int m_epollfd;
	struct epoll_event m_ev;
	message_queue();
      public:
	message_queue(size_t queue_length);
	~message_queue();
	bool enqueue_message(void* message, message_type type);
	bool dequeue_message(void*& message, message_type& type, bool wait);
      };

      int create_event_fd();
      int create_timer_fd();
      int create_epoll_fd();
    }
  }
}
