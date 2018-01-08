///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/config.hpp>
#include <cppcoro/net/ipv6_endpoint.hpp>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("ipv6_endpoint");

using namespace cppcoro::net;

namespace
{
	constexpr bool isMsvc15_5X86Optimised =
#if CPPCORO_COMPILER_MSVC && CPPCORO_CPU_X86 && _MSC_VER == 1912 && defined(CPPCORO_RELEASE_OPTIMISED)
		true;
#else
		false;
#endif
}

// BUG: MSVC 15.5 x86 optimised builds generates bad code
TEST_CASE("to_string" * doctest::skip{ isMsvc15_5X86Optimised })
{
	CHECK(ipv6_endpoint{ ipv6_address{ 0x20010db885a30000, 0x00008a2e03707334 }, 80 }.to_string() ==
		"[2001:db8:85a3::8a2e:370:7334]:80");
}

// BUG: MSVC 15.5 x86 optimised builds generates bad code
TEST_CASE("from_string" * doctest::skip{ isMsvc15_5X86Optimised })
{
	CHECK(ipv6_endpoint::from_string("") == std::nullopt);
	CHECK(ipv6_endpoint::from_string("  ") == std::nullopt);
	CHECK(ipv6_endpoint::from_string("asdf") == std::nullopt);
	CHECK(ipv6_endpoint::from_string("100:100") == std::nullopt);
	CHECK(ipv6_endpoint::from_string("100.10.200.20:100") == std::nullopt);
	CHECK(ipv6_endpoint::from_string("2001:0db8:85a3:0000:0000:8a2e:0370:7334") == std::nullopt);
	CHECK(ipv6_endpoint::from_string("[2001:0db8:85a3:0000:0000:8a2e:0370:7334") == std::nullopt);
	CHECK(ipv6_endpoint::from_string("[2001:0db8:85a3:0000:0000:8a2e:0370:7334]") == std::nullopt);
	CHECK(ipv6_endpoint::from_string("[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:") == std::nullopt);
	CHECK(ipv6_endpoint::from_string("[2001:0db8:85a3:0000:0000:8a2e:0370:7334] :123") == std::nullopt);
	CHECK(ipv6_endpoint::from_string("[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:65536") == std::nullopt);
	CHECK(ipv6_endpoint::from_string("[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:6553600") == std::nullopt);

	CHECK(ipv6_endpoint::from_string("[::]:0") == ipv6_endpoint{});
	CHECK(ipv6_endpoint::from_string("[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:80") ==
		ipv6_endpoint{ ipv6_address{ 0x20010db885a30000, 0x00008a2e03707334 }, 80 });
	CHECK(ipv6_endpoint::from_string("[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:65535") ==
		ipv6_endpoint{ ipv6_address{ 0x20010db885a30000, 0x00008a2e03707334 }, 65535 });
}

TEST_SUITE_END();
