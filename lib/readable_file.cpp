///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/readable_file.hpp>

#if CPPCORO_OS_WINNT

cppcoro::file_read_operation cppcoro::readable_file::read(
	std::uint64_t offset,
	void* buffer,
	std::size_t byteCount) const noexcept
{
	return file_read_operation(
		m_fileHandle.handle(),
		offset,
		buffer,
		byteCount);
}

cppcoro::file_read_operation_cancellable cppcoro::readable_file::read(
	std::uint64_t offset,
	void* buffer,
	std::size_t byteCount,
	cancellation_token ct) const noexcept
{
	return file_read_operation_cancellable(
		m_fileHandle.handle(),
		offset,
		buffer,
		byteCount,
		std::move(ct));
}

#endif
