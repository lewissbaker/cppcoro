///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_SPIN_WAIT_HPP_INCLUDED
#define CPPCORO_SPIN_WAIT_HPP_INCLUDED

#include <cstdint>

namespace cppcoro
{
	class spin_wait
	{
	public:

		spin_wait() noexcept;

		bool next_spin_will_yield() const noexcept;

		void spin_one() noexcept;

		void reset() noexcept;

	private:

		std::uint32_t m_count;

	};
}

#endif
