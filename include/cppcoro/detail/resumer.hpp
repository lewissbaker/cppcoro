///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_RESUMER_HPP_INCLUDED
#define CPPCORO_DETAIL_RESUMER_HPP_INCLUDED

#include <experimental/coroutine>

namespace cppcoro
{
	namespace detail
	{
		class resumer
		{
		public:

			using callback_t = void(void*);

			resumer() noexcept
				: m_callback(nullptr)
				, m_state(nullptr)
			{}

			explicit resumer(std::experimental::coroutine_handle<> awaiter) noexcept
				: m_callback(nullptr)
				, m_state(awaiter.address())
			{}

			explicit resumer(callback_t* callback, void* state) noexcept
				: m_callback(callback)
				, m_state(state)
			{}

			explicit operator bool() const noexcept
			{
				return m_callback != nullptr || m_state != nullptr;
			}

			void resume() noexcept
			{
				if (m_callback == nullptr)
				{
					std::experimental::coroutine_handle<>::from_address(m_state).resume();
				}
				else
				{
					m_callback(m_state);
				}
			}

		private:

			callback_t* m_callback;
			void* m_state;

		};
	}
}

#endif
