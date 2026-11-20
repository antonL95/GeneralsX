#include "Common/CommandPointRules.h"

#include <gtest/gtest.h>

namespace
{

CommandPointRules::Config makeAotRConfig()
{
	CommandPointRules::Config cfg;
	cfg.goodLimit = 300;
	cfg.evilLimit = 600;
	cfg.good = {100, 150};
	cfg.evil = {300, 350};
	cfg.goodBonus = 20;
	cfg.evilBonus = 50;
	cfg.goodAI = {600, 650};
	cfg.evilAI = {600, 650};
	const int caps[7] = {1000, 875, 750, 675, 625, 575, 500};
	for (int i = 0; i < 7; ++i)
	{
		cfg.goodMP[i] = {100, caps[i]};
		cfg.evilMP[i] = {100, caps[i]};
	}
	return cfg;
}

}

TEST(CommandPointRules, ResolvesMultiplayerPlayerCountsAndClampsRange)
{
	const auto cfg = makeAotRConfig();
	auto p1 = CommandPointRules::resolveBaseAndCap(cfg, true, 1, false, false);
	EXPECT_EQ(p1.starting, 100);
	EXPECT_EQ(p1.maximum, 1000);

	auto p2 = CommandPointRules::resolveBaseAndCap(cfg, true, 2, false, false);
	EXPECT_EQ(p2.starting, 100);
	EXPECT_EQ(p2.maximum, 1000);

	auto p3 = CommandPointRules::resolveBaseAndCap(cfg, true, 3, true, false);
	EXPECT_EQ(p3.starting, 100);
	EXPECT_EQ(p3.maximum, 875);

	auto p4 = CommandPointRules::resolveBaseAndCap(cfg, true, 4, false, false);
	EXPECT_EQ(p4.maximum, 750);

	auto p5 = CommandPointRules::resolveBaseAndCap(cfg, true, 5, false, false);
	EXPECT_EQ(p5.maximum, 675);

	auto p6 = CommandPointRules::resolveBaseAndCap(cfg, true, 6, true, false);
	EXPECT_EQ(p6.maximum, 625);

	auto p7 = CommandPointRules::resolveBaseAndCap(cfg, true, 7, false, false);
	EXPECT_EQ(p7.maximum, 575);

	auto p8 = CommandPointRules::resolveBaseAndCap(cfg, true, 8, true, false);
	EXPECT_EQ(p8.maximum, 500);

	auto p12 = CommandPointRules::resolveBaseAndCap(cfg, true, 12, false, false);
	EXPECT_EQ(p12.maximum, 500);

	auto scaled = CommandPointRules::resolveBaseAndCap(cfg, true, 2, false, false, 150);
	EXPECT_EQ(scaled.starting, 100);
	EXPECT_EQ(scaled.maximum, 1500);
}

TEST(CommandPointRules, ResolvesSinglePlayerAndAIBases)
{
	const auto cfg = makeAotRConfig();
	auto goodSP = CommandPointRules::resolveBaseAndCap(cfg, false, 2, false, false);
	EXPECT_EQ(goodSP.starting, 100);
	EXPECT_EQ(goodSP.maximum, 150);

	auto evilSP = CommandPointRules::resolveBaseAndCap(cfg, false, 2, true, false);
	EXPECT_EQ(evilSP.starting, 300);
	EXPECT_EQ(evilSP.maximum, 350);

	auto goodAI = CommandPointRules::resolveBaseAndCap(cfg, false, 2, false, true);
	EXPECT_EQ(goodAI.starting, 600);
	EXPECT_EQ(goodAI.maximum, 650);

	auto evilAI = CommandPointRules::resolveBaseAndCap(cfg, false, 2, true, true);
	EXPECT_EQ(evilAI.starting, 600);
	EXPECT_EQ(evilAI.maximum, 650);
}

TEST(CommandPointRules, ComputesTotalLimitWithBonusesAndCap)
{
	EXPECT_EQ(CommandPointRules::computeTotalLimit(100, 100, 0, 1000), 200);
	EXPECT_EQ(CommandPointRules::computeTotalLimit(100, 150, 100, 1000), 350);
	EXPECT_EQ(CommandPointRules::computeTotalLimit(100, 950, 100, 1000), 1000);
	EXPECT_EQ(CommandPointRules::computeTotalLimit(100, 50, 0, 0), 150);
	EXPECT_EQ(CommandPointRules::computeTotalLimit(0, -10, 0, 100), 0);
}

TEST(CommandPointRules, CountsOnlySelectableNonStructureNonHordeObjects)
{
	EXPECT_EQ(CommandPointRules::objectUsedCommandPoints(5, true, false, false), 5);
	EXPECT_EQ(CommandPointRules::objectUsedCommandPoints(75, true, false, true), 0);
	EXPECT_EQ(CommandPointRules::objectUsedCommandPoints(10, true, true, false), 0);
	EXPECT_EQ(CommandPointRules::objectUsedCommandPoints(5, false, false, false), 0);
	EXPECT_EQ(CommandPointRules::objectUsedCommandPoints(0, true, false, false), 0);
	EXPECT_EQ(CommandPointRules::objectUsedCommandPoints(-5, true, false, false), 0);
}

TEST(CommandPointRules, ChecksAffordabilityAndArmyOfDeadBypass)
{
	EXPECT_TRUE(CommandPointRules::canAffordCommandPoints(0, 200, 75, false));
	EXPECT_TRUE(CommandPointRules::canAffordCommandPoints(125, 200, 75, false));
	EXPECT_FALSE(CommandPointRules::canAffordCommandPoints(150, 200, 75, false));
	EXPECT_TRUE(CommandPointRules::canAffordCommandPoints(200, 200, 0, false));
	EXPECT_TRUE(CommandPointRules::canAffordCommandPoints(200, 200, 75, true));
}
