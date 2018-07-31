///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include "auto_reset_event.hpp"

#if CPPCORO_OS_WINNT
# define WIN32_LEAN_AND_MEAN
# include <Windows.h>
# include <system_error>
#endif

namespace cppcoro
{
#if CPPCORO_OS_WINNT

	auto_reset_event::auto_reset_event(bool initiallySet)
		: m_event(::CreateEventW(NULL, FALSE, initiallySet ? TRUE : FALSE, NULL))
	{
		if (m_event.handle() == NULL)
		{
			DWORD errorCode = ::GetLastError();
			throw std::system_error
			{
				static_cast<int>(errorCode),
				std::system_category(),
				"auto_reset_event: CreateEvent failed"
			};
		}
	}

	auto_reset_event::~auto_reset_event()
	{
	}

	void auto_reset_event::set()
	{
		BOOL ok =::SetEvent(m_event.handle());
		if (!ok)
		{
			DWORD errorCode = ::GetLastError();
			throw std::system_error
			{
				static_cast<int>(errorCode),
				std::system_category(),
				"auto_reset_event: SetEvent failed"
			};
		}
	}

	void auto_reset_event::wait()
	{
		DWORD result = ::WaitForSingleObjectEx(m_event.handle(), INFINITE, FALSE);
		if (result != WAIT_OBJECT_0)
		{
			DWORD errorCode = ::GetLastError();
			throw std::system_error
			{
				static_cast<int>(errorCode),
				std::system_category(),
				"auto_reset_event: WaitForSingleObjectEx failed"
			};
		}
	}

#else

	auto_reset_event::auto_reset_event(bool initiallySet)
		: m_isSet(initiallySet)
	{}

	auto_reset_event::~auto_reset_event()
	{}

	void auto_reset_event::set()
	{
		std::unique_lock lock{ m_mutex };
		if (!m_isSet)
		{
			m_isSet = true;
			m_cv.notify_one();
		}
	}

	void auto_reset_event::wait()
	{
		std::unique_lock lock{ m_mutex };
		while (!m_isSet)
		{
			m_cv.wait(lock);
		}
		m_isSet = false;
	}

#endif
}
