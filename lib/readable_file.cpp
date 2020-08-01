///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/readable_file.hpp>

cppcoro::file_read_operation cppcoro::readable_file::read(
	std::uint64_t offset,
	void* buffer,
	std::size_t byteCount) const noexcept
{
	return file_read_operation(
#if CPPCORO_OS_LINUX
		*m_ioService,
#endif
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
#if CPPCORO_OS_LINUX
        *m_ioService,
#endif
		m_fileHandle.handle(),
		offset,
		buffer,
		byteCount,
		std::move(ct));
}
