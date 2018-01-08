///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/net/ipv6_address.hpp>
#include <cppcoro/config.hpp>

#include <cassert>

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

		std::optional<std::uint8_t> try_parse_hex_digit(char c)
		{
			if (c >= '0' && c <= '9')
			{
				return static_cast<std::uint8_t>(c - '0');
			}
			else if (c >= 'a' && c <= 'f')
			{
				return static_cast<std::uint8_t>(c - 'a' + 10);
			}
			else if (c >= 'A' && c <= 'F')
			{
				return static_cast<std::uint8_t>(c - 'A' + 10);
			}

			return std::nullopt;
		}

		char hex_char(std::uint8_t value)
		{
			return value < 10 ?
				static_cast<char>('0' + value) :
				static_cast<char>('a' + value - 10);
		}
	}
}

std::optional<cppcoro::net::ipv6_address>
cppcoro::net::ipv6_address::from_string(std::string_view string) noexcept
{
	// Longest possible valid IPv6 string is
	// "xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:nnn.nnn.nnn.nnn"
	constexpr std::size_t maxLength = 45;

	if (string.empty() || string.length() > maxLength)
	{
		return std::nullopt;
	}

	const std::size_t length = string.length();

	std::optional<int> doubleColonPos;

	std::size_t pos = 0;

	if (length >= 2 && string[0] == ':' && string[1] == ':')
	{
		doubleColonPos = 0;
		pos = 2;
	}

	int partCount = 0;
	std::uint16_t parts[8] = { 0 };

	while (pos < length && partCount < 8)
	{
		std::uint8_t digits[4];
		int digitCount = 0;
		auto digit = local::try_parse_hex_digit(string[pos]);
		if (!digit)
		{
			return std::nullopt;
		}

		do
		{
			digits[digitCount] = *digit;
			++digitCount;
			++pos;
		} while (digitCount < 4 && pos < length && (digit = local::try_parse_hex_digit(string[pos])));

		// If we're not at the end of the string then there must either be a ':' or a '.' next
		// followed by the next part.
		if (pos < length)
		{
			// Check if there's room for anything after the separator.
			if ((pos + 1) == length)
			{
				return std::nullopt;
			}

			if (string[pos] == ':')
			{
				++pos;
				if (string[pos] == ':')
				{
					if (doubleColonPos)
					{
						// This is a second double-colon, which is invalid.
						return std::nullopt;
					}

					doubleColonPos = partCount + 1;
					++pos;
				}
			}
			else if (string[pos] == '.')
			{
				// Treat the current set of digits as decimal digits and parse
				// the remaining three groups as dotted decimal notation.

				// Decimal notation produces two 16-bit parts.
				// If we already have more than 6 parts then we'll end up
				// with too many.
				if (partCount > 6)
				{
					return std::nullopt;
				}

				// Check for over-long or octal notation.
				if (digitCount > 3 || (digitCount > 1 && digits[0] == 0))
				{
					return std::nullopt;
				}

				// Check that digits are valid decimal digits
				if (digits[0] > 9 ||
					(digitCount > 1 && digits[1] > 9) ||
					(digitCount == 3 && digits[2] > 9))
				{
					return std::nullopt;
				}

				std::uint16_t decimalParts[4];

				{
					decimalParts[0] = digits[0];
					for (int i = 1; i < digitCount; ++i)
					{
						decimalParts[0] *= 10;
						decimalParts[0] += digits[i];
					}

					if (decimalParts[0] > 255)
					{
						return std::nullopt;
					}
				}

				for (int decimalPart = 1; decimalPart < 4; ++decimalPart)
				{
					if (string[pos] != '.')
					{
						return std::nullopt;
					}

					++pos;

					if (pos == length || !local::is_digit(string[pos]))
					{
						// Expected a number after a dot.
						return std::nullopt;
					}

					const bool hasLeadingZero = string[pos] == '0';

					decimalParts[decimalPart] = local::digit_value(string[pos]);
					++pos;
					digitCount = 1;
					while (digitCount < 3 && pos < length && local::is_digit(string[pos]))
					{
						decimalParts[decimalPart] *= 10;
						decimalParts[decimalPart] += local::digit_value(string[pos]);
						++pos;
						++digitCount;
					}

					if (decimalParts[decimalPart] > 255)
					{
						return std::nullopt;
					}

					// Detect octal-style number (redundant leading zero)
					if (digitCount > 1 && hasLeadingZero)
					{
						return std::nullopt;
					}
				}

				parts[partCount] = (decimalParts[0] << 8) + decimalParts[1];
				parts[partCount + 1] = (decimalParts[2] << 8) + decimalParts[3];
				partCount += 2;

				// Dotted decimal notation only appears at end.
				// Don't parse any more of the string.
				break;
			}
			else
			{
				// Invalid separator.
				return std::nullopt;
			}
		}

		// Current part was made up of hex-digits.
		std::uint16_t partValue = digits[0];
		for (int i = 1; i < digitCount; ++i)
		{
			partValue = partValue * 16 + digits[i];
		}

		parts[partCount] = partValue;
		++partCount;
	}

	// Finished parsing the IPv6 address, we should have consumed all of the string.
	if (pos < length)
	{
		return std::nullopt;
	}

	if (partCount < 8)
	{
		if (!doubleColonPos)
		{
			return std::nullopt;
		}

		const int preCount = *doubleColonPos;

		//CPPCORO_ASSUME(preCount <= partCount);

		const int postCount = partCount - preCount;
		const int zeroCount = 8 - preCount - postCount;

		// Move parts after double colon down to the end.
		for (int i = 0; i < postCount; ++i)
		{
			parts[7 - i] = parts[7 - zeroCount - i];
		}

		// Fill gap with zeroes.
		for (int i = 0; i < zeroCount; ++i)
		{
			parts[preCount + i] = 0;
		}
	}
	else if (doubleColonPos)
	{
		return std::nullopt;
	}

	return ipv6_address{ parts };
}

