#include "Common/INIRetailTolerance.h"

#include <gtest/gtest.h>

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
