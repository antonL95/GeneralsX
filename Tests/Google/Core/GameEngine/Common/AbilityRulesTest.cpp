#include <gtest/gtest.h>

#include "Common/AbilityRules.h"

using namespace AbilityRules;

TEST(AbilityRules, MountedConditionsGateAbilities)
{
	EXPECT_TRUE(conditionsAllow(0, 0, 0));
	EXPECT_TRUE(conditionsAllow(ABILITY_CONDITION_MOUNTED, 0, ABILITY_CONDITION_MOUNTED));
	EXPECT_FALSE(conditionsAllow(ABILITY_CONDITION_MOUNTED, 0, 0));
	EXPECT_FALSE(conditionsAllow(0, ABILITY_CONDITION_MOUNTED, ABILITY_CONDITION_MOUNTED));
	EXPECT_TRUE(conditionsAllow(0, ABILITY_CONDITION_WEAPON_TOGGLE, ABILITY_CONDITION_MOUNTED));
}

TEST(AbilityRules, QueryAllNeedsEveryIncludedKind)
{
	QueryCandidate candidate;
	candidate.includeKinds = 2;
	candidate.includeKindsHit = 1;
	EXPECT_FALSE(queryIncludes(QueryMode::All, candidate));
	EXPECT_TRUE(queryIncludes(QueryMode::Any, candidate));
	candidate.includeKindsHit = 2;
	EXPECT_TRUE(queryIncludes(QueryMode::All, candidate));
}

TEST(AbilityRules, QueryWithoutIncludesMatchesEverythingButNone)
{
	QueryCandidate candidate;
	EXPECT_TRUE(queryIncludes(QueryMode::All, candidate));
	EXPECT_TRUE(queryIncludes(QueryMode::Any, candidate));
	EXPECT_FALSE(queryIncludes(QueryMode::None, candidate));
	candidate.excluded = true;
	EXPECT_FALSE(queryIncludes(QueryMode::All, candidate));
}

TEST(AbilityRules, QueryNamedObjectsOnlyMatchTheirNames)
{
	QueryCandidate candidate;
	candidate.hasIncludeNames = true;
	EXPECT_FALSE(queryIncludes(QueryMode::Any, candidate));
	EXPECT_FALSE(queryIncludes(QueryMode::None, candidate));
	candidate.nameIncluded = true;
	EXPECT_TRUE(queryIncludes(QueryMode::None, candidate));
	candidate.excluded = true;
	EXPECT_FALSE(queryIncludes(QueryMode::None, candidate));
}

TEST(AbilityRules, RelationFilterAlliesIncludesOwnUnits)
{
	RelationFilter allies;
	allies.allies = true;
	EXPECT_TRUE(relationAllows(allies, Relation::SamePlayer));
	EXPECT_TRUE(relationAllows(allies, Relation::Ally));
	EXPECT_FALSE(relationAllows(allies, Relation::Enemy));
	RelationFilter enemies;
	enemies.enemies = true;
	EXPECT_FALSE(relationAllows(enemies, Relation::Self));
	EXPECT_FALSE(relationAllows(enemies, Relation::Neutral));
	EXPECT_TRUE(relationAllows(RelationFilter(), Relation::Enemy));
}

TEST(AbilityRules, ConditionNamesFollowTheWeaponAndVariationIndex)
{
	EXPECT_EQ(specialWeaponCondition(1), "SPECIAL_WEAPON_ONE");
	EXPECT_EQ(specialWeaponCondition(5), "SPECIAL_WEAPON_FIVE");
	EXPECT_EQ(specialWeaponCondition(0), "");
	EXPECT_EQ(specialWeaponCondition(7), "");
	EXPECT_EQ(packingTypeCondition(2), "PACKING_TYPE_2");
	EXPECT_EQ(packingTypeCondition(0), "");
}

TEST(AbilityRules, PersistentAbilitiesRepeatOnlyUpToTheirCount)
{
	EXPECT_FALSE(repeatsTrigger(true, 1, 0));
	EXPECT_TRUE(repeatsTrigger(true, 1, 5));
	EXPECT_FALSE(repeatsTrigger(true, 5, 5));
	EXPECT_FALSE(repeatsTrigger(false, 1, 5));
}

TEST(AbilityRules, HealAmountIsAbsoluteOrAFractionOfMaxHealth)
{
	EXPECT_FLOAT_EQ(healAmount(2000.0f, 0.5f, true), 1000.0f);
	EXPECT_FLOAT_EQ(healAmount(2000.0f, 150.0f, false), 150.0f);
	EXPECT_FLOAT_EQ(healAmount(2000.0f, -1.0f, false), 0.0f);
}

TEST(AbilityRules, AutoAbilityDefaultsToStartsActiveOrComputer)
{
	EXPECT_FALSE(autoAbilityActive(false, false, 0));
	EXPECT_TRUE(autoAbilityActive(true, false, 0));
	EXPECT_TRUE(autoAbilityActive(false, true, 0));
	EXPECT_TRUE(autoAbilityActive(false, false, 1));
	EXPECT_FALSE(autoAbilityActive(true, true, -1));
}

TEST(AbilityRules, MonitorSwapsTheCommandSetOnlyOnChange)
{
	EXPECT_EQ(monitorCommandSet(true, false), CommandSetChoice::UseToggle);
	EXPECT_EQ(monitorCommandSet(true, true), CommandSetChoice::Keep);
	EXPECT_EQ(monitorCommandSet(false, true), CommandSetChoice::Restore);
	EXPECT_EQ(monitorCommandSet(false, false), CommandSetChoice::Keep);
}

TEST(AbilityRules, MountOptionsGateButtons)
{
	EXPECT_TRUE(mountButtonAllowed(false, false, true));
	EXPECT_FALSE(mountButtonAllowed(false, true, true));
	EXPECT_TRUE(mountButtonAllowed(false, true, false));
	EXPECT_FALSE(mountButtonAllowed(true, false, false));
}
