///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/io_service.hpp>
#include <cppcoro/on_scope_exit.hpp>

#include <system_error>
#include <cassert>

#if CPPCORO_OS_WINNT
# define WIN32_LEAN_AND_MEAN
# include <Windows.h>
#endif

cppcoro::io_service::io_service()
	: io_service(0)
{
}

cppcoro::io_service::io_service(std::uint32_t concurrencyHint)
	: m_threadState(0)
	, m_workCount(0)
#if CPPCORO_OS_WINNT
	, m_iocpHandle()
#endif
	, m_scheduleOperations(nullptr)
{
	m_iocpHandle = detail::win32::safe_handle(
		::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, concurrencyHint));
	if (m_iocpHandle.handle() == NULL)
	{
		detail::win32::dword_t errorCode = ::GetLastError();
		throw std::system_error
		{
			static_cast<int>(errorCode),
			std::system_category(),
			"Error creating io_service: CreateIoCompletionPort"
		};
	}
}

cppcoro::io_service::~io_service()
{
	assert(m_scheduleOperations.load(std::memory_order_relaxed) == nullptr);
	assert(m_threadState.load(std::memory_order_relaxed) < active_thread_count_increment);
}

cppcoro::io_service::schedule_operation cppcoro::io_service::schedule() noexcept
{
	return schedule_operation{ *this };
}

std::uint64_t cppcoro::io_service::process_events()
{
	std::uint64_t eventCount = 0;
	if (try_enter_event_loop())
	{
		auto exitLoop = on_scope_exit([&] { exit_event_loop(); });

		constexpr bool waitForEvent = true;
		while (try_process_one_event(waitForEvent))
		{
			++eventCount;
		}
	}

	return eventCount;
}

std::uint64_t cppcoro::io_service::process_pending_events()
{
	std::uint64_t eventCount = 0;
	if (try_enter_event_loop())
	{
		auto exitLoop = on_scope_exit([&] { exit_event_loop(); });

		constexpr bool waitForEvent = false;
		while (try_process_one_event(waitForEvent))
		{
			++eventCount;
		}
	}

	return eventCount;
}

std::uint64_t cppcoro::io_service::process_one_event()
{
	std::uint64_t eventCount = 0;
	if (try_enter_event_loop())
	{
		auto exitLoop = on_scope_exit([&] { exit_event_loop(); });

		constexpr bool waitForEvent = true;
		if (try_process_one_event(waitForEvent))
		{
			++eventCount;
		}
	}

	return eventCount;
}

std::uint64_t cppcoro::io_service::process_one_pending_event()
{
	std::uint64_t eventCount = 0;
	if (try_enter_event_loop())
	{
		auto exitLoop = on_scope_exit([&] { exit_event_loop(); });

		constexpr bool waitForEvent = false;
		if (try_process_one_event(waitForEvent))
		{
			++eventCount;
		}
	}

	return eventCount;
}

void cppcoro::io_service::stop() noexcept
{
	const auto oldState = m_threadState.fetch_or(stop_requested_flag, std::memory_order_release);
	if ((oldState & stop_requested_flag) == 0)
	{
		for (auto activeThreadCount = oldState / active_thread_count_increment;
			activeThreadCount > 0;
			--activeThreadCount)
		{
			post_wake_up_event();
		}
	}
}

void cppcoro::io_service::reset()
{
	const auto oldState = m_threadState.fetch_and(~stop_requested_flag, std::memory_order_relaxed);

	// Check that there were no active threads running the event loop.
	assert(oldState == stop_requested_flag);
}

bool cppcoro::io_service::is_stop_requested() const noexcept
{
	return (m_threadState.load(std::memory_order_acquire) & stop_requested_flag) != 0;
}

void cppcoro::io_service::notify_work_started() noexcept
{
	m_workCount.fetch_add(1, std::memory_order_relaxed);
}

void cppcoro::io_service::notify_work_finished() noexcept
{
	if (m_workCount.fetch_sub(1, std::memory_order_relaxed) == 1)
	{
		stop();
	}
}

cppcoro::detail::win32::handle_t cppcoro::io_service::native_iocp_handle() noexcept
{
	return m_iocpHandle.handle();
}

void cppcoro::io_service::schedule_impl(schedule_operation* operation) noexcept
{
#if CPPCORO_OS_WINNT
	const BOOL ok = ::PostQueuedCompletionStatus(
		m_iocpHandle.handle(),
		0,
		reinterpret_cast<ULONG_PTR>(operation->m_awaiter.address()),
		nullptr);
	if (!ok)
	{
		// Failed to post to the I/O completion port.
		//
		// This is most-likely because the queue is currently full.
		//
		// We'll queue up the operation to a linked-list using a lock-free
		// push and defer the dispatch to the completion port until some I/O
		// thread next enters its event loop.
		auto* head = m_scheduleOperations.load(std::memory_order_acquire);
		do
		{
			operation->m_next = head;
		} while (!m_scheduleOperations.compare_exchange_weak(
			head,
			operation,
			std::memory_order_release,
			std::memory_order_acquire));
	}
#endif
}

