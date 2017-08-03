///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_CONTINUATION_HPP_INCLUDED
#define CPPCORO_DETAIL_CONTINUATION_HPP_INCLUDED

#include <experimental/coroutine>

namespace cppcoro
{
	namespace detail
	{
		class continuation
		{
		public:

			using callback_t = std::experimental::coroutine_handle<>(void*);

			continuation() noexcept
				: m_callback(nullptr)
				, m_state(nullptr)
			{}

			explicit continuation(std::experimental::coroutine_handle<> awaiter) noexcept
				: m_callback(nullptr)
				, m_state(awaiter.address())
			{}

			explicit continuation(callback_t* callback, void* state) noexcept
				: m_callback(callback)
				, m_state(state)
			{}

			explicit operator bool() const noexcept
			{
				return m_callback != nullptr || m_state != nullptr;
			}

			void resume() noexcept
			{
				tail_call_resume().resume();
			}

			std::experimental::coroutine_handle<> tail_call_resume()
			{
				if (m_callback == nullptr)
				{
					return std::experimental::coroutine_handle<>::from_address(m_state);
				}
				else
				{
					return m_callback(m_state);
				}
			}

		private:

			callback_t* m_callback;
			void* m_state;

		};
	}
}

#endif
