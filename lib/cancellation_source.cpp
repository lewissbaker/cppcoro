///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/cancellation_source.hpp>

#include "cancellation_state.hpp"

#include <cassert>

cppcoro::cancellation_source::cancellation_source()
	: m_state(detail::cancellation_state::create())
{
}

cppcoro::cancellation_source::cancellation_source(const cancellation_source& other) noexcept
	: m_state(other.m_state)
{
	if (m_state != nullptr)
	{
		m_state->add_source_ref();
	}
}

cppcoro::cancellation_source::cancellation_source(cancellation_source&& other) noexcept
	: m_state(other.m_state)
{
	other.m_state = nullptr;
}

cppcoro::cancellation_source::~cancellation_source()
{
	if (m_state != nullptr)
	{
		m_state->release_source_ref();
	}
}

cppcoro::cancellation_source& cppcoro::cancellation_source::operator=(const cancellation_source& other) noexcept
{
	if (m_state != other.m_state)
	{
		if (m_state != nullptr)
		{
			m_state->release_source_ref();
		}

		m_state = other.m_state;

		if (m_state != nullptr)
		{
			m_state->add_source_ref();
		}
	}

	return *this;
}

cppcoro::cancellation_source& cppcoro::cancellation_source::operator=(cancellation_source&& other) noexcept
{
	if (this != &other)
	{
		if (m_state != nullptr)
		{
			m_state->release_source_ref();
		}

		m_state = other.m_state;
		other.m_state = nullptr;
	}

	return *this;
}

bool cppcoro::cancellation_source::can_be_cancelled() const noexcept
{
	return m_state != nullptr;
}

cppcoro::cancellation_token cppcoro::cancellation_source::token() const noexcept
{
	return cancellation_token(m_state);
}

void cppcoro::cancellation_source::request_cancellation()
{
	if (m_state != nullptr)
	{
		m_state->request_cancellation();
	}
}

bool cppcoro::cancellation_source::is_cancellation_requested() const noexcept
{
	return m_state != nullptr && m_state->is_cancellation_requested();
}
