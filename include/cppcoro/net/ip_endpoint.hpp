///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_NET_IP_ENDPOINT_HPP_INCLUDED
#define CPPCORO_NET_IP_ENDPOINT_HPP_INCLUDED

#include <cppcoro/net/ip_address.hpp>
#include <cppcoro/net/ipv4_endpoint.hpp>
#include <cppcoro/net/ipv6_endpoint.hpp>

#include <cassert>
#include <optional>
#include <string>

namespace cppcoro
{
	namespace net
	{
		class ip_endpoint
		{
		public:

			// Constructs to IPv4 end-point 0.0.0.0:0
			ip_endpoint() noexcept;

			ip_endpoint(ipv4_endpoint endpoint) noexcept;
			ip_endpoint(ipv6_endpoint endpoint) noexcept;

			bool is_ipv4() const noexcept { return m_family == family::ipv4; }
			bool is_ipv6() const noexcept { return m_family == family::ipv6; }

			const ipv4_endpoint& to_ipv4() const;
			const ipv6_endpoint& to_ipv6() const;

			ip_address address() const noexcept;
			std::uint16_t port() const noexcept;

			std::string to_string() const;

			static std::optional<ip_endpoint> from_string(std::string_view string) noexcept;

			bool operator==(const ip_endpoint& rhs) const noexcept;
			bool operator!=(const ip_endpoint& rhs) const noexcept;

			//  ipv4_endpoint sorts less than ipv6_endpoint
			bool operator<(const ip_endpoint& rhs) const noexcept;
			bool operator>(const ip_endpoint& rhs) const noexcept;
			bool operator<=(const ip_endpoint& rhs) const noexcept;
			bool operator>=(const ip_endpoint& rhs) const noexcept;

		private:

			enum class family
			{
				ipv4,
				ipv6
			};

			family m_family;

			union
			{
				ipv4_endpoint m_ipv4;
				ipv6_endpoint m_ipv6;
			};

		};

		inline ip_endpoint::ip_endpoint() noexcept
			: m_family(family::ipv4)
			, m_ipv4()
		{}

		inline ip_endpoint::ip_endpoint(ipv4_endpoint endpoint) noexcept
			: m_family(family::ipv4)
			, m_ipv4(endpoint)
		{}

		inline ip_endpoint::ip_endpoint(ipv6_endpoint endpoint) noexcept
			: m_family(family::ipv6)
			, m_ipv6(endpoint)
		{
		}

		inline const ipv4_endpoint& ip_endpoint::to_ipv4() const
		{
			assert(is_ipv4());
			return m_ipv4;
		}

		inline const ipv6_endpoint& ip_endpoint::to_ipv6() const
		{
			assert(is_ipv6());
			return m_ipv6;
		}

		inline ip_address ip_endpoint::address() const noexcept
		{
			if (is_ipv4())
			{
				return m_ipv4.address();
			}
			else
			{
				return m_ipv6.address();
			}
		}

		inline std::uint16_t ip_endpoint::port() const noexcept
		{
			return is_ipv4() ? m_ipv4.port() : m_ipv6.port();
		}

		inline bool ip_endpoint::operator==(const ip_endpoint& rhs) const noexcept
		{
			if (is_ipv4())
			{
				return rhs.is_ipv4() && m_ipv4 == rhs.m_ipv4;
			}
			else
			{
				return rhs.is_ipv6() && m_ipv6 == rhs.m_ipv6;
			}
		}

		inline bool ip_endpoint::operator!=(const ip_endpoint& rhs) const noexcept
		{
			return !(*this == rhs);
		}

		inline bool ip_endpoint::operator<(const ip_endpoint& rhs) const noexcept
		{
			if (is_ipv4())
			{
				return !rhs.is_ipv4() || m_ipv4 < rhs.m_ipv4;
			}
			else
			{
				return rhs.is_ipv6() && m_ipv6 < rhs.m_ipv6;
			}
		}

		inline bool ip_endpoint::operator>(const ip_endpoint& rhs) const noexcept
		{
			return rhs < *this;
		}

		inline bool ip_endpoint::operator<=(const ip_endpoint& rhs) const noexcept
		{
			return !(rhs < *this);
		}

		inline bool ip_endpoint::operator>=(const ip_endpoint& rhs) const noexcept
		{
			return !(*this < rhs);
		}
	}
}

#endif
