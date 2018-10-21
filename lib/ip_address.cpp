///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/net/ip_address.hpp>

std::string cppcoro::net::ip_address::to_string() const
{
	return is_ipv4() ? m_ipv4.to_string() : m_ipv6.to_string();
}

std::optional<cppcoro::net::ip_address>
cppcoro::net::ip_address::from_string(std::string_view string) noexcept
{
	if (auto ipv4 = ipv4_address::from_string(string); ipv4)
	{
		return *ipv4;
	}

	if (auto ipv6 = ipv6_address::from_string(string); ipv6)
	{
		return *ipv6;
	}

	return std::nullopt;
}
