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
#if CPPCORO_COMPILER_MSVC && CPPCORO_COMPILER_MSVC < 19'20'00000
		// HACK: Need to explicitly specify template argument to make_sync_wait_task
		// here to work around a bug in MSVC when passing parameters by universal
		// reference to a coroutine which causes the compiler to think it needs to
		// 'move' parameters passed by rvalue reference.
		auto task = detail::make_sync_wait_task<AWAITABLE>(awaitable);
#else
		auto task = detail::make_sync_wait_task(std::forward<AWAITABLE>(awaitable));
#endif
		detail::lightweight_manual_reset_event event;
		task.start(event);
		event.wait();
		return task.result();
	}
}

#endif
