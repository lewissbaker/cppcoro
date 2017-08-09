///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/detail/lightweight_manual_reset_event.hpp>

#include <system_error>

#if CPPCORO_OS_WINNT
# define WIN32_LEAN_AND_MEAN
# include <Windows.h>

# if CPPCORO_OS_WINNT >= 0x0602

cppcoro::detail::lightweight_manual_reset_event::lightweight_manual_reset_event(bool initiallySet)
	: m_value(initiallySet ? 1 : 0)
{}

cppcoro::detail::lightweight_manual_reset_event::~lightweight_manual_reset_event()
{
}

void cppcoro::detail::lightweight_manual_reset_event::set()
{
	m_value.store(1, std::memory_order_release);
	::WakeByAddressAll(&m_value);
}

void cppcoro::detail::lightweight_manual_reset_event::reset()
{
	m_value.store(0, std::memory_order_relaxed);
}

void cppcoro::detail::lightweight_manual_reset_event::wait()
{
	// Wait in a loop as WaitOnAddress() can have spurious wake-ups.
	int value = m_value.load(std::memory_order_acquire);
	while (value == 0)
	{
		const BOOL ok = ::WaitOnAddress(&m_value, &value, sizeof(m_value), INFINITE);
		if (!ok)
		{
			const DWORD errorCode = ::GetLastError();
			throw std::system_error
			{
				static_cast<int>(errorCode),
				std::system_category()
			};
		}

		value = m_value.load(std::memory_order_acquire);
	}
}

# else

cppcoro::detail::lightweight_manual_reset_event::lightweight_manual_reset_event(bool initiallySet)
	: m_eventHandle(::CreateEvent(nullptr, TRUE, initiallySet, nullptr))
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

void cppcoro::detail::lightweight_manual_reset_event::set()
{
	const BOOL ok = ::SetEvent(m_eventHandle);
	if (!ok)
	{
		const DWORD errorCode = ::GetLastError();
		throw std::system_error
		{
			static_cast<int>(errorCode),
			std::system_category()
		};
	}
}

void cppcoro::detail::lightweight_manual_reset_event::reset()
{
	const BOOL ok = ::ResetEvent(m_eventHandle);
	if (!ok)
	{
		const DWORD errorCode = ::GetLastError();
		throw std::system_error
		{
			static_cast<int>(errorCode),
			std::system_category()
		};
	}
}

void cppcoro::detail::lightweight_manual_reset_event::wait()
{
	const BOOL alertable = FALSE;
	const DWORD waitResult = ::WaitForSingleObjectEx(m_eventHandle, INFINITE, alertable);
	if (waitResult == WAIT_FAILED)
	{
		const DWORD errorCode = ::GetLastError();
		throw std::system_error
		{
			static_cast<int>(errorCode),
			std::system_category()
		};
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

void cppcoro::detail::lightweight_manual_reset_event::set()
{
	m_value.store(1, std::memory_order_release);

	constexpr int numberOfWaitersToWakeUp = INT_MAX;

	int numberOfWaitersWokenUp = local::futex(
		reinterpret_cast<int*>(&m_value),
		FUTEX_WAKE_PRIVATE,
		numberOfWaitersToWakeUp,
		nullptr,
		nullptr,
		0);
	if (numberOfWaitersWokenUp == -1)
	{
		// There are no errors expected here unless this class (or the caller)
		// has done something wrong.
		throw std::system_error{ errno, std::system_category() };
	}
}

void cppcoro::detail::lightweight_manual_reset_event::reset()
{
	m_value.store(0, std::memory_order_relaxed);
}

void cppcoro::detail::lightweight_manual_reset_event::wait()
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
		if (result == 0)
		{
			// We were blocked and subsequently woken up.
			// This could have been a spurious wake-up so we need to
			// check the value again.
			oldValue = m_value.load(std::memory_order_acquire);
		}
		else
		{
			// An error occurred.
			switch (errno)
			{
			case EAGAIN:
				// The state was changed from zero before we could wait
				// Must have been changed to 1.
				return;

			case EINTR:
				// Wait operation was interrupted by a signal.
				// Go around the loop again.
				break;

			default:
				// Some other, more serious error occurred.
				throw std::system_error{ errno, std::system_category() };
			}
		}
	}
}

#endif
