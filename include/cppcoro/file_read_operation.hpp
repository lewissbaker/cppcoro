///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_FILE_READ_OPERATION_HPP_INCLUDED
#define CPPCORO_FILE_READ_OPERATION_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/cancellation_registration.hpp>
#include <cppcoro/cancellation_token.hpp>

#include <atomic>
#include <optional>
#include <experimental/coroutine>

#if CPPCORO_OS_WINNT
# include <cppcoro/detail/win32.hpp>
#endif

namespace cppcoro
{
	class file_read_operation
	{
	public:

#if CPPCORO_OS_WINNT
		file_read_operation(
			detail::win32::handle_t fileHandle,
			std::uint64_t fileOffset,
			void* buffer,
			std::size_t byteCount,
			cancellation_token cancellationToken) noexcept;
#endif

		file_read_operation(file_read_operation&& other) noexcept;
		file_read_operation(const file_read_operation& other) noexcept;

		file_read_operation& operator=(const file_read_operation& other) = delete;
		file_read_operation& operator=(file_read_operation&& other) = delete;

		bool await_ready() const noexcept;
		bool await_suspend(std::experimental::coroutine_handle<> awaiter);
		std::size_t await_resume();

	private:

		void on_cancellation_requested() noexcept;

#if CPPCORO_OS_WINNT
		static void on_operation_completed(
			detail::win32::io_state* ioState,
			detail::win32::dword_t errorCode,
			detail::win32::dword_t numberOfBytesTransferred,
			detail::win32::ulongptr_t completionKey) noexcept;
#endif

		enum class state
		{
			not_started,
			started,
			cancellation_requested,
			complete
		};

		std::atomic<state> m_state;

#if CPPCORO_OS_WINNT
		detail::win32::handle_t m_fileHandle;
		detail::win32::io_state m_ioState;
#endif

		void* m_buffer;
		std::size_t m_byteCount;
		cancellation_token m_cancellationToken;

		std::experimental::coroutine_handle<> m_awaiter;

#if CPPCORO_OS_WINNT
		detail::win32::dword_t m_errorCode;
		detail::win32::dword_t m_numberOfBytesTransferred;
#endif

		std::optional<cancellation_registration> m_cancellationRegistration;

	};
}

#endif
