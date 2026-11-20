#include "GameClient/UiLayoutDump.h"
#include "GameClient/UiScale.h"

#include <gtest/gtest.h>

namespace
{
	UiWindowRecord record(const char* name, const char* parent, int x, int y)
	{
		UiWindowRecord window;
		window.name = name;
		window.parentPath = parent;
		window.type = "USER";
		window.screen[0] = x;
		window.screen[1] = y;
		window.screen[2] = 10;
		window.screen[3] = 10;
		return window;
	}
}

TEST(UiScale, UniformScaleUsesTheSmallerAxis)
{
	EXPECT_DOUBLE_EQ(UiScale::scale(1024, 768), 1.0);
	EXPECT_DOUBLE_EQ(UiScale::scale(1920, 1080), 1.40625);
	EXPECT_DOUBLE_EQ(UiScale::scale(2560, 1440), 1.875);
	EXPECT_DOUBLE_EQ(UiScale::scale(1600, 1200), 1.5625);
	EXPECT_DOUBLE_EQ(UiScale::scale(0, 768), 1.0);
}

TEST(UiScale, FontPixelSizeRoundsOnce)
{
	EXPECT_EQ(UiScale::fontPixelSize(14, 1024, 768), 14);
	EXPECT_EQ(UiScale::fontPixelSize(14, 3840, 2160), 39);
	EXPECT_EQ(UiScale::fontPixelSize(16, 1920, 1080), 23);
	EXPECT_EQ(UiScale::fontPixelSize(16, 2560, 1440), 30);
	EXPECT_EQ(UiScale::fontPixelSize(1, 100, 100), 1);
}

TEST(UiScale, CanvasIsCenteredAndRoundedByEdges)
{
	auto rect = UiScale::canvasToScreen(0, 0, 1024, 768, 1920, 1080);
	EXPECT_EQ(rect.x0, 240);
	EXPECT_EQ(rect.y0, 0);
	EXPECT_EQ(rect.x1, 1680);
	EXPECT_EQ(rect.y1, 1080);

	const auto left = UiScale::canvasToScreen(10, 10, 21, 20, 1920, 1080);
	const auto right = UiScale::canvasToScreen(21, 10, 33, 20, 1920, 1080);
	EXPECT_EQ(left.x1, right.x0);

	rect = UiScale::canvasToScreen(32, 64, 992, 128, 1024, 768);
	EXPECT_EQ(rect.x0, 32);
	EXPECT_EQ(rect.y1, 128);
}

TEST(UiLayoutDump, OutputIsSortedIndependentOfInputOrder)
{
	UiLayoutSummary summary{1024, 768, 1.0, "Menus/Test.wnd"};
	std::vector<UiWindowRecord> a = {record("B", "", 5, 5), record("A", "", 0, 0), record("C", "A", 1, 1)};
	std::vector<UiWindowRecord> b = {a[2], a[0], a[1]};
	const std::string first = formatUiLayoutJson(summary, a);
	EXPECT_EQ(first, formatUiLayoutJson(summary, b));
	EXPECT_LT(first.find("\"name\": \"A\""), first.find("\"name\": \"C\""));
	EXPECT_LT(first.find("\"name\": \"C\""), first.find("\"name\": \"B\""));
	EXPECT_NE(first.find("\"windowCount\": 3"), std::string::npos);
}

TEST(UiLayoutDump, UnnamedSiblingsSortByRectangle)
{
	UiLayoutSummary summary{800, 600, 0.78125, ""};
	const std::string json = formatUiLayoutJson(summary, {record("", "P", 0, 50), record("", "P", 0, 10)});
	EXPECT_LT(json.find("[0, 10, 10, 10]"), json.find("[0, 50, 10, 10]"));
	EXPECT_NE(json.find("\"scale\": 0.781250"), std::string::npos);
}

TEST(UiLayoutDump, WritesFontsLayoutUnitsAndEscapes)
{
	UiWindowRecord window = record("Menu.wnd:\"Quote\"\\", "", 1, 2);
	window.hasLayoutRect = true;
	window.layoutRect[0] = 10;
	window.layoutRect[1] = 20;
	window.layoutRect[2] = 110;
	window.layoutRect[3] = 40;
	window.layoutResolution[0] = 800;
	window.layoutResolution[1] = 600;
	window.hasFont = true;
	window.fontName = "Albertus MT";
	window.fontSize = 23;
	window.fontPixels = true;
	window.fontHeight = 27;
	window.hidden = true;
	window.visible = false;

	const std::string json = formatUiLayoutJson(UiLayoutSummary{1920, 1080, 1.40625, "x"}, {window});
	EXPECT_NE(json.find("\"name\": \"Menu.wnd:\\\"Quote\\\"\\\\\""), std::string::npos);
	EXPECT_NE(json.find("\"layoutRect\": [10, 20, 110, 40], \"layoutResolution\": [800, 600]"), std::string::npos);
	EXPECT_NE(json.find("\"font\": {\"name\": \"Albertus MT\", \"size\": 23, \"unit\": \"px\", \"bold\": false, \"height\": 27}"),
		std::string::npos);
	EXPECT_NE(json.find("\"hidden\": true, \"visible\": false"), std::string::npos);
	EXPECT_EQ(uiJsonEscape(std::string("\x01", 1)), "\\u0001");
}

TEST(UiLayoutDump, EmptyDumpIsValid)
{
	EXPECT_EQ(formatUiLayoutJson(UiLayoutSummary{1024, 768, 1.0, ""}, {}),
		"{\n  \"resolution\": [1024, 768],\n  \"scale\": 1.000000,\n  \"layout\": \"\",\n  \"windowCount\": 0,\n  \"windows\": []\n}\n");
}

TEST(UiSpecimen, ParsesFontSizeAndText)
{
	const auto lines = parseUiSpecimen(" Albertus MT ;16; GAME SETUP |Omnia LT Std;30;skirmish|bad|x;0;y|y;3;|z;4;a;b");
	ASSERT_EQ(lines.size(), 3u);
	EXPECT_EQ(lines[0].font, "Albertus MT");
	EXPECT_EQ(lines[0].designSize, 16);
	EXPECT_EQ(lines[0].text, "GAME SETUP");
	EXPECT_EQ(lines[1].font, "Omnia LT Std");
	EXPECT_EQ(lines[2].text, "a;b");
}

TEST(UiFrameSpecimen, ParsesFrameNamesAndDesignRects)
{
	const auto frames = parseUiFrameSpecimen(" MenuButtonMed ; 24;-8 ;170;52|MapPreviewFrame;0;0;0;10|bad;1;2;3|x;1;2;3;4;5|y;a;1;1;1|PopupChainSide;40;381;42;200");
	ASSERT_EQ(frames.size(), 2u);
	EXPECT_EQ(frames[0].frame, "MenuButtonMed");
	EXPECT_EQ(frames[0].x, 24);
	EXPECT_EQ(frames[0].y, -8);
	EXPECT_EQ(frames[0].width, 170);
	EXPECT_EQ(frames[0].height, 52);
	EXPECT_EQ(frames[1].frame, "PopupChainSide");
	EXPECT_EQ(frames[1].height, 200);
}
