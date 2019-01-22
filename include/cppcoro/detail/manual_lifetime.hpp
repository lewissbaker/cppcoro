///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_MANUAL_LIFETIME_HPP_INCLUDED
#define CPPCORO_DETAIL_MANUAL_LIFETIME_HPP_INCLUDED

#include <type_traits>
#include <memory>

namespace cppcoro::detail
{
	template<typename T>
	struct manual_lifetime
	{
	public:
		manual_lifetime() noexcept {}
		~manual_lifetime() noexcept {}

		manual_lifetime(const manual_lifetime&) = delete;
		manual_lifetime(manual_lifetime&&) = delete;
		manual_lifetime& operator=(const manual_lifetime&) = delete;
		manual_lifetime& operator=(manual_lifetime&&) = delete;

		template<typename... Args>
		std::enable_if_t<std::is_constructible_v<T, Args&&...>> construct(Args&&... args)
			noexcept(std::is_nothrow_constructible_v<T, Args&&...>)
		{
			::new (static_cast<void*>(std::addressof(m_value))) T(static_cast<Args&&>(args)...);
		}

		void destruct() noexcept(std::is_nothrow_destructible_v<T>)
		{
			m_value.~T();
		}

		std::add_pointer_t<T> operator->() noexcept { return std::addressof(**this); }
		std::add_pointer_t<const T> operator->() const noexcept { return std::addressof(**this); }

		T& operator*() & noexcept { return m_value; }
		const T& operator*() const & noexcept { return m_value; }
		T&& operator*() && noexcept { return static_cast<T&&>(m_value); }
		const T&& operator*() const && noexcept { return static_cast<const T&&>(m_value); }

	private:
		union {
			T m_value;
		};
	};

	template<typename T>
	struct manual_lifetime<T&>
	{
	public:
		manual_lifetime() noexcept {}
		~manual_lifetime() noexcept {}

		manual_lifetime(const manual_lifetime&) = delete;
		manual_lifetime(manual_lifetime&&) = delete;
		manual_lifetime& operator=(const manual_lifetime&) = delete;
		manual_lifetime& operator=(manual_lifetime&&) = delete;

		void construct(T& value) noexcept
		{
			m_value = std::addressof(value);
		}

		void destruct() noexcept {}

		T* operator->() noexcept { return m_value; }
		const T* operator->() const noexcept { return m_value; }

		T& operator*() noexcept { return *m_value; }
		const T& operator*() const noexcept { return *m_value; }

	private:
		T* m_value;
	};

	template<typename T>
	struct manual_lifetime<T&&>
	{
	public:
		manual_lifetime() noexcept {}
		~manual_lifetime() noexcept {}

		manual_lifetime(const manual_lifetime&) = delete;
		manual_lifetime(manual_lifetime&&) = delete;
		manual_lifetime& operator=(const manual_lifetime&) = delete;
		manual_lifetime& operator=(manual_lifetime&&) = delete;

		void construct(T&& value) noexcept
		{
			m_value = std::addressof(value);
		}

		void destruct() noexcept {}

		T* operator->() noexcept { return m_value; }
		const T* operator->() const noexcept { return m_value; }

		T& operator*() & noexcept { return *m_value; }
		const T& operator*() const & noexcept { return *m_value; }
		T&& operator*() && noexcept { return static_cast<T&&>(*m_value); }
		const T&& operator*() const && noexcept { return static_cast<const T&&>(*m_value); }

	private:
		T* m_value;
	};

	template<>
	struct manual_lifetime<void>
	{
		void construct() noexcept {}
		void destruct() noexcept {}
		void operator*() const noexcept {}
	};
}

#endif
