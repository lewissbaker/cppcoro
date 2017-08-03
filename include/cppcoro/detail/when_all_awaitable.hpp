///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_WHEN_ALL_AWAITABLE_HPP_INCLUDED
#define CPPCORO_DETAIL_WHEN_ALL_AWAITABLE_HPP_INCLUDED

#include <cppcoro/detail/continuation.hpp>
#include <cppcoro/detail/dummy_coroutine.hpp>

#include <atomic>

namespace cppcoro
{
	namespace detail
	{
		class when_all_awaitable
		{
		public:

			explicit when_all_awaitable(std::size_t count) noexcept
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

			auto await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept
			{
				m_awaiter = awaiter;
				if (m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
				{
					return awaiter;
				}
				else
				{
					return dummy_coroutine::handle();
				}
			}

			void await_resume() noexcept {}

		private:

			static std::experimental::coroutine_handle<> resumer_callback(void* state) noexcept
			{
				auto* that = static_cast<when_all_awaitable*>(state);
				if (that->m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
				{
					return that->m_awaiter;
				}
				else
				{
					return dummy_coroutine::handle();
				}
			}

			std::atomic<std::size_t> m_refCount;
			std::experimental::coroutine_handle<> m_awaiter;
		};
	}
}

#endif
