///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_INLINE_SCHEDULER_HPP_INCLUDED
#define CPPCORO_INLINE_SCHEDULER_HPP_INCLUDED

#include <cppcoro/coroutine.hpp>

namespace cppcoro
{
	class inline_scheduler
	{
	public:

		inline_scheduler() noexcept = default;

		cppcoro::suspend_never schedule() const noexcept
		{
			return {};
		}
	};
}

#endif
