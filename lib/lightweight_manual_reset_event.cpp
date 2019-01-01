///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/detail/lightweight_manual_reset_event.hpp>

#include <system_error>

#if CPPCORO_OS_WINNT
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <Windows.h>

# if CPPCORO_OS_WINNT >= 0x0602

cppcoro::detail::lightweight_manual_reset_event::lightweight_manual_reset_event(bool initiallySet)
	: m_value(initiallySet ? 1 : 0)
{}

cppcoro::detail::lightweight_manual_reset_event::~lightweight_manual_reset_event()
{
}

void cppcoro::detail::lightweight_manual_reset_event::set() noexcept
{
	m_value.store(1, std::memory_order_release);
	::WakeByAddressAll(&m_value);
}

void cppcoro::detail::lightweight_manual_reset_event::reset() noexcept
{
	m_value.store(0, std::memory_order_relaxed);
}

void cppcoro::detail::lightweight_manual_reset_event::wait() noexcept
{
	// Wait in a loop as WaitOnAddress() can have spurious wake-ups.
	int value = m_value.load(std::memory_order_acquire);
	BOOL ok = TRUE;
	while (value == 0)
	{
		if (!ok)
		{
			// Previous call to WaitOnAddress() failed for some reason.
			// Put thread to sleep to avoid sitting in a busy loop if it keeps failing.
			::Sleep(1);
		}

		ok = ::WaitOnAddress(&m_value, &value, sizeof(m_value), INFINITE);
		value = m_value.load(std::memory_order_acquire);
	}
}

# else

cppcoro::detail::lightweight_manual_reset_event::lightweight_manual_reset_event(bool initiallySet)
	: m_eventHandle(::CreateEventW(nullptr, TRUE, initiallySet, nullptr))
{
	if (m_eventHandle == NULL)
	{
		const DWORD errorCode = ::GetLastError();
		throw std::system_error
		{
			static_cast<int>(errorCode),
			std::system_category()
		};
	}
}

cppcoro::detail::lightweight_manual_reset_event::~lightweight_manual_reset_event()
{
	// Ignore failure to close the object.
	// We can't do much here as we want destructor to be noexcept.
	(void)::CloseHandle(m_eventHandle);
}

void cppcoro::detail::lightweight_manual_reset_event::set() noexcept
{
	if (!::SetEvent(m_eventHandle))
	{
		std::abort();
	}
}

void cppcoro::detail::lightweight_manual_reset_event::reset() noexcept
{
	if (!::ResetEvent(m_eventHandle))
	{
		std::abort();
	}
}

void cppcoro::detail::lightweight_manual_reset_event::wait() noexcept
{
	constexpr BOOL alertable = FALSE;
	DWORD waitResult = ::WaitForSingleObjectEx(m_eventHandle, INFINITE, alertable);
	if (waitResult == WAIT_FAILED)
	{
		std::abort();
	}
}

# endif

#elif CPPCORO_OS_LINUX

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <linux/futex.h>
#include <cerrno>
#include <climits>
#include <cassert>

namespace
{
	namespace local
	{
		// No futex() function provided by libc.
		// Wrap the syscall ourselves here.
		int futex(
			int* UserAddress,
			int FutexOperation,
			int Value,
			const struct timespec* timeout,
			int* UserAddress2,
			int Value3)
		{
			return syscall(
				SYS_futex,
				UserAddress,
				FutexOperation,
				Value,
				timeout,
				UserAddress2,
				Value3);
		}
	}
}

cppcoro::detail::lightweight_manual_reset_event::lightweight_manual_reset_event(bool initiallySet)
	: m_value(initiallySet ? 1 : 0)
{}

cppcoro::detail::lightweight_manual_reset_event::~lightweight_manual_reset_event()
{
}

void cppcoro::detail::lightweight_manual_reset_event::set() noexcept
{
	m_value.store(1, std::memory_order_release);

	constexpr int numberOfWaitersToWakeUp = INT_MAX;

	[[maybe_unused]] int numberOfWaitersWokenUp = local::futex(
		reinterpret_cast<int*>(&m_value),
		FUTEX_WAKE_PRIVATE,
		numberOfWaitersToWakeUp,
		nullptr,
		nullptr,
		0);

	// There are no errors expected here unless this class (or the caller)
	// has done something wrong.
	assert(numberOfWaitersWokenUp != -1);
}

void cppcoro::detail::lightweight_manual_reset_event::reset() noexcept
{
	m_value.store(0, std::memory_order_relaxed);
}

void cppcoro::detail::lightweight_manual_reset_event::wait() noexcept
{
	// Wait in a loop as futex() can have spurious wake-ups.
	int oldValue = m_value.load(std::memory_order_acquire);
	while (oldValue == 0)
	{
		int result = local::futex(
			reinterpret_cast<int*>(&m_value),
			FUTEX_WAIT_PRIVATE,
			oldValue,
			nullptr,
			nullptr,
			0);
		if (result == -1)
		{
			if (errno == EAGAIN)
			{
				// The state was changed from zero before we could wait.
				// Must have been changed to 1.
				return;
			}

			// Other errors we'll treat as transient and just read the
			// value and go around the loop again.
		}

		oldValue = m_value.load(std::memory_order_acquire);
	}
}

#else

cppcoro::detail::lightweight_manual_reset_event::lightweight_manual_reset_event(bool initiallySet)
	: m_isSet(initiallySet)
{
}

cppcoro::detail::lightweight_manual_reset_event::~lightweight_manual_reset_event()
{
}

void cppcoro::detail::lightweight_manual_reset_event::set() noexcept
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_isSet = true;
	m_cv.notify_all();
}

void cppcoro::detail::lightweight_manual_reset_event::reset() noexcept
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_isSet = false;
}

void cppcoro::detail::lightweight_manual_reset_event::wait() noexcept
{
	std::unique_lock<std::mutex> lock(m_mutex);
	m_cv.wait(lock, [this] { return m_isSet; });
}

#endif
