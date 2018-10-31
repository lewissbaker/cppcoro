///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_INLINE_SCHEDULER_HPP_INCLUDED
#define CPPCORO_INLINE_SCHEDULER_HPP_INCLUDED

#include <experimental/coroutine>

namespace cppcoro
{
	class inline_scheduler
	{
	public:

		inline_scheduler() noexcept = default;

		std::experimental::suspend_never schedule() const noexcept
		{
			return {};
		}
	};
}

#endif
