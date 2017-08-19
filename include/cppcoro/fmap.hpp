///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_FMAP_HPP_INCLUDED
#define CPPCORO_FMAP_HPP_INCLUDED

#include <utility>
#include <type_traits>

namespace cppcoro
{
	template<typename FUNC>
	struct fmap_transform
	{
		explicit fmap_transform(FUNC&& f)
			noexcept(std::is_nothrow_move_constructible_v<FUNC>)
			: func(std::forward<FUNC>(f))
		{}

		FUNC func;
	};

	template<typename FUNC>
	auto fmap(FUNC&& func)
	{
		return fmap_transform<FUNC>{ std::forward<FUNC>(func) };
	}

	template<typename T, typename FUNC>
	decltype(auto) operator|(T&& value, fmap_transform<FUNC>&& transform)
	{
		// Use ADL for finding fmap() overload.
		return fmap(std::forward<FUNC>(transform.func), std::forward<T>(value));
	}

	template<typename T, typename FUNC>
	decltype(auto) operator|(T&& value, const fmap_transform<FUNC>& transform)
	{
		// Use ADL for finding fmap() overload.
		return fmap(transform.func, std::forward<T>(value));
	}

	template<typename T, typename FUNC>
	decltype(auto) operator|(T&& value, fmap_transform<FUNC>& transform)
	{
		// Use ADL for finding fmap() overload.
		return fmap(transform.func, std::forward<T>(value));
	}
}

#endif
