///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_FMAP_HPP_INCLUDED
#define CPPCORO_FMAP_HPP_INCLUDED

#include <cppcoro/awaitable_traits.hpp>
#include <cppcoro/is_awaitable.hpp>

#include <utility>
#include <type_traits>
#include <functional>

namespace cppcoro
{
	namespace detail
	{
		template<typename FUNC, typename AWAITABLE>
		class fmap_awaiter
		{
			using awaiter_t = typename awaitable_traits<AWAITABLE&&>::awaiter_t;
            FUNC&& m_func;
            awaiter_t m_awaiter;

		public:

			fmap_awaiter(FUNC&& func, AWAITABLE&& awaitable)
				noexcept(
					std::is_nothrow_move_constructible_v<awaiter_t> &&
					noexcept(detail::get_awaiter(static_cast<AWAITABLE&&>(awaitable))))
				: m_func(static_cast<FUNC&&>(func))
				, m_awaiter(detail::get_awaiter(static_cast<AWAITABLE&&>(awaitable)))
			{}

			decltype(auto) await_ready()
				noexcept(noexcept(static_cast<awaiter_t&&>(m_awaiter).await_ready()))
			{
				return static_cast<awaiter_t&&>(m_awaiter).await_ready();
			}

			template<typename PROMISE>
			decltype(auto) await_suspend(cppcoro::coroutine_handle<PROMISE> coro)
				noexcept(noexcept(static_cast<awaiter_t&&>(m_awaiter).await_suspend(std::move(coro))))
			{
				return static_cast<awaiter_t&&>(m_awaiter).await_suspend(std::move(coro));
			}

			template<
				typename AWAIT_RESULT = decltype(std::declval<awaiter_t>().await_resume()),
				std::enable_if_t<std::is_void_v<AWAIT_RESULT>, int> = 0>
			decltype(auto) await_resume()
				noexcept(noexcept(std::invoke(static_cast<FUNC&&>(m_func))))
			{
				static_cast<awaiter_t&&>(m_awaiter).await_resume();
				return std::invoke(static_cast<FUNC&&>(m_func));
			}

			template<
				typename AWAIT_RESULT = decltype(std::declval<awaiter_t>().await_resume()),
				std::enable_if_t<!std::is_void_v<AWAIT_RESULT>, int> = 0>
			decltype(auto) await_resume()
				noexcept(noexcept(std::invoke(static_cast<FUNC&&>(m_func), static_cast<awaiter_t&&>(m_awaiter).await_resume())))
			{
				return std::invoke(
					static_cast<FUNC&&>(m_func),
					static_cast<awaiter_t&&>(m_awaiter).await_resume());
			}
		};

		template<typename FUNC, typename AWAITABLE>
		class fmap_awaitable
		{
			static_assert(!std::is_lvalue_reference_v<FUNC>);
			static_assert(!std::is_lvalue_reference_v<AWAITABLE>);
		public:

			template<
				typename FUNC_ARG,
				typename AWAITABLE_ARG,
				std::enable_if_t<
					std::is_constructible_v<FUNC, FUNC_ARG&&> &&
					std::is_constructible_v<AWAITABLE, AWAITABLE_ARG&&>, int> = 0>
			explicit fmap_awaitable(FUNC_ARG&& func, AWAITABLE_ARG&& awaitable)
				noexcept(
					std::is_nothrow_constructible_v<FUNC, FUNC_ARG&&> &&
					std::is_nothrow_constructible_v<AWAITABLE, AWAITABLE_ARG&&>)
				: m_func(static_cast<FUNC_ARG&&>(func))
				, m_awaitable(static_cast<AWAITABLE_ARG&&>(awaitable))
			{}

			auto operator co_await() const &
			{
				return fmap_awaiter<const FUNC&, const AWAITABLE&>(m_func, m_awaitable);
			}

			auto operator co_await() &
			{
				return fmap_awaiter<FUNC&, AWAITABLE&>(m_func, m_awaitable);
			}

			auto operator co_await() &&
			{
				return fmap_awaiter<FUNC&&, AWAITABLE&&>(
					static_cast<FUNC&&>(m_func),
					static_cast<AWAITABLE&&>(m_awaitable));
			}

		private:

			FUNC m_func;
			AWAITABLE m_awaitable;

		};
	}

	template<typename FUNC>
	struct fmap_transform
	{
		explicit fmap_transform(FUNC&& f)
			noexcept(std::is_nothrow_move_constructible_v<FUNC>)
			: func(std::forward<FUNC>(f))
		{}

		FUNC func;
	};

	template<
		typename FUNC,
		typename AWAITABLE,
		std::enable_if_t<cppcoro::is_awaitable_v<AWAITABLE>, int> = 0>
	auto fmap(FUNC&& func, AWAITABLE&& awaitable)
	{
		return detail::fmap_awaitable<
			std::remove_cv_t<std::remove_reference_t<FUNC>>,
			std::remove_cv_t<std::remove_reference_t<AWAITABLE>>>(
			std::forward<FUNC>(func),
			std::forward<AWAITABLE>(awaitable));
	}

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