std::string cppcoro::net::ipv6_address::to_string() const
{
	std::uint32_t longestZeroRunStart = 0;
	std::uint32_t longestZeroRunLength = 0;
	for (std::uint32_t i = 0; i < 8; )
	{
		if (m_bytes[2 * i] == 0 && m_bytes[2 * i + 1] == 0)
		{
			const std::uint32_t zeroRunStart = i;
			++i;
			while (i < 8 && m_bytes[2 * i] == 0 && m_bytes[2 * i + 1] == 0)
			{
				++i;
			}

			std::uint32_t zeroRunLength = i - zeroRunStart;
			if (zeroRunLength > longestZeroRunLength)
			{
				longestZeroRunLength = zeroRunLength;
				longestZeroRunStart = zeroRunStart;
			}
		}
		else
		{
			++i;
		}
	}

	// Longest string will be 8 x 4 digits + 7 ':' separators
	char buffer[40];

	char* c = &buffer[0];

	auto appendPart = [&](std::uint32_t index)
	{
		const std::uint8_t highByte = m_bytes[index * 2];
		const std::uint8_t lowByte = m_bytes[index * 2 + 1];

		// Don't output leading zero hex digits in the part string.
		if (highByte > 0 || lowByte > 15)
		{
			if (highByte > 0)
			{
				if (highByte > 15)
				{
					*c++ = local::hex_char(highByte >> 4);
				}
				*c++ = local::hex_char(highByte & 0xF);
			}
			*c++ = local::hex_char(lowByte >> 4);
		}
		*c++ = local::hex_char(lowByte & 0xF);
	};

	if (longestZeroRunLength >= 2)
	{
		for (std::uint32_t i = 0; i < longestZeroRunStart; ++i)
		{
			if (i > 0)
			{
				*c++ = ':';
			}

			appendPart(i);
		}

		*c++ = ':';
		*c++ = ':';

		for (std::uint32_t i = longestZeroRunStart + longestZeroRunLength; i < 8; ++i)
		{
			appendPart(i);

			if (i < 7)
			{
				*c++ = ':';
			}
		}
	}
	else
	{
		appendPart(0);
		for (std::uint32_t i = 1; i < 8; ++i)
		{
			*c++ = ':';
			appendPart(i);
		}
	}

	assert((c - &buffer[0]) <= sizeof(buffer));

	return std::string{ &buffer[0], c };
}
