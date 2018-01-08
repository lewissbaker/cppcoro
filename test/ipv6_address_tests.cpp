///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/net/ipv6_address.hpp>

#include "doctest/doctest.h"


TEST_SUITE_BEGIN("ipv6_address");

using cppcoro::net::ipv6_address;

TEST_CASE("default constructor")
{
	ipv6_address zero;
	for (std::uint8_t i = 0; i < 16; ++i)
	{
		CHECK(zero.bytes()[i] == 0);
	}

	CHECK(zero == ipv6_address::unspecified());
}

TEST_CASE("to_string")
{
	CHECK(ipv6_address(0, 0).to_string() == "::");
	CHECK(ipv6_address::loopback().to_string() == "::1");

	CHECK(
		ipv6_address(0x0102030405060708, 0x090A0B0C0D0E0F10).to_string() ==
		"102:304:506:708:90a:b0c:d0e:f10");
	CHECK(
		ipv6_address(0x0001001001001000, 0x0).to_string() ==
		"1:10:100:1000::");
	CHECK(
		ipv6_address(0x0002030405060708, 0x090A0B0C0D0E0F10).to_string() ==
		"2:304:506:708:90a:b0c:d0e:f10");
	CHECK(
		ipv6_address(0x0000030405060708, 0x090A0B0C0D0E0F10).to_string() ==
		"0:304:506:708:90a:b0c:d0e:f10");
	CHECK(
		ipv6_address(0x0000000005060708, 0x090A0B0C0D0E0F10).to_string() ==
		"::506:708:90a:b0c:d0e:f10");
	CHECK(
		ipv6_address(0x0102030400000000, 0x00000B0C0D0E0F10).to_string() ==
		"102:304::b0c:d0e:f10");
	CHECK(
		ipv6_address(0x0102030405060708, 0x090A0B0C0D0E0000).to_string() ==
		"102:304:506:708:90a:b0c:d0e:0");
	CHECK(
		ipv6_address(0x0102030405060708, 0x090A0B0C00000000).to_string() ==
		"102:304:506:708:90a:b0c::");

	// Check that it contracts the first of multiple equal-length zero runs.
	CHECK(
		ipv6_address(0x0102030400000000, 0x090A0B0C00000000).to_string() ==
		"102:304::90a:b0c:0:0");
}

TEST_CASE("from_string")
{
	CHECK(ipv6_address::from_string("") == std::nullopt);
	CHECK(ipv6_address::from_string("123") == std::nullopt);
	CHECK(ipv6_address::from_string("foo") == std::nullopt);
	CHECK(ipv6_address::from_string(":1234") == std::nullopt);
	CHECK(ipv6_address::from_string("0102:0304:0506:0708:090a:0b0c:0d0e:0f10 ") == std::nullopt);
	CHECK(
		ipv6_address::from_string(" 0102:0304:0506:0708:090a:0b0c:0d0e:0f10") ==
		std::nullopt);
	CHECK(
		ipv6_address::from_string("0102:0304:0506:0708:090a:0b0c:0d0e:0f10:") ==
		std::nullopt);
	CHECK(
		ipv6_address::from_string("0102:0304:0506:0708:090a:0b0c:0d0e") ==
		std::nullopt);
	CHECK(
		ipv6_address::from_string("01022:0304:0506:0708:090a:0b0c:0d0e:0f10") ==
		std::nullopt);
	CHECK(
		ipv6_address::from_string("0102:0304:0506:192.168.0.1:0b0c:0d0e:0f10") ==
		std::nullopt);
	CHECK(ipv6_address::from_string("::") == ipv6_address(0, 0));
	CHECK(ipv6_address::from_string("::1") == ipv6_address::loopback());
	CHECK(ipv6_address::from_string("::01") == ipv6_address::loopback());
	CHECK(ipv6_address::from_string("::001") == ipv6_address::loopback());
	CHECK(ipv6_address::from_string("::0001") == ipv6_address::loopback());
	CHECK(
		ipv6_address::from_string("0102:0304:0506:0708:090a:0b0c:0d0e:0f10") ==
		ipv6_address(0x0102030405060708, 0x090A0B0C0D0E0F10));
	CHECK(
		ipv6_address::from_string("0002:0304:0506:0708:090a:0b0c:0d0e:0f10") ==
		ipv6_address(0x0002030405060708, 0x090A0B0C0D0E0F10));
	CHECK(
		ipv6_address::from_string("0000:0304:0506:0708:090a:0b0c:0d0e:0f10") ==
		ipv6_address(0x0000030405060708, 0x090A0B0C0D0E0F10));
	CHECK(
		ipv6_address::from_string("::0506:0708:090a:0b0c:0d0e:0f10") ==
		ipv6_address(0x0000000005060708, 0x090A0B0C0D0E0F10));
	CHECK(
		ipv6_address::from_string("0102:0304::0b0c:0d0e:0f10") ==
		ipv6_address(0x0102030400000000, 0x00000B0C0D0E0F10));
	CHECK(
		ipv6_address::from_string("0102:0304:0506:0708:090a:0b0c::") ==
		ipv6_address(0x0102030405060708, 0x090A0B0C00000000));
	CHECK(
		ipv6_address::from_string("2001:db8:85a3:8d3:1319:8a2e:370:7348") ==
		ipv6_address(0x20010db885a308d3, 0x13198a2e03707348));
}

TEST_CASE("from_string IPv4 interop format")
{
	CHECK(
		ipv6_address::from_string("::ffff:192.168.0.1") ==
		ipv6_address(0x0, 0xffffc0a80001));
	CHECK(
		ipv6_address::from_string("0102:0304::128.69.32.17") ==
		ipv6_address(0x0102030400000000, 0x0000000080452011));
	CHECK(
		ipv6_address::from_string("0102:0304::128.69.32.17") ==
		ipv6_address(0x0102030400000000, 0x0000000080452011));

	// Hexadecimal chars in dotted decimal part
	CHECK(ipv6_address::from_string("64:ff9b::12f.100.30.1") == std::nullopt);
	CHECK(ipv6_address::from_string("64:ff9b::123.10a.30.1") == std::nullopt);
	CHECK(ipv6_address::from_string("64:ff9b::123.100.3d.1") == std::nullopt);
	CHECK(ipv6_address::from_string("64:ff9b::12f.100.30.f4") == std::nullopt);

	// Overflow of individual parts of dotted decimal notation
	CHECK(ipv6_address::from_string("::ffff:456.12.45.30") == std::nullopt);
	CHECK(ipv6_address::from_string("::ffff:45.256.45.30") == std::nullopt);
	CHECK(ipv6_address::from_string("::ffff:45.25.677.30") == std::nullopt);
	CHECK(ipv6_address::from_string("::ffff:123.12.45.301") == std::nullopt);
}

TEST_CASE("operator<")
{
	ipv6_address a(0x0, 0x1);
	ipv6_address b(0xff00000000000011, 0xee00000000000022);
	ipv6_address c(0xee00000000000022, 0xee00000000000022);
	ipv6_address d(0xee00000000000022, 0xff00000000000011);

	CHECK(a <= a);
	CHECK(a < b);
	CHECK(a < c);
	CHECK(a < d);
	CHECK(b >= b);
	CHECK(b > c);
	CHECK(b > d);
	CHECK(c < d);
}

TEST_SUITE_END();
