#include "Common/INIRetailTolerance.h"

#include <gtest/gtest.h>

TEST(INIRetailTolerance, FindsEndAlignedWithHeader)
{
	const std::string_view text = "Header\n\t\tNested\n\t\teNd\r\n \t\r\n\tEnD";
	EXPECT_TRUE(INIRetailTolerance::hasIndentedBlockEnd(text, 7, 4));
	EXPECT_FALSE(INIRetailTolerance::hasIndentedBlockEnd(text, 7, 0));
}

TEST(INIRetailTolerance, StopsAtSiblingOrOuterEnd)
{
	EXPECT_FALSE(INIRetailTolerance::hasIndentedBlockEnd("\t\tNested\n\tSibling\n\tEnd\n", 0, 4));
	EXPECT_FALSE(INIRetailTolerance::hasIndentedBlockEnd("\t\tNested\nEnd\n", 0, 4));
	EXPECT_FALSE(INIRetailTolerance::hasIndentedBlockEnd("\tEnd extra\n", 0, 4));
}

TEST(INIRetailTolerance, HandlesEmptyAndUnterminatedBlocks)
{
	EXPECT_TRUE(INIRetailTolerance::hasIndentedBlockEnd("End\r\n", 0, 0));
	EXPECT_FALSE(INIRetailTolerance::hasIndentedBlockEnd("\tContent\n\n", 0, 0));
	EXPECT_FALSE(INIRetailTolerance::hasIndentedBlockEnd("", 0, 0));
}

TEST(INIRetailTolerance, RecognizesOptionalRetailPaths)
{
	EXPECT_TRUE(INIRetailTolerance::isOptionalRetailINI("Data\\INI\\Default\\Voice"));
	EXPECT_TRUE(INIRetailTolerance::isOptionalRetailINI("Data\\english\\Language"));
	EXPECT_TRUE(INIRetailTolerance::isOptionalRetailINI("Data\\german\\CommandMap"));
}

TEST(INIRetailTolerance, KeepsOtherPathsRequired)
{
	EXPECT_FALSE(INIRetailTolerance::isOptionalRetailINI(""));
	EXPECT_FALSE(INIRetailTolerance::isOptionalRetailINI("Data\\INI\\Language"));
	EXPECT_FALSE(INIRetailTolerance::isOptionalRetailINI("Data\\INI\\Default\\Voice\\Other"));
	EXPECT_FALSE(INIRetailTolerance::isOptionalRetailINI("Data\\english\\GameData"));
	EXPECT_FALSE(INIRetailTolerance::isOptionalRetailINI("Other\\english\\Language"));
}
