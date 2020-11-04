///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_ROUND_ROBIN_SCHEDULER_HPP_INCLUDED
#define CPPCORO_ROUND_ROBIN_SCHEDULER_HPP_INCLUDED

#include <cppcoro/config.hpp>

#include <cppcoro/coroutine.hpp>
#include <array>
#include <cassert>
#include <algorithm>
#include <utility>

namespace cppcoro
{
#if CPPCORO_COMPILER_SUPPORTS_SYMMETRIC_TRANSFER
	/// This is a scheduler class that schedules coroutines in a round-robin
	/// fashion once N coroutines have been scheduled to it.
	///
	/// Only supports access from a single thread at a time so
	///
	/// This implementation was inspired by Gor Nishanov's CppCon 2018 talk
	/// about nano-coroutines.
	///
	/// The implementation relies on symmetric transfer and noop_coroutine()
	/// and so only works with a relatively recent version of Clang and does
	/// not yet work with MSVC.
	template<size_t N>
	class round_robin_scheduler
	{
		static_assert(
			N >= 2,
			"Round robin scheduler must be configured to support at least two coroutines");

		class schedule_operation
		{
		public:
			explicit schedule_operation(round_robin_scheduler& s) noexcept : m_scheduler(s) {}

			bool await_ready() noexcept
			{
				return false;
			}

			cppcoro::coroutine_handle<> await_suspend(
				cppcoro::coroutine_handle<> awaitingCoroutine) noexcept
			{
				return m_scheduler.exchange_next(awaitingCoroutine);
			}

			void await_resume() noexcept {}

		private:
			round_robin_scheduler& m_scheduler;
		};

		friend class schedule_operation;

	public:
		round_robin_scheduler() noexcept
			: m_index(0)
			, m_noop(cppcoro::noop_coroutine())
		{
			for (size_t i = 0; i < N - 1; ++i)
			{
				m_coroutines[i] = m_noop();
			}
		}

		~round_robin_scheduler()
		{
			// All tasks should have been joined before calling destructor.
			assert(std::all_of(
				m_coroutines.begin(),
				m_coroutines.end(),
				[&](auto h) { return h == m_noop; }));
		}

		schedule_operation schedule() noexcept
		{
			return schedule_operation{ *this };
		}

		/// Resume any queued coroutines until there are no more coroutines.
		void drain() noexcept
		{
			size_t countRemaining = N - 1;
			do
			{
				auto nextToResume = exchange_next(m_noop);
				if (nextToResume != m_noop)
				{
					nextToResume.resume();
					countRemaining = N - 1;
				}
				else
				{
					--countRemaining;
				}
			} while (countRemaining > 0);
		}

	private:

		cppcoro::coroutine_handle exchange_next(
			cppcoro::coroutine_handle<> coroutine) noexcept
		{
			auto coroutineToResume = std::exchange(
				m_scheduler.m_coroutines[m_scheduler.m_index],
				awaitingCoroutine);
			m_scheduler.m_index = m_scheduler.m_index < (N - 2) ? m_scheduler.m_index + 1 : 0;
			return coroutineToResume;
		}

		size_t m_index;
		const cppcoro::coroutine_handle<> m_noop;
		std::array<cppcoro::coroutine_handle<>, N - 1> m_coroutines;
	};
#endif
}

#endif
