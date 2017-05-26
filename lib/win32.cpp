///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/detail/win32.hpp>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

void cppcoro::detail::win32::safe_handle::close() noexcept
{
	if (m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE)
	{
		::CloseHandle(m_handle);
		m_handle = nullptr;
	}
}
