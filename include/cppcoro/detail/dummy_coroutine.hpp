///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_DUMMY_COROUTINE_HPP_INCLUDED
#define CPPCORO_DETAIL_DUMMY_COROUTINE_HPP_INCLUDED

#include <experimental/coroutine>

namespace cppcoro
{
	namespace detail
	{
		class dummy_coroutine final
		{
		public:

			struct promise_type final
			{
				dummy_coroutine get_return_object() noexcept;
				std::experimental::suspend_never initial_suspend() noexcept { return {}; }
				std::experimental::suspend_never final_suspend() noexcept { return {}; }
				void unhandled_exception() noexcept {}
				void return_void() noexcept {}
			};

			static std::experimental::coroutine_handle<> handle() noexcept
			{
				static const auto dummyHandle = coroutine().m_handle;
				return dummyHandle;
			}

		private:


			explicit dummy_coroutine(std::experimental::coroutine_handle<> handle) noexcept
				: m_handle(handle)
			{}

			static dummy_coroutine coroutine()
			{
				for (;;)
				{
					co_await std::experimental::suspend_always{};
				}
			}

			const std::experimental::coroutine_handle<> m_handle;

		};

		inline dummy_coroutine dummy_coroutine::promise_type::get_return_object() noexcept
		{
			return dummy_coroutine{
				std::experimental::coroutine_handle<promise_type>::from_promise(*this)
			};
		}
	}
}

#endif
