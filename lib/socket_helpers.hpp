///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_PRIVATE_SOCKET_HELPERS_HPP_INCLUDED
#define CPPCORO_PRIVATE_SOCKET_HELPERS_HPP_INCLUDED

#include <cppcoro/config.hpp>

#if CPPCORO_OS_WINNT
# include <cppcoro/detail/win32.hpp>
struct sockaddr;
struct sockaddr_storage;
#endif

namespace cppcoro
{
	namespace net
	{
		class ip_endpoint;

		namespace detail
		{
#if CPPCORO_OS_WINNT
			/// Convert a sockaddr to an IP endpoint.
			ip_endpoint sockaddr_to_ip_endpoint(const sockaddr& address) noexcept;

			/// Converts an ip_endpoint to a sockaddr structure.
			///
			/// \param endPoint
			/// The IP endpoint to convert to a sockaddr structure.
			///
			/// \param address
			/// The sockaddr structure to populate.
			///
			/// \return
			/// The length of the sockaddr structure that was populated.
			int ip_endpoint_to_sockaddr(
				const ip_endpoint& endPoint,
				std::reference_wrapper<sockaddr_storage> address) noexcept;

#endif
		}
	}
}

#endif
