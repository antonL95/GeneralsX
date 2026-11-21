#include <gtest/gtest.h>

#include "GameClient/UiHudRules.h"

namespace
{
int charWidth(const std::wstring& text)
{
	return static_cast<int>(text.size()) * 10;
}
}

TEST(UiHudRulesTest, HealthStageThresholds)
{
	EXPECT_EQ(UiHudRules::healthStage(1.0), UiHudRules::HealthStage::Green);
	EXPECT_EQ(UiHudRules::healthStage(0.51), UiHudRules::HealthStage::Green);
	EXPECT_EQ(UiHudRules::healthStage(0.5), UiHudRules::HealthStage::Yellow);
	EXPECT_EQ(UiHudRules::healthStage(0.26), UiHudRules::HealthStage::Yellow);
	EXPECT_EQ(UiHudRules::healthStage(0.25), UiHudRules::HealthStage::Red);
	EXPECT_EQ(UiHudRules::healthStage(0.0), UiHudRules::HealthStage::Red);
}

TEST(UiHudRulesTest, TooltipLinesDropLeadingSpacesAndBlankLines)
{
	const std::wstring text = L"Requires Level 2 \n Veterans and nearby allied units gain\n\n  \n+25% damage\nLeft click to activate";
	const std::vector<std::wstring> lines = UiHudRules::tooltipLines(text);
	ASSERT_EQ(lines.size(), 4u);
	EXPECT_EQ(lines[0], L"Requires Level 2");
	EXPECT_EQ(lines[1], L"Veterans and nearby allied units gain");
	EXPECT_EQ(lines[2], L"+25% damage");
	EXPECT_EQ(lines[3], L"Left click to activate");
}

TEST(UiHudRulesTest, TooltipLinesOfEmptyText)
{
	EXPECT_TRUE(UiHudRules::tooltipLines(L"").empty());
	EXPECT_TRUE(UiHudRules::tooltipLines(L" \n \n").empty());
}

TEST(UiHudRulesTest, WrapKeepsWordsAndHasNoLeadingSpace)
{
	const std::vector<std::wstring> lines = UiHudRules::wrapLine(L"Click here to select the nearest builder who is ready to work", 200, charWidth);
	ASSERT_EQ(lines.size(), 3u);
	EXPECT_EQ(lines[0], L"Click here to select");
	EXPECT_EQ(lines[1], L"the nearest builder");
	EXPECT_EQ(lines[2], L"who is ready to work");
	for (const std::wstring& line : lines)
	{
		EXPECT_LE(charWidth(line), 200);
		EXPECT_NE(line.front(), L' ');
	}
}

TEST(UiHudRulesTest, WrapPutsLongWordOnItsOwnLine)
{
	const std::vector<std::wstring> lines = UiHudRules::wrapLine(L"a verylongwordthatdoesnotfit b", 50, charWidth);
	ASSERT_EQ(lines.size(), 3u);
	EXPECT_EQ(lines[0], L"a");
	EXPECT_EQ(lines[1], L"verylongwordthatdoesnotfit");
	EXPECT_EQ(lines[2], L"b");
}

TEST(UiHudRulesTest, ClockText)
{
	EXPECT_EQ(UiHudRules::clockText(0), L"00:00");
	EXPECT_EQ(UiHudRules::clockText(380), L"06:20");
	EXPECT_EQ(UiHudRules::clockText(3599), L"59:59");
	EXPECT_EQ(UiHudRules::clockText(6000), L"100:00");
}

TEST(UiHudRulesTest, ReflowKeepsOrderAppendsNewAndDropsGone)
{
	std::vector<int> order = { 7, 3, 9 };
	EXPECT_FALSE(UiHudRules::reflow(order, std::vector<int>{ 9, 7, 3 }));
	EXPECT_EQ(order, (std::vector<int>{ 7, 3, 9 }));
	EXPECT_TRUE(UiHudRules::reflow(order, std::vector<int>{ 9, 7, 3, 4 }));
	EXPECT_EQ(order, (std::vector<int>{ 7, 3, 9, 4 }));
	EXPECT_TRUE(UiHudRules::reflow(order, std::vector<int>{ 4, 7 }));
	EXPECT_EQ(order, (std::vector<int>{ 7, 4 }));
}

TEST(UiHudRulesTest, BuilderOrderPrefersIdleThenNearest)
{
	const std::vector<unsigned int> order = UiHudRules::builderOrder({
		{ 100.0, 5, false },
		{ 400.0, 2, true },
		{ 50.0, 9, false },
		{ 400.0, 1, true },
	});
	EXPECT_EQ(order, (std::vector<unsigned int>{ 1, 2, 9, 5 }));
}

TEST(UiHudRulesTest, NextBuilderCyclesOnlyWithinTimeout)
{
	const std::vector<unsigned int> order = { 4, 8, 6 };
	EXPECT_EQ(UiHudRules::nextBuilder(order, 8, false), 0);
	EXPECT_EQ(UiHudRules::nextBuilder(order, 8, true), 2);
	EXPECT_EQ(UiHudRules::nextBuilder(order, 6, true), 0);
	EXPECT_EQ(UiHudRules::nextBuilder(order, 99, true), 0);
	EXPECT_EQ(UiHudRules::nextBuilder({}, 4, true), -1);
}

TEST(UiHudRulesTest, PulseAndFade)
{
	EXPECT_DOUBLE_EQ(UiHudRules::pulse(0, 400), 0.0);
	EXPECT_DOUBLE_EQ(UiHudRules::pulse(200, 400), 1.0);
	EXPECT_DOUBLE_EQ(UiHudRules::pulse(300, 400), 0.5);
	EXPECT_DOUBLE_EQ(UiHudRules::fadeAlpha(1000, 2000, 500), 1.0);
	EXPECT_DOUBLE_EQ(UiHudRules::fadeAlpha(2250, 2000, 500), 0.5);
	EXPECT_DOUBLE_EQ(UiHudRules::fadeAlpha(2600, 2000, 500), 0.0);
}
