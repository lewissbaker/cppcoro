///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/file_write_operation.hpp>
#include <cppcoro/operation_cancelled.hpp>

#include <system_error>
#include <utility>
#include <cassert>

#if CPPCORO_OS_WINNT
# define WIN32_LEAN_AND_MEAN
# include <Windows.h>
#endif

#if CPPCORO_OS_WINNT

cppcoro::file_write_operation::file_write_operation(
	detail::win32::handle_t fileHandle,
	std::uint64_t fileOffset,
	const void* buffer,
	std::size_t byteCount,
	cancellation_token cancellationToken) noexcept
	: m_state(state::not_started)
	, m_fileHandle(fileHandle)
	, m_buffer(buffer)
	, m_byteCount(byteCount)
	, m_cancellationToken(std::move(cancellationToken))
	, m_cancellationRegistration(std::nullopt)
{
	m_ioState.Offset = static_cast<detail::win32::dword_t>(fileOffset);
	m_ioState.OffsetHigh = static_cast<detail::win32::dword_t>(fileOffset >> 32);

	if (m_cancellationToken.is_cancellation_requested())
	{
		m_state.store(state::complete, std::memory_order_relaxed);
		m_numberOfBytesTransferred = 0;
		m_errorCode = ERROR_OPERATION_ABORTED;
	}
}

cppcoro::file_write_operation::file_write_operation(
	file_write_operation&& other) noexcept
	: m_state(state::not_started)
	, m_fileHandle(other.m_fileHandle)
	, m_buffer(other.m_buffer)
	, m_byteCount(other.m_byteCount)
	, m_cancellationToken(std::move(other.m_cancellationToken))
	, m_cancellationRegistration(std::nullopt)
{
	m_ioState.Offset = other.m_ioState.Offset;
	m_ioState.OffsetHigh = other.m_ioState.OffsetHigh;

	if (m_cancellationToken.is_cancellation_requested())
	{
		m_state.store(state::complete, std::memory_order_relaxed);
		m_numberOfBytesTransferred = 0;
		m_errorCode = ERROR_OPERATION_ABORTED;
	}
}

cppcoro::file_write_operation::file_write_operation(
	const file_write_operation& other) noexcept
	: m_state(state::not_started)
	, m_fileHandle(other.m_fileHandle)
	, m_buffer(other.m_buffer)
	, m_byteCount(other.m_byteCount)
	, m_cancellationToken(other.m_cancellationToken)
	, m_cancellationRegistration(std::nullopt)
{
	m_ioState.Offset = other.m_ioState.Offset;
	m_ioState.OffsetHigh = other.m_ioState.OffsetHigh;

	if (m_cancellationToken.is_cancellation_requested())
	{
		m_state.store(state::complete, std::memory_order_relaxed);
		m_numberOfBytesTransferred = 0;
		m_errorCode = ERROR_OPERATION_ABORTED;
	}
}

bool cppcoro::file_write_operation::await_ready() const noexcept
{
	return m_state.load(std::memory_order_acquire) == state::complete;
}

