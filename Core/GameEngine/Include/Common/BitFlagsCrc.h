#pragma once

#include <bitset>
#include <cstddef>

namespace BitFlagsCrc
{
	template <std::size_t PREFIX_BITS, std::size_t NUMBITS>
	std::size_t byteCount([[maybe_unused]] const std::bitset<NUMBITS>& bits)
	{
		constexpr std::size_t fullBytes = sizeof(std::bitset<NUMBITS>);
		if constexpr (PREFIX_BITS == 0 || PREFIX_BITS >= NUMBITS || sizeof(std::bitset<PREFIX_BITS>) >= fullBytes)
		{
			return fullBytes;
		}
		else
		{
			constexpr std::size_t prefixBytes = sizeof(std::bitset<PREFIX_BITS>);
			return (bits >> (prefixBytes * 8)).none() ? prefixBytes : fullBytes;
		}
	}
}
