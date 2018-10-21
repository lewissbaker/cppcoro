///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/net/ipv4_endpoint.hpp>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("ip_endpoint");

using namespace cppcoro::net;

TEST_CASE("to_string")
{
	CHECK(ipv4_endpoint{ ipv4_address{ 192, 168, 2, 254 }, 80 }.to_string() == "192.168.2.254:80");
}

TEST_CASE("from_string")
{
	CHECK(ipv4_endpoint::from_string("") == std::nullopt);
	CHECK(ipv4_endpoint::from_string("  ") == std::nullopt);
	CHECK(ipv4_endpoint::from_string("100") == std::nullopt);
	CHECK(ipv4_endpoint::from_string("100.10.200.20") == std::nullopt);
	CHECK(ipv4_endpoint::from_string("100.10.200.20:") == std::nullopt);
	CHECK(ipv4_endpoint::from_string("100.10.200.20::80") == std::nullopt);
	CHECK(ipv4_endpoint::from_string("100.10.200.20 80") == std::nullopt);

	CHECK(ipv4_endpoint::from_string("192.168.2.254:80") ==
		ipv4_endpoint{ ipv4_address{ 192, 168, 2, 254 }, 80 });
}

TEST_SUITE_END();
