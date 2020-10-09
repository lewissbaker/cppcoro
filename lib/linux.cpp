///////////////////////////////////////////////////////////////////////////////
// Copyright (c) GIG <bigig@live.ru>
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/detail/linux.hpp>

#include <unistd.h>

void cppcoro::detail::linux::safe_file_descriptor::close() noexcept
{
	if (m_fd >= 0)
	{
		::close(m_fd);
		m_fd = -1;
	}
}
