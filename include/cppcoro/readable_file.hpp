///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_READABLE_FILE_HPP_INCLUDED
#define CPPCORO_READABLE_FILE_HPP_INCLUDED

#include <cppcoro/file.hpp>
#include <cppcoro/file_read_operation.hpp>
#include <cppcoro/cancellation_token.hpp>

namespace cppcoro
{
	class readable_file : virtual public file
	{
	public:

		/// Read some data from the file.
		///
		/// Reads \a byteCount bytes from the file starting at \a offset
		/// into the specified \a buffer.
		///
		/// \param offset
		/// The offset within the file to start reading from.
		/// If the file has been opened using file_buffering_mode::unbuffered
		/// then the offset must be a multiple of the file-system's sector size.
		///
		/// \param buffer
		/// The buffer to read the file contents into.
		/// If the file has been opened using file_buffering_mode::unbuffered
		/// then the address of the start of the buffer must be a multiple of
		/// the file-system's sector size.
		///
		/// \param byteCount
		/// The number of bytes to read from the file.
		/// If the file has been opeend using file_buffering_mode::unbuffered
		/// then the byteCount must be a multiple of the file-system's sector size.
		///
		/// \param ct
		/// An optional cancellation_token that can be used to cancel the
		/// read operation before it completes.
		///
		/// \return
		/// An object that represents the read-operation.
		/// This object must be co_await'ed to start the read operation.
		[[nodiscard]]
		file_read_operation read(
			std::uint64_t offset,
			void* buffer,
			std::size_t byteCount) const noexcept;
		[[nodiscard]]
		file_read_operation_cancellable read(
			std::uint64_t offset,
			void* buffer,
			std::size_t byteCount,
			cancellation_token ct) const noexcept;

	protected:

		using file::file;

	};
}

#endif
