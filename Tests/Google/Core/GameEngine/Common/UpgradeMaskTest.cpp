#include <gtest/gtest.h>

#include "Common/ZeroHourUpgradeMaskLayout.h"

#include <bit>
#include <bitset>
#include <cstdint>
#include <cstring>
#include <random>

namespace
{
	using LegacyMask = std::bitset<512>;
	using WideMask = std::bitset<ZeroHourUpgradeMaskLayout::CAPACITY>;

	std::uint32_t foldUpgradeMask(const void* data, int size)
	{
		const auto* bytes = static_cast<const unsigned char*>(data);
		std::uint32_t crc = 0x89abcdefU;
		for (int offset = 0; offset < size; offset += 4)
		{
			const auto word = (std::uint32_t{bytes[offset]} << 24)
				| (std::uint32_t{bytes[offset + 1]} << 16)
				| (std::uint32_t{bytes[offset + 2]} << 8)
				| std::uint32_t{bytes[offset + 3]};
			crc = std::rotl(crc, 1) + word;
		}
		return crc;
	}
}

TEST(UpgradeMaskTest, PreservesLegacyPrefixAndCrc)
{
	ASSERT_EQ(sizeof(LegacyMask), 64u);
	ASSERT_GE(sizeof(WideMask), sizeof(LegacyMask));
	const int wideCrcBytes = ZeroHourUpgradeMaskLayout::crcBytes(512, sizeof(WideMask));
	ASSERT_EQ(wideCrcBytes, 64);

	std::mt19937 random(0x1152);
	for (std::size_t bit = 0; bit < 512; ++bit)
	{
		SCOPED_TRACE(bit);
		LegacyMask legacy;
		WideMask wide;
		legacy.set(bit);
		wide.set(bit);
		EXPECT_EQ(std::memcmp(&legacy, &wide, sizeof(legacy)), 0);

		for (std::size_t other = 0; other < 512; ++other)
		{
			const bool set = (random() & 1u) != 0;
			legacy.set(other, set);
			wide.set(other, set);
		}
		EXPECT_EQ(std::memcmp(&legacy, &wide, sizeof(legacy)), 0);
		EXPECT_EQ(foldUpgradeMask(&legacy, sizeof(legacy)), foldUpgradeMask(&wide, wideCrcBytes));
	}
}

TEST(UpgradeMaskTest, IncludesHighBitsOnlyAboveLegacyCapacity)
{
	ASSERT_EQ(sizeof(WideMask), 144u);
	EXPECT_EQ(ZeroHourUpgradeMaskLayout::crcBytes(0, sizeof(WideMask)), 64);
	EXPECT_EQ(ZeroHourUpgradeMaskLayout::crcBytes(511, sizeof(WideMask)), 64);
	EXPECT_EQ(ZeroHourUpgradeMaskLayout::crcBytes(512, sizeof(WideMask)), 64);
	const int fullCrcBytes = ZeroHourUpgradeMaskLayout::crcBytes(513, sizeof(WideMask));
	ASSERT_EQ(fullCrcBytes, 144);
	EXPECT_EQ(ZeroHourUpgradeMaskLayout::crcBytes(1152, sizeof(WideMask)), 144);

	WideMask empty;
	WideMask withHighBit;
	withHighBit.set(512);
	EXPECT_EQ(foldUpgradeMask(&empty, 64), foldUpgradeMask(&withHighBit, 64));
	EXPECT_NE(foldUpgradeMask(&empty, fullCrcBytes), foldUpgradeMask(&withHighBit, fullCrcBytes));
}
