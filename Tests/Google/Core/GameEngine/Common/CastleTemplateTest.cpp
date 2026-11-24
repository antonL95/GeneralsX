#include <bit>
#include <cstdint>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "GameLogic/CastleTemplateStore.h"

namespace
{
using Bytes = std::vector<std::uint8_t>;

void writeWord(Bytes& bytes, std::uint32_t value)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}

void writeString(Bytes& bytes, std::string_view value)
{
	bytes.push_back(static_cast<std::uint8_t>(value.size()));
	bytes.push_back(0);
	bytes.insert(bytes.end(), value.begin(), value.end());
}

Bytes makeChunk(unsigned version, std::string_view second = "Pad")
{
	Bytes bytes;
	writeWord(bytes, 0x103);
	writeWord(bytes, 2);
	for (const std::string_view name : {std::string_view("Keep"), second})
	{
		writeString(bytes, "");
		writeString(bytes, name);
		for (float value : {4.0f, -8.0f, 2.0f, 0.5f})
			writeWord(bytes, std::bit_cast<std::uint32_t>(value));
		if (version >= 4)
		{
			writeWord(bytes, 40);
			writeWord(bytes, 1);
		}
	}
	if (version >= 2)
	{
		writeWord(bytes, 2);
		for (const auto* name : {"Outer", "Inner"})
		{
			if (version >= 5)
				writeString(bytes, name);
			writeWord(bytes, 1);
			writeWord(bytes, version == 2 ? std::bit_cast<std::uint32_t>(std::int32_t{-4}) : std::bit_cast<std::uint32_t>(-4.0f));
			writeWord(bytes, version == 2 ? 8u : std::bit_cast<std::uint32_t>(8.0f));
			if (version == 2)
				writeWord(bytes, 0);
		}
	}
	return bytes;
}
}

TEST(CastleTemplateTest, ReadsAllVersionsAndPreservesMemberOrder)
{
	for (unsigned version = 1; version <= 5; ++version)
	{
		const auto result = CastleTemplates::parse(makeChunk(version), version);
		ASSERT_TRUE(result) << version;
		ASSERT_EQ(result->members.size(), 2u);
		EXPECT_EQ(result->members[0].templateName, "Keep");
		const auto& pad = result->members[1];
		EXPECT_EQ(pad.templateName, "Pad");
		EXPECT_EQ(pad.offsetX, 4.0f);
		EXPECT_EQ(pad.offsetY, -8.0f);
		EXPECT_EQ(pad.offsetZ, 2.0f);
		EXPECT_EQ(pad.angle, 0.5f);
		ASSERT_EQ(result->perimeters.size(), version == 1 ? 0u : 2u);
		for (const auto& perimeter : result->perimeters)
		{
			ASSERT_EQ(perimeter.size(), 1u);
			EXPECT_EQ(perimeter.front().first, -4.0f);
			EXPECT_EQ(perimeter.front().second, 8.0f);
		}
	}
}

TEST(CastleTemplateTest, RejectsCountsOutsideTheChunk)
{
	auto bytes = makeChunk(5);
	bytes[7] = 0xff;
	EXPECT_FALSE(CastleTemplates::parse(bytes, 5));

	bytes = makeChunk(5);
	bytes[bytes.size() - 9] = 0xff;
	EXPECT_FALSE(CastleTemplates::parse(bytes, 5));
}

TEST(CastleTemplateTest, RejectsTruncatedBodiesAndUnknownVersions)
{
	auto bytes = makeChunk(5);
	EXPECT_FALSE(CastleTemplates::parse(bytes, 0));
	EXPECT_FALSE(CastleTemplates::parse(bytes, 6));
	while (!bytes.empty())
	{
		bytes.pop_back();
		EXPECT_FALSE(CastleTemplates::parse(bytes, 5)) << bytes.size();
	}
}

TEST(CastleTemplateTest, KeepsEmptyTemplateNamesForTheUnpackToSkip)
{
	const auto result = CastleTemplates::parse(makeChunk(5, ""), 5);
	ASSERT_TRUE(result);
	ASSERT_EQ(result->members.size(), 2u);
	EXPECT_EQ(result->members[0].templateName, "Keep");
	EXPECT_TRUE(result->members[1].templateName.empty());
	EXPECT_EQ(result->members[1].offsetY, -8.0f);
}
