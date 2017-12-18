///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/net/ip_endpoint.hpp>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("ip_endpoint");

using namespace cppcoro::net;

TEST_CASE("to_string")
{
	ip_endpoint a = ipv4_endpoint{ ipv4_address{ 192, 168, 2, 254 }, 80 };
	ip_endpoint b = ipv6_endpoint{
		*ipv6_address::from_string("2001:0db8:85a3:0000:0000:8a2e:0370:7334"),
		22 };

	CHECK(a.to_string() == "192.168.2.254:80");
	CHECK(b.to_string() == "[2001:db8:85a3::8a2e:370:7334]:22");
}

TEST_CASE("from_string")
{
	CHECK(ip_endpoint::from_string("") == std::nullopt);
	CHECK(ip_endpoint::from_string("[foo]:123") == std::nullopt);
	CHECK(ip_endpoint::from_string("[123]:1000") == std::nullopt);
	CHECK(ip_endpoint::from_string("[10.11.12.13]:1000") == std::nullopt);

	CHECK(ip_endpoint::from_string("192.168.2.254:80") ==
		ipv4_endpoint{
			ipv4_address{ 192, 168, 2, 254 }, 80 });
	CHECK(ip_endpoint::from_string("[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:443") ==
		ipv6_endpoint{
			ipv6_address{ 0x2001, 0xdb8, 0x85a3, 0x0, 0x0, 0x8a2e, 0x370, 0x7334 },
			443 });
}

TEST_SUITE_END();

