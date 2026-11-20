#include "GameClient/RetailGameText.h"

#include <gtest/gtest.h>

namespace
{
	void appendInt(std::string& bytes, uint32_t value)
	{
		for (int shift = 0; shift < 32; shift += 8)
			bytes.push_back(static_cast<char>(value >> shift));
	}

	std::string csf()
	{
		std::string bytes = " FSC";
		for (uint32_t value : {3, 1, 1, 0, 0})
			appendInt(bytes, value);
		bytes += " LBL";
		appendInt(bytes, 1);
		appendInt(bytes, 8);
		bytes += "UI:Title";
		bytes += "WRTS";
		appendInt(bytes, 3);
		for (uint16_t codeUnit : {0x0041, 0xd83d, 0xde00})
		{
			bytes.push_back(static_cast<char>(~codeUnit));
			bytes.push_back(static_cast<char>((~static_cast<uint32_t>(codeUnit)) >> 8));
		}
		appendInt(bytes, 4);
		bytes += "line";
		return bytes;
	}
}

TEST(RetailGameText, ReadsVersionThreeCsfWithSpeechAndUtf16)
{
	RetailGameText::Catalogue catalogue;
	ASSERT_TRUE(RetailGameText::parseCsf(csf(), catalogue));
	const auto& entry = catalogue.at("ui:title");
	EXPECT_EQ(entry.label, "UI:Title");
	EXPECT_EQ(entry.text, L"A\U0001f600");
	EXPECT_EQ(entry.speech, "line");
}

TEST(RetailGameText, ReadsVersionZeroCsf)
{
	auto bytes = csf();
	bytes[4] = 0;
	RetailGameText::Catalogue catalogue;
	ASSERT_TRUE(RetailGameText::parseCsf(bytes, catalogue));
	EXPECT_EQ(catalogue.at("ui:title").text, L"A\U0001f600");
}

TEST(RetailGameText, RejectsTruncatedAndOversizedCsfWithoutPublishingPartialEntries)
{
	const auto valid = csf();
	RetailGameText::Catalogue catalogue;
	for (size_t length = 0; length < valid.size(); ++length)
		EXPECT_FALSE(RetailGameText::parseCsf(std::string_view(valid).substr(0, length), catalogue));

	for (size_t field : {4, 8, 12, 16, 20, 28, 32, 48, 58})
	{
		auto invalid = valid;
		invalid.replace(field, 4, 4, '\xff');
		EXPECT_FALSE(RetailGameText::parseCsf(invalid, catalogue)) << field;
	}
	EXPECT_TRUE(catalogue.empty());
}

TEST(RetailGameText, ReadsStrCommentsEscapesAndCaseInsensitiveDuplicates)
{
	RetailGameText::Catalogue catalogue;
	ASSERT_TRUE(RetailGameText::parseStr(
		"; heading\r\nUI:Title\r\n// comment\r\n\"Caf\xc3\xa9\\nA \\\"quote\\\"\"\r\nEnd\r\n"
		"ui:TITLE\n\"later\"\nEND\n"
		"UI:Other\n\"separate\"\nEnd\n", catalogue));
	ASSERT_EQ(catalogue.size(), 2u);
	EXPECT_EQ(catalogue.at("ui:title").text, L"Caf\u00e9\nA \"quote\"");
	EXPECT_EQ(catalogue.at("ui:other").text, L"separate");
}

TEST(RetailGameText, BoundsStrRecordsAndKeepsFollowingValidEntries)
{
	RetailGameText::Catalogue catalogue;
	const std::string str = "UI:Oversized\n\"" + std::string(65536, 'a') + "\"\nEnd\n"
		"UI:Valid\n\"good\"\nEnd\nUI:Broken\n\"unfinished";
	EXPECT_FALSE(RetailGameText::parseStr(str, catalogue));
	ASSERT_EQ(catalogue.size(), 1u);
	EXPECT_EQ(catalogue.at("ui:valid").text, L"good");
}

TEST(RetailGameText, AcceptsRetailLabelsWithSpacesAndStopsAtTheLastQuote)
{
	RetailGameText::Catalogue catalogue;
	ASSERT_TRUE(RetailGameText::parseStr(
		"Map:Two Words\n\"one \"two\" three\"\nEnd\n"
		"BareLabel\n\"value\\\"\nEnd\n", catalogue));
	EXPECT_EQ(catalogue.at("map:two words").text, L"one \"two\" three");
	EXPECT_EQ(catalogue.at("barelabel").text, L"value");
}
