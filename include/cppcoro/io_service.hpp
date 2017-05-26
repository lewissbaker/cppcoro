///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_IO_SERVICE_HPP_INCLUDED
#define CPPCORO_IO_SERVICE_HPP_INCLUDED

#include <cppcoro/config.hpp>

#if CPPCORO_OS_WINNT
# include <cppcoro/detail/win32.hpp>
#endif

#include <cstdint>
#include <atomic>
#include <utility>
#include <experimental/coroutine>

namespace cppcoro
{
	class io_context;

	class io_service
	{
	public:

		class schedule_operation;

		io_service();

		io_service(std::uint32_t concurrencyHint);

		~io_service();

		io_service(const io_service& other) = delete;
		io_service& operator=(const io_service& other) = delete;

		/// Returns an operation that when awaited suspends the awaiting
		/// coroutine and reschedules it for resumption on an I/O thread
		/// associated with this io_service.
		schedule_operation schedule() noexcept;

		/// Process events until the io_service is stopped.
		///
		/// \return
		/// The number of events processed during this call.
		std::uint64_t process_events();

		/// Process events until either the io_service is stopped or
		/// there are no more pending events in the queue.
		///
		/// \return
		/// The number of events processed during this call.
		std::uint64_t process_pending_events();

		/// Block until either one event is processed or the io_service is stopped.
		///
		/// \return
		/// The number of events processed during this call.
		/// This will either be 0 or 1.
		std::uint64_t process_one_event();

		/// Process one event if there are any events pending, otherwise if there
		/// are no events pending or the io_service is stopped then return immediately.
		///
		/// \return
		/// The number of events processed during this call.
		/// This will either be 0 or 1.
		std::uint64_t process_one_pending_event();

		/// Shut down the io_service.
		///
		/// This will cause any threads currently in a call to one of the process_xxx() methods
		/// to return from that call once they finish processing the current event.
		///
		/// This call does not wait until all threads have exited the event loop so you
		/// must use other synchronisation mechanisms to wait for those threads.
		void stop() noexcept;

		/// Reset an io_service to prepare it for resuming processing of events.
		///
		/// Call this after a call to stop() to allow calls to process_xxx() methods
		/// to process events.
		///
		/// After calling stop() you must ensure that all threads have returned from
		/// calls to process_xxx() methods before calling reset().
		void reset();

		bool is_stop_requested() const noexcept;

		void notify_work_started() noexcept;

		void notify_work_finished() noexcept;

		io_context get_context() noexcept;

#if CPPCORO_OS_WINNT
		detail::win32::handle_t native_iocp_handle() noexcept;
#endif

	private:

		friend class schedule_operation;

		void schedule_impl(schedule_operation* operation);

		bool try_enter_event_loop() noexcept;
		void exit_event_loop() noexcept;

		bool try_process_one_event(bool waitForEvent);

		void post_wake_up_event() noexcept;

		static constexpr std::uint32_t stop_requested_flag = 1;
		static constexpr std::uint32_t active_thread_count_increment = 2;

		// Bit 0: stop_requested_flag
		// Bit 1-31: count of active threads currently running the event loop
		std::atomic<std::uint32_t> m_threadState;

		std::atomic<std::uint32_t> m_workCount;

#if CPPCORO_OS_WINNT
		detail::win32::safe_handle m_iocpHandle;
#endif

	};

	class io_service::schedule_operation
	{
	public:

		schedule_operation(io_service& service) noexcept
			: m_service(service)
		{}

		bool await_ready() const noexcept { return false; }

		void await_suspend(std::experimental::coroutine_handle<> awaiter);

		void await_resume() const noexcept {}

	private:

		friend class io_service;

		io_service& m_service;
		std::experimental::coroutine_handle<> m_awaiter;
	};

	class io_context
	{
	public:

		io_context(io_service& service) noexcept
			: m_service(&service)
		{
			service.notify_work_started();
		}

		io_context(const io_context& other) noexcept
			: m_service(other.m_service)
		{
			if (m_service != nullptr)
			{
				m_service->notify_work_started();
			}
		}

		io_context(io_context&& other) noexcept
			: m_service(other.m_service)
		{
			other.m_service = nullptr;
		}

		~io_context()
		{
			if (m_service != nullptr)
			{
				m_service->notify_work_finished();
			}
		}

		io_service::schedule_operation schedule() noexcept
		{
			return m_service->schedule();
		}

		void swap(io_context& other) noexcept
		{
			std::swap(m_service, other.m_service);
		}

		io_context& operator=(io_context other) noexcept
		{
			swap(other);
		}

#if CPPCORO_OS_WINNT
		detail::win32::handle_t native_iocp_handle() noexcept
		{
			return m_service->native_iocp_handle();
		}
#endif

	private:

		io_service* m_service;

	};

	inline void swap(io_context& a, io_context& b)
	{
		a.swap(b);
	}
}

#endif
