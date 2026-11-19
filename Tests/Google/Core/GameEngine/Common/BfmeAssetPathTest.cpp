#include "Common/BfmeAssetPath.h"
#include <gtest/gtest.h>

TEST(BfmeAssetPath, ShardsModelsByTheFirstTwoCharacters)
{
	EXPECT_EQ(makeBfmeAssetPath("art/w3d/", "GUAragorn.w3d"), "art/w3d/GU/GUAragorn.w3d");
}

TEST(BfmeAssetPath, PreservesTheOriginalTextureFilename)
{
	EXPECT_EQ(makeBfmeAssetPath("art/compiledtextures/", "a_hero.DDS"),
		"art/compiledtextures/a_/a_hero.DDS");
	EXPECT_EQ(makeBfmeAssetPath("art/compiledtextures/", "a.detail.dds"),
		"art/compiledtextures/a./a.detail.dds");
}

TEST(BfmeAssetPath, RejectsPathsAndNamesWithoutTwoStemCharacters)
{
	EXPECT_TRUE(makeBfmeAssetPath("art/w3d/", "").empty());
	EXPECT_TRUE(makeBfmeAssetPath("art/w3d/", "a.w3d").empty());
	EXPECT_TRUE(makeBfmeAssetPath("art/w3d/", "..hero.w3d").empty());
	EXPECT_TRUE(makeBfmeAssetPath("art/w3d/", "units/hero.w3d").empty());
	EXPECT_TRUE(makeBfmeAssetPath("art/w3d/", "units\\hero.w3d").empty());
}
