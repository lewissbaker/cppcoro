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
# include <cppcoro/detail/win32_overlapped_operation.hpp>
#endif

namespace cppcoro
{
#if CPPCORO_OS_WINNT

	class file_read_operation
		: public cppcoro::detail::win32_overlapped_operation<file_read_operation>
	{
	public:

		file_read_operation(
			detail::win32::handle_t fileHandle,
			std::uint64_t fileOffset,
			void* buffer,
			std::size_t byteCount) noexcept
			: cppcoro::detail::win32_overlapped_operation<file_read_operation>(fileOffset)
			, m_fileHandle(fileHandle)
			, m_buffer(buffer)
			, m_byteCount(byteCount)
		{}

	private:

		friend class cppcoro::detail::win32_overlapped_operation<file_read_operation>;

		bool try_start() noexcept;

		detail::win32::handle_t m_fileHandle;
		void* m_buffer;
		std::size_t m_byteCount;

	};

	class file_read_operation_cancellable
		: public cppcoro::detail::win32_overlapped_operation_cancellable<file_read_operation_cancellable>
	{
	public:

		file_read_operation_cancellable(
			detail::win32::handle_t fileHandle,
			std::uint64_t fileOffset,
			void* buffer,
			std::size_t byteCount,
			cancellation_token&& cancellationToken) noexcept
			: cppcoro::detail::win32_overlapped_operation_cancellable<file_read_operation_cancellable>(
				fileOffset, std::move(cancellationToken))
			, m_fileHandle(fileHandle)
			, m_buffer(buffer)
			, m_byteCount(byteCount)
		{}

	private:

		friend class cppcoro::detail::win32_overlapped_operation_cancellable<file_read_operation_cancellable>;

		bool try_start() noexcept;
		void cancel() noexcept;

		detail::win32::handle_t m_fileHandle;
		void* m_buffer;
		std::size_t m_byteCount;

	};

#endif
}

#endif
