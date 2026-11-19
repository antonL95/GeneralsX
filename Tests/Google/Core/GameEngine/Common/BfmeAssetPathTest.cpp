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

TEST(BfmeAssetPath, ShardsJpegAndItsCompanionTogether)
{
	EXPECT_EQ(makeBfmeJpegPath("rjsky01.jpg"), "Art/CompiledTextures/rj/rjsky01.jpg");
	EXPECT_EQ(makeBfmeJpegPath("rjsky01.png"), "Art/CompiledTextures/rj/rjsky01.png");
	EXPECT_EQ(makeBfmeJpegPath("amonsul .jpeg.jpg"), "Art/CompiledTextures/am/amonsul .jpeg.jpg");
}

TEST(BfmeAssetPath, UsesFlatTextureDirectoryForAptAndShortNames)
{
	EXPECT_EQ(makeBfmeJpegPath("apt_menu.jpg"), "Art/Textures/apt_menu.jpg");
	EXPECT_EQ(makeBfmeJpegPath("a .png"), "Art/Textures/a .png");
}

TEST(BfmeAssetPath, RetainsFullyQualifiedTexturePaths)
{
	EXPECT_EQ(makeBfmeJpegPath("Art/Textures/a.jpg"), "Art/Textures/a.jpg");
	EXPECT_EQ(makeBfmeJpegPath("Art\\Textures\\a.png"), "Art\\Textures\\a.png");
}
