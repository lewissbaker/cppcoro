///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_STATIC_THREAD_POOL_HPP_INCLUDED
#define CPPCORO_STATIC_THREAD_POOL_HPP_INCLUDED

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>
#include <mutex>
#include <cppcoro/coroutine.hpp>

namespace cppcoro
{
	class static_thread_pool
	{
	public:

		/// Initialise to a number of threads equal to the number of cores
		/// on the current machine.
		static_thread_pool();

		/// Construct a thread pool with the specified number of threads.
		///
		/// \param threadCount
		/// The number of threads in the pool that will be used to execute work.
		explicit static_thread_pool(std::uint32_t threadCount);

		~static_thread_pool();

		class schedule_operation
		{
		public:

			schedule_operation(static_thread_pool* tp) noexcept : m_threadPool(tp) {}

			bool await_ready() noexcept { return false; }
			void await_suspend(cppcoro::coroutine_handle<> awaitingCoroutine) noexcept;
			void await_resume() noexcept {}

		private:

			friend class static_thread_pool;

			static_thread_pool* m_threadPool;
			cppcoro::coroutine_handle<> m_awaitingCoroutine;
			schedule_operation* m_next;

		};

		std::uint32_t thread_count() const noexcept { return m_threadCount; }

		[[nodiscard]]
		schedule_operation schedule() noexcept { return schedule_operation{ this }; }

	private:

		friend class schedule_operation;

		void run_worker_thread(std::uint32_t threadIndex) noexcept;

		void shutdown();

		void schedule_impl(schedule_operation* operation) noexcept;

		void remote_enqueue(schedule_operation* operation) noexcept;

		bool has_any_queued_work_for(std::uint32_t threadIndex) noexcept;

		bool approx_has_any_queued_work_for(std::uint32_t threadIndex) const noexcept;

		bool is_shutdown_requested() const noexcept;

		void notify_intent_to_sleep(std::uint32_t threadIndex) noexcept;
		void try_clear_intent_to_sleep(std::uint32_t threadIndex) noexcept;

		schedule_operation* try_global_dequeue() noexcept;

		/// Try to steal a task from another thread.
		///
		/// \return
		/// A pointer to the operation that was stolen if one could be stolen
		/// from another thread. Otherwise returns nullptr if none of the other
		/// threads had any tasks that could be stolen.
		schedule_operation* try_steal_from_other_thread(std::uint32_t thisThreadIndex) noexcept;

		void wake_one_thread() noexcept;

		class thread_state;

		static thread_local thread_state* s_currentState;
		static thread_local static_thread_pool* s_currentThreadPool;

		const std::uint32_t m_threadCount;
		const std::unique_ptr<thread_state[]> m_threadStates;

		std::vector<std::thread> m_threads;

		std::atomic<bool> m_stopRequested;

		std::mutex m_globalQueueMutex;
		std::atomic<schedule_operation*> m_globalQueueHead;

		//alignas(std::hardware_destructive_interference_size)
		std::atomic<schedule_operation*> m_globalQueueTail;

		//alignas(std::hardware_destructive_interference_size)
		std::atomic<std::uint32_t> m_sleepingThreadCount;

	};
}

#endif
