///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_WRITABLE_FILE_HPP_INCLUDED
#define CPPCORO_WRITABLE_FILE_HPP_INCLUDED

#include <cppcoro/file.hpp>
#include <cppcoro/file_write_operation.hpp>
#include <cppcoro/cancellation_token.hpp>

namespace cppcoro
{
	class writable_file : virtual public file
	{
	public:

		/// Set the size of the file.
		///
		/// \param fileSize
		/// The new size of the file in bytes.
		void set_size(std::uint64_t fileSize);

		/// Write some data to the file.
		///
		/// Writes \a byteCount bytes from the file starting at \a offset
		/// into the specified \a buffer.
		///
		/// \param offset
		/// The offset within the file to start writing from.
		/// If the file has been opened using file_buffering_mode::unbuffered
		/// then the offset must be a multiple of the file-system's sector size.
		///
		/// \param buffer
		/// The buffer containing the data to be written to the file.
		/// If the file has been opened using file_buffering_mode::unbuffered
		/// then the address of the start of the buffer must be a multiple of
		/// the file-system's sector size.
		///
		/// \param byteCount
		/// The number of bytes to write to the file.
		/// If the file has been opeend using file_buffering_mode::unbuffered
		/// then the byteCount must be a multiple of the file-system's sector size.
		///
		/// \param ct
		/// An optional cancellation_token that can be used to cancel the
		/// write operation before it completes.
		///
		/// \return
		/// An object that represents the write operation.
		/// This object must be co_await'ed to start the write operation.
		[[nodiscard]]
		file_write_operation write(
			std::uint64_t offset,
			const void* buffer,
			std::size_t byteCount) noexcept;
		[[nodiscard]]
		file_write_operation_cancellable write(
			std::uint64_t offset,
			const void* buffer,
			std::size_t byteCount,
			cancellation_token ct) noexcept;

	protected:

		using file::file;

	};
}

#endif
