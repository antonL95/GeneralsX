#include <cmath>

#include <gtest/gtest.h>

#include "GameLogic/WeaponNuggetRules.h"

namespace
{
WeaponNuggetRules::FilterFacts facts()
{
	return {};
}
}

TEST(WeaponNuggetRulesTest, NoneWithKindsMatchesOnlyThoseKinds)
{
	auto hero = facts();
	hero.ruleNone = true;
	hero.hasIncludedKinds = true;
	hero.includedByKind = true;
	EXPECT_TRUE(WeaponNuggetRules::filterMatches(hero));

	auto infantry = hero;
	infantry.includedByKind = false;
	EXPECT_FALSE(WeaponNuggetRules::filterMatches(infantry));

	auto nothing = facts();
	nothing.ruleNone = true;
	EXPECT_FALSE(WeaponNuggetRules::filterMatches(nothing));
}

TEST(WeaponNuggetRulesTest, AllMatchesEverythingNotExcluded)
{
	auto all = facts();
	all.ruleAll = true;
	EXPECT_TRUE(WeaponNuggetRules::filterMatches(all));

	auto machine = all;
	machine.excludedByKind = true;
	EXPECT_FALSE(WeaponNuggetRules::filterMatches(machine));

	auto named = all;
	named.excludedByName = true;
	EXPECT_FALSE(WeaponNuggetRules::filterMatches(named));
}

TEST(WeaponNuggetRulesTest, NamesWinOverKinds)
{
	auto carrion = facts();
	carrion.hasIncludedKinds = true;
	carrion.excludedByKind = true;
	carrion.includedByName = true;
	EXPECT_TRUE(WeaponNuggetRules::filterMatches(carrion));

	auto sauron = facts();
	sauron.hasIncludedKinds = true;
	sauron.includedByKind = true;
	sauron.excludedByName = true;
	EXPECT_FALSE(WeaponNuggetRules::filterMatches(sauron));

	auto onlyNames = facts();
	onlyNames.ruleNone = true;
	onlyNames.hasIncludedNames = true;
	EXPECT_FALSE(WeaponNuggetRules::filterMatches(onlyNames));
}

TEST(WeaponNuggetRulesTest, RelationshipsRestrictTheMatch)
{
	auto allies = facts();
	allies.ruleAll = true;
	allies.wantsAllies = true;
	allies.isAlly = true;
	EXPECT_TRUE(WeaponNuggetRules::filterMatches(allies));

	auto enemy = allies;
	enemy.isAlly = false;
	enemy.isEnemy = true;
	EXPECT_FALSE(WeaponNuggetRules::filterMatches(enemy));

	auto either = enemy;
	either.wantsEnemies = true;
	EXPECT_TRUE(WeaponNuggetRules::filterMatches(either));

	auto anyone = facts();
	anyone.isNeutral = true;
	EXPECT_TRUE(WeaponNuggetRules::filterMatches(anyone));
}

TEST(WeaponNuggetRulesTest, UpgradeGates)
{
	EXPECT_TRUE(WeaponNuggetRules::upgradeGateOpen(false, false, false));
	EXPECT_FALSE(WeaponNuggetRules::upgradeGateOpen(true, false, false));
	EXPECT_TRUE(WeaponNuggetRules::upgradeGateOpen(true, true, false));
	EXPECT_FALSE(WeaponNuggetRules::upgradeGateOpen(false, false, true));
	EXPECT_FALSE(WeaponNuggetRules::upgradeGateOpen(true, true, true));
}

TEST(WeaponNuggetRulesTest, ArcIsTheTotalWidthAroundTheFacing)
{
	const float cos45 = std::cos(45.0f * 3.14159265f / 180.0f);
	EXPECT_TRUE(WeaponNuggetRules::withinArc(1.0f, 0.0f, 10.0f, 9.0f, cos45, false, false));
	EXPECT_FALSE(WeaponNuggetRules::withinArc(1.0f, 0.0f, 10.0f, 11.0f, cos45, false, false));
	EXPECT_FALSE(WeaponNuggetRules::withinArc(1.0f, 0.0f, -10.0f, 0.0f, cos45, false, false));
	EXPECT_TRUE(WeaponNuggetRules::withinArc(1.0f, 0.0f, -10.0f, 0.0f, cos45, false, true));
	EXPECT_TRUE(WeaponNuggetRules::withinArc(1.0f, 0.0f, -10.0f, 0.0f, cos45, true, false));
	EXPECT_TRUE(WeaponNuggetRules::withinArc(1.0f, 0.0f, 0.0f, 0.0f, cos45, false, false));
	EXPECT_TRUE(WeaponNuggetRules::isFullArc(0.0f));
	EXPECT_TRUE(WeaponNuggetRules::isFullArc(360.0f));
	EXPECT_FALSE(WeaponNuggetRules::isFullArc(180.0f));
}

TEST(WeaponNuggetRulesTest, MatchingScalarsMultiply)
{
	EXPECT_FLOAT_EQ(WeaponNuggetRules::scalarProduct({}), 1.0f);
	EXPECT_FLOAT_EQ(WeaponNuggetRules::scalarProduct({{500.0f, true}, {0.5f, false}}), 500.0f);
	EXPECT_FLOAT_EQ(WeaponNuggetRules::scalarProduct({{2.0f, true}, {0.5f, true}}), 1.0f);
	EXPECT_FLOAT_EQ(WeaponNuggetRules::scalarProduct({{0.0f, true}, {2.0f, true}}), 0.0f);
}

TEST(WeaponNuggetRulesTest, DotTicksCoverTheDuration)
{
	const auto poison = WeaponNuggetRules::dotTickOffsets(0, 30, 450);
	ASSERT_EQ(poison.size(), 15u);
	EXPECT_EQ(poison.front(), 0u);
	EXPECT_EQ(poison.back(), 420u);

	const auto delayed = WeaponNuggetRules::dotTickOffsets(6, 15, 31);
	ASSERT_EQ(delayed.size(), 3u);
	EXPECT_EQ(delayed[2], 36u);

	EXPECT_EQ(WeaponNuggetRules::dotTickOffsets(4, 0, 0), std::vector<unsigned int>{4u});
	EXPECT_EQ(WeaponNuggetRules::dotTickOffsets(0, 30, 0), std::vector<unsigned int>{0u});
}

TEST(WeaponNuggetRulesTest, ProjectileSpeedClampsOnlyWithAMaximum)
{
	EXPECT_FLOAT_EQ(WeaponNuggetRules::clampProjectileSpeed(16.0f, 0.0f), 16.0f);
	EXPECT_FLOAT_EQ(WeaponNuggetRules::clampProjectileSpeed(18.0f, 16.03f), 16.03f);
	EXPECT_FLOAT_EQ(WeaponNuggetRules::clampProjectileSpeed(9.0f, 16.03f), 9.0f);
}

TEST(WeaponNuggetRulesTest, HitPercentageRolls)
{
	EXPECT_FALSE(WeaponNuggetRules::needsHitRoll(100.0f));
	EXPECT_TRUE(WeaponNuggetRules::needsHitRoll(70.0f));
	EXPECT_TRUE(WeaponNuggetRules::rollHits(69, 70.0f));
	EXPECT_FALSE(WeaponNuggetRules::rollHits(70, 70.0f));
	EXPECT_FALSE(WeaponNuggetRules::rollHits(0, 0.0f));
	int hits = 0;
	for (int roll = 0; roll < 100; ++roll)
		hits += WeaponNuggetRules::rollHits(roll, 77.0f) ? 1 : 0;
	EXPECT_EQ(hits, 77);
}
