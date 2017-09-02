///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_SYNC_WAIT_HPP_INCLUDED
#define CPPCORO_SYNC_WAIT_HPP_INCLUDED

#include <cppcoro/detail/lightweight_manual_reset_event.hpp>
#include <cppcoro/detail/sync_wait_task.hpp>
#include <cppcoro/awaitable_traits.hpp>

#include <cstdint>
#include <atomic>

namespace cppcoro
{
	template<typename AWAITABLE>
	auto sync_wait(AWAITABLE&& awaitable)
		-> typename cppcoro::awaitable_traits<AWAITABLE&&>::await_result_t
	{
		auto task = detail::make_sync_wait_task<AWAITABLE&&>(awaitable);
		detail::lightweight_manual_reset_event event;
		task.start(event);
		event.wait();
		return task.result();
	}
}

#endif
