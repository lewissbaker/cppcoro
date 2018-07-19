///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_WHEN_ALL_AWAITABLE_HPP_INCLUDED
#define CPPCORO_DETAIL_WHEN_ALL_AWAITABLE_HPP_INCLUDED

#include <cppcoro/detail/continuation.hpp>

#include <atomic>

namespace cppcoro
{
	namespace detail
	{
		class when_all_awaitable
		{
		public:

			when_all_awaitable(std::size_t count) noexcept
				: m_refCount(count + 1)
			{}

			detail::continuation get_continuation() noexcept
			{
				return detail::continuation{ &when_all_awaitable::resumer_callback, this };
			}

			bool await_ready() noexcept
			{
				return m_refCount.load(std::memory_order_acquire) == 1;
			}

			bool await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept
			{
				m_awaiter = awaiter;
				return m_refCount.fetch_sub(1, std::memory_order_acq_rel) > 1;
			}

			void await_resume() noexcept {}

		protected:
			std::atomic<std::size_t> m_refCount;

		private:
			static void resumer_callback(void* state) noexcept
			{
				auto* that = static_cast<when_all_awaitable*>(state);
				if (that->m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
				{
					that->m_awaiter.resume();
				}
			}

			std::experimental::coroutine_handle<> m_awaiter;
		};

		class when_all_auto_awaitable : public when_all_awaitable
		{
		public:
			when_all_auto_awaitable() noexcept
				: when_all_awaitable(0)
			{}

			detail::continuation get_continuation() noexcept
			{
				m_refCount.fetch_add(1, std::memory_order_relaxed);
				return when_all_awaitable::get_continuation();
			}
		};
	}
}

#endif
