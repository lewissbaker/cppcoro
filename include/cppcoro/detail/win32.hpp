///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_WIN32_HPP_INCLUDED
#define CPPCORO_DETAIL_WIN32_HPP_INCLUDED

#include <cppcoro/config.hpp>

#if !CPPCORO_OS_WINNT
# error <cppcoro/detail/win32.hpp> is only supported on the Windows platform.
#endif

#include <utility>
#include <cstdint>

struct _OVERLAPPED;

namespace cppcoro
{
	namespace detail
	{
		namespace win32
		{
			using handle_t = void*;
			using ulongptr_t = std::uintptr_t;
			using longptr_t = std::intptr_t;
			using dword_t = unsigned long;
			using socket_t = std::uintptr_t;
			using ulong_t = unsigned long;

#if CPPCORO_COMPILER_MSVC
# pragma warning(push)
# pragma warning(disable : 4201) // Non-standard anonymous struct/union
#endif

			/// Structure needs to correspond exactly to the builtin
			/// _OVERLAPPED structure from Windows.h.
			struct overlapped
			{
				ulongptr_t Internal;
				ulongptr_t InternalHigh;
				union
				{
					struct
					{
						dword_t Offset;
						dword_t OffsetHigh;
					};
					void* Pointer;
				};
				handle_t hEvent;
			};

#if CPPCORO_COMPILER_MSVC
# pragma warning(pop)
#endif

			struct wsabuf
			{
				constexpr wsabuf() noexcept
					: len(0)
					, buf(nullptr)
				{}

				constexpr wsabuf(void* ptr, std::size_t size)
					: len(size <= ulong_t(-1) ? ulong_t(size) : ulong_t(-1))
					, buf(static_cast<char*>(ptr))
				{}

				ulong_t len;
				char* buf;
			};

			struct io_state : win32::overlapped
			{
				using callback_type = void(
					io_state* state,
					win32::dword_t errorCode,
					win32::dword_t numberOfBytesTransferred,
					win32::ulongptr_t completionKey);

				io_state(callback_type* callback = nullptr) noexcept
					: io_state(std::uint64_t(0), callback)
				{}

				io_state(void* pointer, callback_type* callback) noexcept
					: m_callback(callback)
				{
					this->Internal = 0;
					this->InternalHigh = 0;
					this->Pointer = pointer;
					this->hEvent = nullptr;
				}

				io_state(std::uint64_t offset, callback_type* callback) noexcept
					: m_callback(callback)
				{
					this->Internal = 0;
					this->InternalHigh = 0;
					this->Offset = static_cast<dword_t>(offset);
					this->OffsetHigh = static_cast<dword_t>(offset >> 32);
					this->hEvent = nullptr;
				}

				callback_type* m_callback;
			};

			class safe_handle
			{
			public:

				safe_handle()
					: m_handle(nullptr)
				{}

				explicit safe_handle(handle_t handle)
					: m_handle(handle)
				{}

				safe_handle(const safe_handle& other) = delete;

				safe_handle(safe_handle&& other) noexcept
					: m_handle(other.m_handle)
				{
					other.m_handle = nullptr;
				}

				~safe_handle()
				{
					close();
				}

				safe_handle& operator=(safe_handle handle) noexcept
				{
					swap(handle);
					return *this;
				}

				constexpr handle_t handle() const { return m_handle; }

				/// Calls CloseHandle() and sets the handle to NULL.
				void close() noexcept;

				void swap(safe_handle& other) noexcept
				{
					std::swap(m_handle, other.m_handle);
				}

				bool operator==(const safe_handle& other) const
				{
					return m_handle == other.m_handle;
				}

				bool operator!=(const safe_handle& other) const
				{
					return m_handle != other.m_handle;
				}

				bool operator==(handle_t handle) const
				{
					return m_handle == handle;
				}

				bool operator!=(handle_t handle) const
				{
					return m_handle != handle; }

			private:
				handle_t m_handle;
			};
		}  // namespace win32

		using dword_t = win32::dword_t;
		using handle_t = win32::handle_t;
		using sock_buf = win32::wsabuf;
		using safe_handle = win32::safe_handle;
	}  // namespace detail
}

#endif