bool cppcoro::file_write_operation::await_suspend(
	std::experimental::coroutine_handle<> awaiter)
{
	m_awaiter = awaiter;

	m_ioState.hEvent = nullptr;
	m_ioState.m_callback = &file_write_operation::on_operation_completed;

	const bool enableCancellation = m_cancellationToken.can_be_cancelled();
	if (enableCancellation)
	{
		// Registering a cancellation callback can throw std::bad_alloc in
		// low-memory situations so we want to do this before we start the
		// I/O operation below as we need it to be noexcept after the
		// operation has started.
		m_cancellationRegistration.emplace(
			std::move(m_cancellationToken),
			[this] { this->on_cancellation_requested(); });
	}
	else
	{
		m_state.store(state::started, std::memory_order_relaxed);
	}

	const DWORD numberOfBytesToWrite =
		m_byteCount <= 0xFFFFFFFF ?
		static_cast<DWORD>(m_byteCount) : DWORD(0xFFFFFFFF);

	BOOL ok = ::WriteFile(
		m_fileHandle,
		m_buffer,
		numberOfBytesToWrite,
		nullptr,
		reinterpret_cast<LPOVERLAPPED>(&m_ioState));
	const DWORD errorCode = ok ? ERROR_SUCCESS : ::GetLastError();
	if (errorCode != ERROR_IO_PENDING)
	{
		// Completed synchronously.
		//
		// We are assuming that the file-handle has been set to the
		// mode where synchronous completions do not post a completion
		// event to the I/O completion port and thus can return without
		// suspending here.

		m_errorCode = errorCode;

		ok = ::GetOverlappedResult(
			m_fileHandle,
			reinterpret_cast<LPOVERLAPPED>(&m_ioState),
			&m_numberOfBytesTransferred,
			FALSE);
		if (!ok)
		{
			m_numberOfBytesTransferred = 0;
		}

		m_state.store(state::complete, std::memory_order_relaxed);

		return false;
	}

	if (enableCancellation)
	{
		// Now that the I/O operation has been started we need to update
		// the state to reflect that. However, we now have the potential
		// for two other threads to be concurrently trying to update the
		// state: a thread requesting cancellation that is trying to
		// transition not_started -> cancellation_requested, and an I/O
		// thread that is handling a completion event that is trying to
		// transition not_started -> complete.
		//
		// We need to use a compare-exchange operation to determine the
		// winner of these potential racing cases.
		// See also on_cancellation_requested() and on_operation_completed().
		state oldState = state::not_started;
		bool succeeded = m_state.compare_exchange_strong(
			oldState,
			state::started,
			std::memory_order_release,
			std::memory_order_acquire);
		if (!succeeded)
		{
			if (oldState == state::cancellation_requested)
			{
				// Another thread requested cancellation and has executed the
				// on_cancellation_requested() callback concurrently and has
				// handed off responsibility for cancelling the operation to us.
				::CancelIoEx(
					m_fileHandle,
					reinterpret_cast<LPOVERLAPPED>(&m_ioState));

				// We might still be racing with an I/O thread that is processing
				// completion of this operation concurrently so we need to use
				// another compare-exchange operation to decide who wins.
				succeeded = m_state.compare_exchange_strong(
					oldState,
					state::started,
					std::memory_order_release,
					std::memory_order_acquire);
				if (succeeded)
				{
					return true;
				}
			}

			assert(oldState == state::complete);

			// The operation completed concurrently on another thread before we could
			// transition to the 'started' state so we'll return as if we completed
			// synchronously.
			return false;
		}
	}

	return true;
}

std::size_t cppcoro::file_write_operation::await_resume()
{
	if (m_errorCode != ERROR_SUCCESS)
	{
		if (m_errorCode == ERROR_OPERATION_ABORTED)
		{
			throw operation_cancelled{};
		}

		throw std::system_error
		{
			static_cast<int>(m_errorCode),
			std::system_category(),
			"file write error"
		};
	}

	return m_numberOfBytesTransferred;
}

void cppcoro::file_write_operation::on_cancellation_requested() noexcept
{
	auto oldState = m_state.load(std::memory_order_acquire);
	if (oldState == state::not_started)
	{
		// This callback is running concurrently with await_suspend().
		// The call to WriteFile() may not have returned yet so we can't call
		// CancelIoEx().
		// Try to notify the await_suspend() thread by transitioning to
		// state::cancellation_requested so that it can call CancelIoEx()
		// once WriteFile() returns.
		const bool transferredCancelResponsibility =
			m_state.compare_exchange_strong(
				oldState,
				state::cancellation_requested,
				std::memory_order_release,
				std::memory_order_acquire);
		if (transferredCancelResponsibility)
		{
			return;
		}
	}

	if (oldState != state::complete)
	{
		::CancelIoEx(
			m_fileHandle,
			reinterpret_cast<LPOVERLAPPED>(&m_ioState));
	}
}

void cppcoro::file_write_operation::on_operation_completed(
	detail::win32::io_state* ioState,
	detail::win32::dword_t errorCode,
	detail::win32::dword_t numberOfBytesTransferred,
	detail::win32::ulongptr_t completionKey) noexcept
{
	auto* operation = reinterpret_cast<file_write_operation*>(
		reinterpret_cast<char*>(ioState) -
		offsetof(file_write_operation, m_ioState));

	operation->m_errorCode = errorCode;
	operation->m_numberOfBytesTransferred = numberOfBytesTransferred;

	auto state = operation->m_state.load(std::memory_order_acquire);
	if (state == state::started)
	{
		operation->m_state.store(state::complete, std::memory_order_release);
		operation->m_awaiter.resume();
	}
	else
	{
		state = operation->m_state.exchange(
			state::complete,
			std::memory_order_acq_rel);
		if (state == state::started)
		{
			operation->m_awaiter.resume();
		}
	}
}

#endif // CPPCORO_OS_WINNT
