///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_LINUX_HPP_INCLUDED
#define CPPCORO_DETAIL_LINUX_HPP_INCLUDED

#include <cppcoro/config.hpp>

#if !CPPCORO_OS_LINUX
# error <cppcoro/detail/linux.hpp> is only supported on the Linux platform.
#endif

#include <utility>
#include <cstdint>

namespace cppcoro
{
	namespace detail
	{
		namespace linux
		{
			struct io_state
			{
				using callback_type = void(
					io_state* state,
					std::int32_t res);

				io_state(callback_type* callback = nullptr) noexcept
					: m_callback(callback)
				{}

				callback_type* m_callback;
			};

			class safe_file_descriptor
			{
			public:
				safe_file_descriptor()
					: m_fd(-1)
				{}

				explicit safe_file_descriptor(int fd)
					: m_fd(fd)
				{}

				safe_file_descriptor(const safe_file_descriptor& other) = delete;

				safe_file_descriptor(safe_file_descriptor&& other) noexcept
					: m_fd(other.m_fd)
				{
					other.m_fd = -1;
				}

				~safe_file_descriptor()
				{
					close();
				}

				safe_file_descriptor& operator=(safe_file_descriptor fd) noexcept
				{
					swap(fd);
					return *this;
				}

				constexpr int get() const { return m_fd; }

				void close() noexcept;

				void swap(safe_file_descriptor& other) noexcept
				{
					std::swap(m_fd, other.m_fd);
				}

				bool operator==(const safe_file_descriptor& other) const
				{
					return m_fd == other.m_fd;
				}

				bool operator!=(const safe_file_descriptor& other) const
				{
					return m_fd != other.m_fd;
				}

				bool operator==(int fd) const
				{
					return m_fd == fd;
				}

				bool operator!=(int fd) const
				{
					return m_fd != fd;
				}

			private:

				int m_fd;

			};
		}
	}
}

#endif
