#include "GameClient/FontSubstitution.h"

#include <gtest/gtest.h>

namespace
{
	FontRequest resolved(const FontSubstitutionTable& table, const char* name, int size, bool bold = false)
	{
		FontRequest result;
		EXPECT_TRUE(table.resolve(FontRequest{name, size, bold}, result));
		return result;
	}

	const char* const kInterpolationFixture =
		"FontSubstitution \"Fixture Source\" ; interpolation example\n"
		"  Size 10 = 10 Example Sans\n"
		"  Size 20 = 16 Example Sans\n"
		"  Size 24 = 18 Example Display\n"
		"  Size 48 = 36 Example Display\n"
		"End\n";
}

TEST(FontSubstitution, InterpolatesFromTheLowerEntry)
{
	std::vector<std::string> warnings;
	const auto table = FontSubstitutionTable::parse(kInterpolationFixture, &warnings);
	EXPECT_TRUE(warnings.empty());
	ASSERT_EQ(table.fontCount(), 1u);
	EXPECT_EQ(table.ruleCount(), 4u);

	auto result = resolved(table, "Fixture Source", 9);
	EXPECT_EQ(result.name, "Example Sans");
	EXPECT_EQ(result.size, 10);

	result = resolved(table, "Fixture Source", 10);
	EXPECT_EQ(result.size, 10);

	result = resolved(table, "Fixture Source", 15);
	EXPECT_EQ(result.name, "Example Sans");
	EXPECT_EQ(result.size, 13);

	result = resolved(table, "Fixture Source", 22);
	EXPECT_EQ(result.name, "Example Sans");
	EXPECT_EQ(result.size, 17);

	result = resolved(table, "Fixture Source", 24);
	EXPECT_EQ(result.name, "Example Display");
	EXPECT_EQ(result.size, 18);

	result = resolved(table, "Fixture Source", 72);
	EXPECT_EQ(result.name, "Example Display");
	EXPECT_EQ(result.size, 36);
}

TEST(FontSubstitution, QuotedNamesAndCrLfMapSizes)
{
	const auto table = FontSubstitutionTable::parse(
		"FontSubstitution \"Fixture Script\"\r\n"
		"    Size 8 = 12 \"Example Display\"\r\n"
		"    Size 40 = 70 \"Example Display\"\r\n"
		"End\r\n"
		"// Map a family to its own pixel scale\r\n"
		"FontSubstitution \"Example Serif\"\r\n"
		"    Size 8 = 8 \"Example Serif\"\r\n"
		"    Size 40 = 36 \"Example Serif\"\r\n"
		"End\r\n");
	ASSERT_EQ(table.fontCount(), 2u);

	auto result = resolved(table, "fixture script", 8);
	EXPECT_EQ(result.name, "Example Display");
	EXPECT_EQ(result.size, 12);

	result = resolved(table, "Fixture Script", 24);
	EXPECT_EQ(result.size, 41);

	result = resolved(table, "Example Serif", 12);
	EXPECT_EQ(result.name, "Example Serif");
	EXPECT_EQ(result.size, 11);

	result = resolved(table, "Example Serif", 16);
	EXPECT_EQ(result.size, 15);

	result = resolved(table, "Example Serif", 40);
	EXPECT_EQ(result.size, 36);
}

TEST(FontSubstitution, UnknownFontsAreNotSubstituted)
{
	const auto table = FontSubstitutionTable::parse(kInterpolationFixture);
	FontRequest result;
	EXPECT_FALSE(table.resolve(FontRequest{"Arial", 12, false}, result));
	EXPECT_FALSE(FontSubstitutionTable().resolve(FontRequest{"Fixture Source", 12, false}, result));
}

TEST(FontSubstitution, ListsCurrentSubstitutionTargets)
{
	const auto table = FontSubstitutionTable::parse(
		"FontSubstitution \"Fixture Source\"\n"
		"  Size 10 = 12 \"Example Serif\"\n"
		"  Size 10 = 14 \"Example Sans\"\n"
		"  Size 20 = 24 \"Example Display\"\n"
		"End\n");

	EXPECT_EQ(table.targetNames(), (std::vector<std::string>{"Example Sans", "Example Display"}));
}

TEST(FontSubstitution, BoldModifiersOverrideTheRequest)
{
	const auto table = FontSubstitutionTable::parse(
		"FontSubstitution Sample\n"
		"  Size 10 = 10 +BOLD Arial\n"
		"  Size 20 = 20 -BOLD \"Arial Black\"\n"
		"  Size 30 = 30 Arial\n"
		"End\n");

	auto result = resolved(table, "Sample", 10, false);
	EXPECT_EQ(result.name, "Arial");
	EXPECT_TRUE(result.bold);

	result = resolved(table, "Sample", 25, true);
	EXPECT_EQ(result.name, "Arial Black");
	EXPECT_FALSE(result.bold);

	result = resolved(table, "Sample", 30, true);
	EXPECT_TRUE(result.bold);
}

TEST(FontSubstitution, UnsortedAndDuplicateSizesAreOrdered)
{
	const auto table = FontSubstitutionTable::parse(
		"FontSubstitution A\n"
		"  Size 40 = 36 B\n"
		"  Size 8 = 6 B\n"
		"  Size 8 = 8 B\n"
		"End\n");
	EXPECT_EQ(table.ruleCount(), 2u);
	EXPECT_EQ(resolved(table, "A", 8).size, 8);
	EXPECT_EQ(resolved(table, "A", 1).size, 8);
}

TEST(FontSubstitution, MalformedLinesWarnAndAreSkipped)
{
	std::vector<std::string> warnings;
	const auto table = FontSubstitutionTable::parse(
		"Size 8 = 8 Arial\n"
		"FontSubstitution\n"
		"FontSubstitution X\n"
		"  Size eight = 8 Arial\n"
		"  Size 8 8 Arial\n"
		"  Size 8 = 8\n"
		"  Color 8\n"
		"  Size 8 = 9 Arial\n",
		&warnings);
	EXPECT_EQ(warnings.size(), 7u);
	EXPECT_EQ(table.ruleCount(), 1u);
}

TEST(LocalFontFiles, ReadsEntriesInOrderWithoutComments)
{
	const auto files = parseLocalFontFiles(
		"Language\r\n"
		"  UnicodeFontName = \"Fixture Unicode\"\r\n"
		"  ; LocalFontFile = Ignored.ttf\r\n"
		"  LocalFontFile = \"Example Serif.otf\" ; display face\r\n"
		"  LocalFontFile = SampleUI.ttf // interface face\r\n"
		"  localfontfile = \"example serif.otf\"\r\n"
		"  LocalFontFileName = Unrelated.ttf\r\n"
		"End\r\n");
	ASSERT_EQ(files.size(), 2u);
	EXPECT_EQ(files[0], "Example Serif.otf");
	EXPECT_EQ(files[1], "SampleUI.ttf");
}
