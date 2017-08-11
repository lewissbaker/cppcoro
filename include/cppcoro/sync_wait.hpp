///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_SYNC_WAIT_HPP_INCLUDED
#define CPPCORO_SYNC_WAIT_HPP_INCLUDED

#include <cppcoro/detail/lightweight_manual_reset_event.hpp>
#include <cppcoro/detail/continuation.hpp>

#include <cstdint>
#include <atomic>

namespace cppcoro
{
	template<typename TASK>
	decltype(auto) sync_wait(TASK&& task)
	{
		if (!task.is_ready())
		{
			detail::lightweight_manual_reset_event event;

			auto callback = [](void* state)
			{
				static_cast<detail::lightweight_manual_reset_event*>(state)->set();
			};

			auto starter = task.get_starter();
			starter.start(cppcoro::detail::continuation{ callback, &event });

			event.wait();
		}

		return std::forward<TASK>(task).operator co_await().await_resume();
	}
}

#endif
