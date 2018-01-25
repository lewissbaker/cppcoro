///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_CONSUME_HPP_INCLUDED
#define CPPCORO_CONSUME_HPP_INCLUDED

#include <functional>

namespace cppcoro
{
	template<typename SUBSCRIBABLE, typename CONSUME_FUNC>
	auto consume(SUBSCRIBABLE&& subscribable, CONSUME_FUNC&& func)
	{
		// First subscribe to the subscribable.
		auto[stream, task] = std::forward<SUBSCRIBABLE>(subscribable).subscribe();

		// Pass the stream to the specified consumer function, which is expected to
		// return an awaitable. We'll return a new awaitable that completes when
		// both the producer awaitable and the consumer awaitable complete.
		// But we'll only return the value from the consumer awaitable - do this by
		// applying fmap(std::get<0>) to the result.
		return when_all(std::invoke(func, std::move(stream)), std::move(task))
			| fmap([](auto&& value) { return std::get<0>(value); });
	}
}

#endif
