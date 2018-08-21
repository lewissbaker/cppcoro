///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_AUTO_RESET_EVENT_HPP_INCLUDED
#define CPPCORO_AUTO_RESET_EVENT_HPP_INCLUDED

#include <cppcoro/config.hpp>

#if CPPCORO_OS_WINNT
# include <cppcoro/detail/win32.hpp>
#else
# include <mutex>
# include <condition_variable>
#endif

namespace cppcoro
{
	class auto_reset_event
	{
	public:

		auto_reset_event(bool initiallySet = false);

		~auto_reset_event();

		void set();

		void wait();

	private:

#if CPPCORO_OS_WINNT
		cppcoro::detail::win32::safe_handle m_event;
#else
		std::mutex m_mutex;
		std::condition_variable m_cv;
		bool m_isSet;
#endif

	};
}

#endif
