///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_SEQUENCE_RANGE_HPP_INCLUDED
#define CPPCORO_SEQUENCE_RANGE_HPP_INCLUDED

#include <cppcoro/sequence_traits.hpp>

#include <algorithm>
#include <iterator>

namespace cppcoro
{
	template<typename SEQUENCE, typename TRAITS = sequence_traits<SEQUENCE>>
	class sequence_range
	{
	public:

		using value_type = SEQUENCE;
		using difference_type = typename TRAITS::difference_type;
		using size_type = typename TRAITS::size_type;

		class const_iterator
		{
		public:

			using iterator_category = std::random_access_iterator_tag;
			using value_type = SEQUENCE;
			using difference_type = typename TRAITS::difference_type;
			using reference = const SEQUENCE&;
			using pointer = const SEQUENCE*;

			explicit constexpr const_iterator(SEQUENCE value) noexcept : m_value(value) {}

			const SEQUENCE& operator*() const noexcept { return m_value; }
			const SEQUENCE* operator->() const noexcept { return std::addressof(m_value); }

			const_iterator& operator++() noexcept { ++m_value; return *this; }
			const_iterator& operator--() noexcept { --m_value; return *this; }

			const_iterator operator++(int) noexcept { return const_iterator(m_value++); }
			const_iterator operator--(int) noexcept { return const_iterator(m_value--); }

			constexpr difference_type operator-(const_iterator other) const noexcept { return TRAITS::difference(m_value, other.m_value); }
			constexpr const_iterator operator-(difference_type delta) const noexcept { return const_iterator{ static_cast<SEQUENCE>(m_value - delta) }; }
			constexpr const_iterator operator+(difference_type delta) const noexcept { return const_iterator{ static_cast<SEQUENCE>(m_value + delta) }; }

			constexpr bool operator==(const_iterator other) const noexcept { return m_value == other.m_value; }
			constexpr bool operator!=(const_iterator other) const noexcept { return m_value != other.m_value; }

		private:

			SEQUENCE m_value;

		};

		constexpr sequence_range() noexcept
			: m_begin()
			, m_end()
		{}

		constexpr sequence_range(SEQUENCE begin, SEQUENCE end) noexcept
			: m_begin(begin)
			, m_end(end)
		{}

		constexpr const_iterator begin() const noexcept { return const_iterator(m_begin); }
		constexpr const_iterator end() const noexcept { return const_iterator(m_end); }

		constexpr SEQUENCE front() const noexcept { return m_begin; }
		constexpr SEQUENCE back() const noexcept { return m_end - 1; }

		constexpr size_type size() const noexcept
		{
			return static_cast<size_type>(TRAITS::difference(m_end, m_begin));
		}

		constexpr bool empty() const noexcept
		{
			return m_begin == m_end;
		}

		constexpr SEQUENCE operator[](size_type index) const noexcept
		{
			return m_begin + index;
		}

		constexpr sequence_range first(size_type count) const noexcept
		{
			return sequence_range{ m_begin, static_cast<SEQUENCE>(m_begin + std::min(size(), count)) };
		}

		constexpr sequence_range skip(size_type count) const noexcept
		{
			return sequence_range{ m_begin + std::min(size(), count), m_end };
		}

	private:

		SEQUENCE m_begin;
		SEQUENCE m_end;

	};
}

#endif
