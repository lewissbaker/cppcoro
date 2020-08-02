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
#else
# include <netinet/in.h>
# define SD_RECEIVE SHUT_RD
# define SD_SEND SHUT_WR
# define INVALID_SOCKET -1
# define SOCKET_ERROR -1
# define SOCKADDR_STORAGE struct sockaddr_storage
# define SOCKADDR struct sockaddr
# define SOCKADDR_IN struct sockaddr_in
# define SOCKADDR_IN6 struct sockaddr_in6
# define closesocket(__handle) close((__handle))
# include <functional>
#endif

namespace cppcoro
{
	namespace net
	{
		class ip_endpoint;

		namespace detail
		{
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
		}
	}
}

#endif
