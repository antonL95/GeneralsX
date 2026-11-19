#include "Common/RetailFileLayers.h"

#include <algorithm>
#include <array>
#include <gtest/gtest.h>

TEST(RetailFileLayers, ArchiveOrderUsesAsciiPunctuationAndIgnoresCase)
{
	std::array<std::string_view, 6> names = {
		"Textures.big", "_patch.big", "W3D.BIG", "!timer.big", "base.BIG", "#mod.big"
	};
	std::sort(names.begin(), names.end(), RetailFileLayers::archiveNameLess);
	const std::array<std::string_view, 6> expected = {
		"!timer.big", "#mod.big", "_patch.big", "base.BIG", "Textures.big", "W3D.BIG"
	};
	EXPECT_EQ(names, expected);
}

TEST(RetailFileLayers, ArchiveNamesWithIdenticalFoldsUseByteOrder)
{
	EXPECT_TRUE(RetailFileLayers::archiveNameLess("Same.BIG", "same.big"));
	EXPECT_FALSE(RetailFileLayers::archiveNameLess("same.big", "Same.BIG"));
	EXPECT_FALSE(RetailFileLayers::archiveNameLess("same.big", "same.big"));
}
