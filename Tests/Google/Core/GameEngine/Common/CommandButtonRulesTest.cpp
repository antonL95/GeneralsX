#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "GameClient/CommandButtonRules.h"

namespace
{
const char* const ZeroHourNames[] = { "NEED_TARGET_ENEMY_OBJECT", "NEED_UPGRADE", "OK_FOR_MULTI_SELECT", nullptr };
}

TEST(CommandButtonRules, SplitsZeroHourAndRotwkOptions)
{
	const CommandButtonRules::OptionBits bits = CommandButtonRules::splitOptions(
		{ "HIDE_WHILE_DISABLED", "CANCELABLE", "NEED_UPGRADE" }, ZeroHourNames);
	EXPECT_EQ(bits.zeroHour, 1u << 1);
	EXPECT_EQ(bits.rotwk, static_cast<unsigned int>(CommandButtonRules::ROTWK_HIDE_WHILE_DISABLED | CommandButtonRules::ROTWK_CANCELABLE));
	EXPECT_TRUE(bits.unknown.empty());
}

TEST(CommandButtonRules, OptionNamesAreCaseInsensitiveAndUnknownIsReported)
{
	const CommandButtonRules::OptionBits bits = CommandButtonRules::splitOptions({ "ok_for_multi_select", "BOGUS" }, ZeroHourNames);
	EXPECT_EQ(bits.zeroHour, 1u << 2);
	ASSERT_EQ(bits.unknown.size(), 1u);
	EXPECT_EQ(bits.unknown[0], "BOGUS");
}

TEST(CommandButtonRules, RotwkOptionTableMatchesBits)
{
	EXPECT_EQ(CommandButtonRules::nameIndex(CommandButtonRules::rotwkOptionNames(), "HIDE_WHILE_DISABLED"), 4);
	EXPECT_EQ(CommandButtonRules::nameIndex(CommandButtonRules::rotwkOptionNames(), "CANCELABLE"), 13);
	EXPECT_EQ(1u << 13, static_cast<unsigned int>(CommandButtonRules::ROTWK_CANCELABLE));
}

TEST(CommandButtonRules, RotwkCommandNames)
{
	const char* const* names = CommandButtonRules::rotwkCommandNames();
	int count = 0;
	while (names[count])
		++count;
	EXPECT_EQ(count, 30);
	EXPECT_EQ(CommandButtonRules::nameIndex(names, "REVIVE"), 17);
	EXPECT_GE(CommandButtonRules::nameIndex(names, "push_visible_command_range"), 0);
	EXPECT_EQ(CommandButtonRules::nameIndex(names, "UNIT_BUILD"), -1);
	EXPECT_EQ(CommandButtonRules::nameIndex(names, "REVIVEX"), -1);
}

TEST(CommandButtonRules, SideColumnFollowsInitialVisibleWithoutRange)
{
	const CommandButtonRules::VisibleRange none;
	EXPECT_TRUE(CommandButtonRules::inSideColumn(6, none, 7));
	EXPECT_FALSE(CommandButtonRules::inSideColumn(7, none, 7));
	EXPECT_TRUE(CommandButtonRules::inSideColumn(20, none, 0));
}

TEST(CommandButtonRules, PushedRangeReplacesTheMainMenu)
{
	const CommandButtonRules::VisibleRange heroes = CommandButtonRules::pushRange(14, 15);
	EXPECT_TRUE(heroes.active());
	EXPECT_FALSE(CommandButtonRules::inSideColumn(1, heroes, 7));
	EXPECT_TRUE(CommandButtonRules::inSideColumn(14, heroes, 7));
	EXPECT_TRUE(CommandButtonRules::inSideColumn(24, heroes, 7));
	EXPECT_FALSE(CommandButtonRules::inSideColumn(29, heroes, 7));
	EXPECT_FALSE(CommandButtonRules::pushRange(40, 2).active());
	EXPECT_FALSE(CommandButtonRules::pushRange(3, 0).active());
}

TEST(CommandButtonRules, VisibilityAndClickabilityGateInteractivity)
{
	EXPECT_TRUE(CommandButtonRules::isButtonInteractive(true, true, false));
	EXPECT_FALSE(CommandButtonRules::isButtonInteractive(false, true, false));
	EXPECT_FALSE(CommandButtonRules::isButtonInteractive(true, false, false));
	EXPECT_FALSE(CommandButtonRules::isButtonInteractive(true, true, true));
}

TEST(CommandButtonRules, DoubleClickRequiresSecondClickWithinWindow)
{
	EXPECT_TRUE(CommandButtonRules::isDoubleClickSatisfied(false, false, 0, 10, 15));
	EXPECT_FALSE(CommandButtonRules::isDoubleClickSatisfied(true, false, 0, 10, 15));
	EXPECT_TRUE(CommandButtonRules::isDoubleClickSatisfied(true, false, 10, 20, 15));
	EXPECT_FALSE(CommandButtonRules::isDoubleClickSatisfied(true, false, 10, 30, 15));
	EXPECT_TRUE(CommandButtonRules::isDoubleClickSatisfied(true, true, 0, 30, 15));
}

TEST(CommandButtonRules, AutoAbilityRespectsConditionAndAutoDelay)
{
	EXPECT_EQ(CommandButtonRules::autoDelayFrames(0.0f, 30), 0u);
	EXPECT_EQ(CommandButtonRules::autoDelayFrames(1.5f, 30), 45u);
	EXPECT_FALSE(CommandButtonRules::isAutoAbilityAllowed(true, true, 0, 10, 100));
	EXPECT_FALSE(CommandButtonRules::isAutoAbilityAllowed(false, false, 0, 10, 100));
	EXPECT_FALSE(CommandButtonRules::isAutoAbilityAllowed(false, true, 30, 10, 39));
	EXPECT_TRUE(CommandButtonRules::isAutoAbilityAllowed(false, true, 30, 10, 40));
}

TEST(CommandButtonRules, ClickReactionEscalatesAndRespectsActiveCountdown)
{
	const int frames[5] = { 10, 12, 14, 16, 20 };
	const CommandButtonRules::ClickReactionStep first =
		CommandButtonRules::advanceClickReaction(0, 0, 600, frames, 2);
	EXPECT_EQ(first.nextTimer, 600);
	EXPECT_EQ(first.triggeredLevel, 2);
	EXPECT_EQ(first.nextCountdown, 12);

	const CommandButtonRules::ClickReactionStep busy =
		CommandButtonRules::advanceClickReaction(600, 8, 600, frames, 3);
	EXPECT_EQ(busy.nextTimer, 1200);
	EXPECT_EQ(busy.triggeredLevel, 0);
	EXPECT_EQ(busy.nextCountdown, 8);

	const CommandButtonRules::ClickReactionStep high =
		CommandButtonRules::advanceClickReaction(1800, 0, 600, frames, 1);
	EXPECT_EQ(high.nextTimer, 2400);
	EXPECT_EQ(high.triggeredLevel, 4);
	EXPECT_EQ(high.nextCountdown, 16);

	const CommandButtonRules::ClickReactionStep capped =
		CommandButtonRules::advanceClickReaction(3000, 0, 600, frames, 1);
	EXPECT_EQ(capped.nextTimer, 3000);
	EXPECT_EQ(capped.triggeredLevel, 5);
	EXPECT_EQ(capped.nextCountdown, 20);
}
