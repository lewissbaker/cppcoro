///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include "spin_wait.hpp"

#include <cppcoro/config.hpp>
#include <thread>

#if CPPCORO_OS_WINNT
# define WIN32_LEAN_AND_MEAN
# include <Windows.h>
#endif

namespace
{
	namespace local
	{
		constexpr std::uint32_t yield_threshold = 10;
	}
}

namespace cppcoro
{
	spin_wait::spin_wait() noexcept
	{
		reset();
	}

	bool spin_wait::next_spin_will_yield() const noexcept
	{
		return m_count >= local::yield_threshold;
	}

	void spin_wait::reset() noexcept
	{
		static const std::uint32_t initialCount =
			std::thread::hardware_concurrency() > 1 ? 0 : local::yield_threshold;
		m_count = initialCount;
	}

	void spin_wait::spin_one() noexcept
	{
#if CPPCORO_OS_WINNT
		// Spin strategy taken from .NET System.SpinWait class.
		// I assume the Microsoft developers knew what they're doing.
		if (!next_spin_will_yield())
		{
			// CPU-level pause
			// Allow other hyper-threads to run while we busy-wait.

			// Make each busy-spin exponentially longer
			const std::uint32_t loopCount = 2u << m_count;
			for (std::uint32_t i = 0; i < loopCount; ++i)
			{
				::YieldProcessor();
				::YieldProcessor();
			}
		}
		else
		{
			// We've already spun a number of iterations.
			//
			const auto yieldCount = m_count - local::yield_threshold;
			if (yieldCount % 20 == 19)
			{
				// Yield remainder of time slice to another thread and
				// don't schedule this thread for a little while.
				::SleepEx(1, FALSE);
			}
			else if (yieldCount % 5 == 4)
			{
				// Yield remainder of time slice to another thread
				// that is ready to run (possibly from another processor?).
				::SleepEx(0, FALSE);
			}
			else
			{
				// Yield to another thread that is ready to run on the
				// current processor.
				::SwitchToThread();
			}
		}
#else
		if (next_spin_will_yield())
		{
			std::this_thread::yield();
		}
#endif

		++m_count;
		if (m_count == 0)
		{
			// Don't wrap around to zero as this would go back to
			// busy-waiting.
			m_count = local::yield_threshold;
		}
	}
}

