///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/net/ipv4_address.hpp>

namespace
{
	namespace local
	{
		constexpr bool is_digit(char c)
		{
			return c >= '0' && c <= '9';
		}

		constexpr std::uint8_t digit_value(char c)
		{
			return static_cast<std::uint8_t>(c - '0');
		}
	}
}

std::optional<cppcoro::net::ipv4_address>
cppcoro::net::ipv4_address::from_string(std::string_view string) noexcept
{
	if (string.empty()) return std::nullopt;

	if (!local::is_digit(string[0]))
	{
		return std::nullopt;
	}

	const auto length = string.length();

	std::uint8_t partValues[4];

	if (string[0] == '0' && length > 1)
	{
		if (local::is_digit(string[1]))
		{
			// Octal format (not supported)
			return std::nullopt;
		}
		else if (string[1] == 'x')
		{
			// Hexadecimal format (not supported)
			return std::nullopt;
		}
	}

	// Parse the first integer.
	// Could be a single 32-bit integer or first integer in a dotted decimal string.

	std::size_t pos = 0;

	{
		constexpr std::uint32_t maxValue = 0xFFFFFFFFu / 10;
		constexpr std::uint32_t maxDigit = 0xFFFFFFFFu % 10;

		std::uint32_t partValue = local::digit_value(string[pos]);
		++pos;

		while (pos < length && local::is_digit(string[pos]))
		{
			const auto digitValue = local::digit_value(string[pos]);
			++pos;

			// Check if this digit would overflow the 32-bit integer
			if (partValue > maxValue || (partValue == maxValue && digitValue > maxDigit))
			{
				return std::nullopt;
			}

			partValue = (partValue * 10) + digitValue;
		}

		if (pos == length)
		{
			// A single-integer string
			return ipv4_address{ partValue };
		}
		else if (partValue > 255)
		{
			// Not a valid first component of dotted decimal
			return std::nullopt;
		}

		partValues[0] = static_cast<std::uint8_t>(partValue);
	}

	for (int part = 1; part < 4; ++part)
	{
		if ((pos + 1) >= length || string[pos] != '.' || !local::is_digit(string[pos + 1]))
		{
			return std::nullopt;
		}

		// Skip the '.'
		++pos;

		// Check for an octal format (not yet supported)
		const bool isPartOctal =
			(pos + 1) < length &&
			string[pos] == '0' &&
			local::is_digit(string[pos + 1]);
		if (isPartOctal)
		{
			return std::nullopt;
		}

		std::uint32_t partValue = local::digit_value(string[pos]);
		++pos;
		if (pos < length && local::is_digit(string[pos]))
		{
			partValue = (partValue * 10) + local::digit_value(string[pos]);
			++pos;
			if (pos < length && local::is_digit(string[pos]))
			{
				partValue = (partValue * 10) + local::digit_value(string[pos]);
				if (partValue > 255)
				{
					return std::nullopt;
				}

				++pos;
			}
		}

		partValues[part] = static_cast<std::uint8_t>(partValue);
	}

	if (pos < length)
	{
		// Extra chars after end of a valid IPv4 string
		return std::nullopt;
	}

	return ipv4_address{ partValues };
}

std::string cppcoro::net::ipv4_address::to_string() const
{
	// Buffer is large enough to hold larges ip address
	// "xxx.xxx.xxx.xxx"
	char buffer[15];

	char* c = &buffer[0];
	for (int i = 0; i < 4; ++i)
	{
		if (i > 0)
		{
			*c++ = '.';
		}

		if (m_bytes[i] >= 100)
		{
			*c++ = '0' + (m_bytes[i] / 100);
			*c++ = '0' + (m_bytes[i] % 100) / 10;
			*c++ = '0' + (m_bytes[i] % 10);
		}
		else if (m_bytes[i] >= 10)
		{
			*c++ = '0' + (m_bytes[i] / 10);
			*c++ = '0' + (m_bytes[i] % 10);
		}
		else
		{
			*c++ = '0' + m_bytes[i];
		}
	}

	return std::string{ &buffer[0], c };
}
