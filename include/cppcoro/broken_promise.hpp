///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_BROKEN_PROMISE_HPP_INCLUDED
#define CPPCORO_BROKEN_PROMISE_HPP_INCLUDED

#include <stdexcept>

namespace cppcoro
{
	/// \brief
	/// Exception thrown when you attempt to retrieve the result of
	/// a task that has been detached from its promise/coroutine.
	class broken_promise : public std::logic_error
	{
	public:
		broken_promise()
			: std::logic_error("broken promise")
		{}
	};
}

#endif
