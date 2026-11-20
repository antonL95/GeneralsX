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
