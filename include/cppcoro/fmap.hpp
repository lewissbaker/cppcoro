///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_FMAP_HPP_INCLUDED
#define CPPCORO_FMAP_HPP_INCLUDED

namespace cppcoro
{
	template<typename FUNC>
	struct fmap_transform
	{
		fmap_transform(FUNC&& f) : func(std::forward<FUNC>(f)) {}

		FUNC func;
	};

	template<typename FUNC>
	auto fmap(FUNC&& func)
	{
		return fmap_transform<FUNC>{ std::forward<FUNC>(func) };
	}
}

#endif