void cppcoro::io_service::try_reschedule_overflow_operations() noexcept
{
#if CPPCORO_OS_WINNT
	auto* operation = m_scheduleOperations.exchange(nullptr, std::memory_order_acquire);
	while (operation != nullptr)
	{
		auto* next = operation->m_next;
		BOOL ok = ::PostQueuedCompletionStatus(
			m_iocpHandle.handle(),
			0,
			reinterpret_cast<ULONG_PTR>(operation->m_awaiter.address()),
			nullptr);
		if (!ok)
		{
			// Still unable to queue these operations.
			// Put them back on the list of overflow operations.
			auto* tail = operation;
			while (tail->m_next != nullptr)
			{
				tail = tail->m_next;
			}

			schedule_operation* head = nullptr;
			while (!m_scheduleOperations.compare_exchange_weak(
				head,
				operation,
				std::memory_order_release,
				std::memory_order_relaxed))
			{
				tail->m_next = head;
			}

			return;
		}

		operation = next;
	}
#endif
}

bool cppcoro::io_service::try_enter_event_loop() noexcept
{
	auto currentState = m_threadState.load(std::memory_order_relaxed);
	do
	{
		if ((currentState & stop_requested_flag) != 0)
		{
			return false;
		}
	} while (m_threadState.compare_exchange_weak(
		currentState,
		currentState + active_thread_count_increment,
		std::memory_order_relaxed));

	return true;
}

void cppcoro::io_service::exit_event_loop() noexcept
{
	m_threadState.fetch_sub(active_thread_count_increment, std::memory_order_relaxed);
}

bool cppcoro::io_service::try_process_one_event(bool waitForEvent)
{
#if CPPCORO_OS_WINNT
	if (is_stop_requested())
	{
		return false;
	}

	const DWORD timeout = waitForEvent ? INFINITE : 0;

	while (true)
	{
		// Check for any schedule_operation objects that were unable to be
		// queued to the I/O completion port and try to requeue them now.
		try_reschedule_overflow_operations();

		DWORD numberOfBytesTransferred = 0;
		ULONG_PTR completionKey = 0;
		LPOVERLAPPED overlapped = nullptr;
		BOOL ok = ::GetQueuedCompletionStatus(
			m_iocpHandle.handle(),
			&numberOfBytesTransferred,
			&completionKey,
			&overlapped,
			timeout);
		if (overlapped != nullptr)
		{
			DWORD errorCode = ok ? ERROR_SUCCESS : ::GetLastError();

			auto* state = static_cast<detail::win32::io_state*>(
				reinterpret_cast<detail::win32::overlapped*>(overlapped));

			state->m_callback(
				state,
				errorCode,
				numberOfBytesTransferred,
				completionKey);

			return true;
		}
		else if (ok)
		{
			if (completionKey != 0)
			{
				// This was a coroutine scheduled via a call to
				// io_service::schedule().
				std::experimental::coroutine_handle<>::from_address(
					reinterpret_cast<void*>(completionKey)).resume();
				return true;
			}

			// Empty event is a wake-up request, typically associated with a
			// request to exit the event loop.
			// However, there may be spurious such events remaining in the queue
			// from a previous call to stop() that has since been reset() so we
			// need to check whether stop is still required.
			if (is_stop_requested())
			{
				return false;
			}
		}
		else
		{
			const DWORD errorCode = ::GetLastError();
			if (errorCode == WAIT_TIMEOUT)
			{
				return false;
			}

			throw std::system_error
			{
				static_cast<int>(errorCode),
				std::system_category(),
				"Error retrieving item from io_service queue: GetQueuedCompletionStatus"
			};
		}
	}
#endif
}

void cppcoro::io_service::post_wake_up_event() noexcept
{
#if CPPCORO_OS_WINNT
	// We intentionally ignore the return code here.
	//
	// Assume that if posting an event failed that it failed because the queue was full
	// and the system is out of memory. In this case threads should find other events
	// in the queue next time they check anyway and thus wake-up.
	(void)::PostQueuedCompletionStatus(m_iocpHandle.handle(), 0, 0, nullptr);
#endif
}

void cppcoro::io_service::schedule_operation::await_suspend(
	std::experimental::coroutine_handle<> awaiter) noexcept
{
	m_awaiter = awaiter;
	m_service.schedule_impl(this);
}
