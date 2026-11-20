#include "fontnames.h"

#include <gtest/gtest.h>

namespace
{
	std::vector<FontNames::Candidate> installFonts()
	{
		return {
			{{"Albertus MT", "AlbertusMT", "albertusmt"}, false},
			{{"Omnia LT Std", "OmniaLTStd", "omnialtstd"}, false},
			{{"SachaWynterTight", "SachaWynterTight", "sachwt__"}, false},
		};
	}
}

TEST(FontNames, NormalizeIgnoresCaseSpacesHyphensAndUnderscores)
{
	EXPECT_EQ(FontNames::Normalize("Omnia LT-Std_x"), "omnialtstdx");
	EXPECT_EQ(FontNames::Stem("Data\\fonts/sachwt__.ttf"), "sachwt__");
	EXPECT_EQ(FontNames::Stem("albertusmt.otf"), "albertusmt");
}

TEST(FontNames, ExactNamesWin)
{
	const auto fonts = installFonts();
	EXPECT_EQ(FontNames::Find(fonts, "Albertus MT", false), 0);
	EXPECT_EQ(FontNames::Find(fonts, "albertusmt", false), 0);
	EXPECT_EQ(FontNames::Find(fonts, "OMNIA LT STD", true), 1);
	EXPECT_EQ(FontNames::Find(fonts, "SachaWynterTight", false), 2);
}

TEST(FontNames, UniquePrefixMapsTheDataNameToTheFace)
{
	const auto fonts = installFonts();
	EXPECT_EQ(FontNames::Find(fonts, "SachaWynter", false), 2);
	EXPECT_EQ(FontNames::Find(fonts, "Times New Roman", false), -1);
	EXPECT_EQ(FontNames::Find(fonts, "", false), -1);
}

TEST(FontNames, AmbiguousPrefixIsRejected)
{
	const std::vector<FontNames::Candidate> fonts = {
		{{"Sample One"}, false},
		{{"Sample Two"}, false},
	};
	EXPECT_EQ(FontNames::Find(fonts, "Sample", false), -1);
	EXPECT_EQ(FontNames::Find(fonts, "Sample T", false), 1);
}

TEST(FontNames, BoldFaceIsPreferredWhenRequested)
{
	const std::vector<FontNames::Candidate> fonts = {
		{{"Face"}, false},
		{{"Face"}, true},
	};
	EXPECT_EQ(FontNames::Find(fonts, "Face", true), 1);
	EXPECT_EQ(FontNames::Find(fonts, "Face", false), 0);
	EXPECT_EQ(FontNames::Find(fonts, "Fa", true), 1);
}

TEST(FontNames, SameFamilyComparesNormalizedNames)
{
	EXPECT_TRUE(FontNames::Same_Family("Albertus MT", "albertus mt"));
	EXPECT_FALSE(FontNames::Same_Family("Albertus MT", "DejaVu Sans"));
	EXPECT_FALSE(FontNames::Same_Family("", ""));
}
