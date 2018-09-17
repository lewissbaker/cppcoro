///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_FILE_WRITE_OPERATION_HPP_INCLUDED
#define CPPCORO_FILE_WRITE_OPERATION_HPP_INCLUDED

#include <cppcoro/config.hpp>
#include <cppcoro/cancellation_registration.hpp>
#include <cppcoro/cancellation_token.hpp>

#include <atomic>
#include <optional>
#include <experimental/coroutine>

#if CPPCORO_OS_WINNT
# include <cppcoro/detail/win32.hpp>
# include <cppcoro/detail/win32_overlapped_operation.hpp>

namespace cppcoro
{
	class file_write_operation_impl
	{
	public:

		file_write_operation_impl(
			detail::win32::handle_t fileHandle,
			const void* buffer,
			std::size_t byteCount) noexcept
			: m_fileHandle(fileHandle)
			, m_buffer(buffer)
			, m_byteCount(byteCount)
		{}

		bool try_start(cppcoro::detail::win32_overlapped_operation_base& operation) noexcept;
		void cancel(cppcoro::detail::win32_overlapped_operation_base& operation) noexcept;

	private:

		detail::win32::handle_t m_fileHandle;
		const void* m_buffer;
		std::size_t m_byteCount;

	};

	class file_write_operation
		: public cppcoro::detail::win32_overlapped_operation<file_write_operation>
	{
	public:

		file_write_operation(
			detail::win32::handle_t fileHandle,
			std::uint64_t fileOffset,
			const void* buffer,
			std::size_t byteCount) noexcept
			: cppcoro::detail::win32_overlapped_operation<file_write_operation>(fileOffset)
			, m_impl(fileHandle, buffer, byteCount)
		{}

	private:

		friend class cppcoro::detail::win32_overlapped_operation<file_write_operation>;

		bool try_start() noexcept { return m_impl.try_start(*this); }

		file_write_operation_impl m_impl;

	};

	class file_write_operation_cancellable
		: public cppcoro::detail::win32_overlapped_operation_cancellable<file_write_operation_cancellable>
	{
	public:

		file_write_operation_cancellable(
			detail::win32::handle_t fileHandle,
			std::uint64_t fileOffset,
			const void* buffer,
			std::size_t byteCount,
			cancellation_token&& ct) noexcept
			: cppcoro::detail::win32_overlapped_operation_cancellable<file_write_operation_cancellable>(fileOffset, std::move(ct))
			, m_impl(fileHandle, buffer, byteCount)
		{}

	private:

		friend class cppcoro::detail::win32_overlapped_operation_cancellable<file_write_operation_cancellable>;

		bool try_start() noexcept { return m_impl.try_start(*this); }
		void cancel() noexcept { m_impl.cancel(*this); }

		file_write_operation_impl m_impl;

	};
}

#endif // CPPCORO_OS_WINNT

#endif
