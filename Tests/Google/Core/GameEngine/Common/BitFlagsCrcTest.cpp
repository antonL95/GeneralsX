#include <gtest/gtest.h>

#include "Common/BitFlagsCrc.h"

#include <bitset>
#include <cstdint>
#include <cstring>
#include <random>

namespace
{
	std::uint32_t foldXferCrc(std::uint32_t crc, const void* data, std::size_t size)
	{
		const auto* bytes = static_cast<const unsigned char*>(data);
		std::size_t offset = 0;
		for (; offset + 4 <= size; offset += 4)
		{
			const std::uint32_t word = (std::uint32_t{bytes[offset]} << 24)
				| (std::uint32_t{bytes[offset + 1]} << 16)
				| (std::uint32_t{bytes[offset + 2]} << 8)
				| std::uint32_t{bytes[offset + 3]};
			crc = (crc << 1) + word + ((crc >> 31) & 1u);
		}
		std::uint32_t tail = 0;
		switch (size - offset)
		{
		case 3:
			tail += std::uint32_t{bytes[offset + 2]} << 16;
			[[fallthrough]];
		case 2:
			tail += std::uint32_t{bytes[offset + 1]} << 8;
			[[fallthrough]];
		case 1:
			tail += bytes[offset];
			crc = (crc << 1) + tail + ((crc >> 31) & 1u);
			break;
		default:
			break;
		}
		return crc;
	}

	template <std::size_t BITS>
	std::uint32_t xferCrc(const std::bitset<BITS>& bits, std::size_t byteCount)
	{
		const unsigned char version = 1;
		std::uint32_t crc = foldXferCrc(0, &version, 1);
		return foldXferCrc(crc, &bits, byteCount);
	}

	template <std::size_t UPSTREAM, std::size_t WIDE>
	void expectUpstreamInput()
	{
		using Upstream = std::bitset<UPSTREAM>;
		using Wide = std::bitset<WIDE>;
		SCOPED_TRACE(UPSTREAM);
		ASSERT_LT(sizeof(Upstream), sizeof(Wide));

		std::mt19937 random(static_cast<unsigned>(UPSTREAM * 1000 + WIDE));
		for (int round = 0; round < 200; ++round)
		{
			Upstream upstream;
			Wide wide;
			for (std::size_t bit = 0; bit < UPSTREAM; ++bit)
			{
				const bool set = round == 1 || (round > 1 && (random() & 3u) == 0);
				upstream.set(bit, set);
				wide.set(bit, set);
			}

			const std::size_t upstreamBytes = sizeof(upstream);
			const std::size_t wideBytes = BitFlagsCrc::byteCount<UPSTREAM>(wide);
			ASSERT_EQ(wideBytes, upstreamBytes);
			EXPECT_EQ(std::memcmp(&upstream, &wide, upstreamBytes), 0);
			EXPECT_EQ(xferCrc(upstream, upstreamBytes), xferCrc(wide, wideBytes));
		}
	}
}

TEST(BitFlagsCrcTest, ZeroHourKindOfHashesUpstreamWidth)
{
	expectUpstreamInput<117, 254>();
}

TEST(BitFlagsCrcTest, OtherWidenedSetsWouldHashUpstreamWidth)
{
	expectUpstreamInput<117, 601>();
	expectUpstreamInput<45, 116>();
	expectUpstreamInput<38, 87>();
	expectUpstreamInput<67, 195>();
	expectUpstreamInput<17, 109>();
}

TEST(BitFlagsCrcTest, WholeMaskWhenABitLiesBeyondThePrefix)
{
	using Wide = std::bitset<254>;
	const std::size_t prefixBytes = sizeof(std::bitset<117>);

	Wide lastUpstreamBit;
	lastUpstreamBit.set(116);
	EXPECT_EQ(BitFlagsCrc::byteCount<117>(lastUpstreamBit), prefixBytes);

	Wide insidePrefixWord;
	insidePrefixWord.set(prefixBytes * 8 - 1);
	EXPECT_EQ(BitFlagsCrc::byteCount<117>(insidePrefixWord), prefixBytes);

	Wide firstBeyond;
	firstBeyond.set(prefixBytes * 8);
	EXPECT_EQ(BitFlagsCrc::byteCount<117>(firstBeyond), sizeof(Wide));

	Wide lastBit;
	lastBit.set(253);
	EXPECT_EQ(BitFlagsCrc::byteCount<117>(lastBit), sizeof(Wide));

	Wide empty;
	EXPECT_NE(xferCrc(empty, sizeof(Wide)), xferCrc(firstBeyond, sizeof(Wide)));
}

TEST(BitFlagsCrcTest, UnchangedSetsHashWholeMask)
{
	std::bitset<117> kindOf;
	kindOf.set(3);
	EXPECT_EQ(BitFlagsCrc::byteCount<117>(kindOf), sizeof(kindOf));

	std::bitset<512> upgrades;
	upgrades.set(511);
	EXPECT_EQ(BitFlagsCrc::byteCount<512>(upgrades), sizeof(upgrades));

	std::bitset<120> sameStorage;
	EXPECT_EQ(BitFlagsCrc::byteCount<117>(sameStorage), sizeof(sameStorage));
}

TEST(BitFlagsCrcTest, WholeWidthHashDiffersFromUpstream)
{
	std::bitset<117> upstream;
	std::bitset<254> wide;
	upstream.set(1);
	wide.set(1);
	EXPECT_NE(xferCrc(upstream, sizeof(upstream)), xferCrc(wide, sizeof(wide)));
}
