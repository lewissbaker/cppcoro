///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/cancellation_registration.hpp>

#include "cancellation_state.hpp"

#include <cassert>

cppcoro::cancellation_registration::~cancellation_registration()
{
	if (m_state != nullptr)
	{
		m_state->deregister_callback(this);
		m_state->release_token_ref();
	}
}

void cppcoro::cancellation_registration::register_callback(cancellation_token&& token)
{
	auto* state = token.m_state;
	if (state != nullptr && state->can_be_cancelled())
	{
		m_state = state;
		if (state->try_register_callback(this))
		{
			token.m_state = nullptr;
		}
		else
		{
			m_state = nullptr;
			m_callback();
		}
	}
	else
	{
		m_state = nullptr;
	}
}
