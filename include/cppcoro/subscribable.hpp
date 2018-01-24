///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_SUBSCRIBABLE_HPP_INCLUDED
#define CPPCORO_SUBSCRIBABLE_HPP_INCLUDED

#include <functional>

namespace cppcoro
{
	/// Helper class that maps .subscribe() to an invokable object.
	template<typename SUBSCRIBE_FUNC>
	class subscribable
	{
	public:

		subscribable(SUBSCRIBE_FUNC&& subscribe)
			: m_subscribe(subscribe)
		{}

		decltype(auto) subscribe() const & noexcept(noexcept(std::invoke(m_subscribe)))
		{
			return std::invoke(m_subscribe);
		}

		decltype(auto) subscribe() & noexcept(noexcept(std::invoke(m_subscribe)))
		{
			return std::invoke(m_subscribe);
		}

		decltype(auto) subscribe() && noexcept(noexcept(std::invoke(std::move(m_subscribe))))
		{
			return std::invoke(std::move(m_subscribe));
		}

	private:

		SUBSCRIBE_FUNC m_subscribe;

	};

	template<typename SUBSCRIBE_FUNC>
	auto make_subscribable(SUBSCRIBE_FUNC&& subscribeFunc)
	{
		return subscribable<std::remove_reference_t<SUBSCRIBE_FUNC>>{ std::forward<SUBSCRIBE_FUNC>(subscribeFunc) };
	}
}

#endif
