#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Common/PlayerTemplateRules.h"

TEST(PlayerTemplateRulesTest, ParsesRotwkFieldsOnlyInRetailOrAudit)
{
	EXPECT_FALSE(PlayerTemplateRules::parsesRotwkFields(false, false));
	EXPECT_TRUE(PlayerTemplateRules::parsesRotwkFields(true, false));
	EXPECT_TRUE(PlayerTemplateRules::parsesRotwkFields(false, true));
	EXPECT_TRUE(PlayerTemplateRules::parsesRotwkFields(true, true));
}

TEST(PlayerTemplateRulesTest, UsesMpVariantOnlyInRetailMultiplayerOrSkirmish)
{
	EXPECT_FALSE(PlayerTemplateRules::usesMpVariant(false, false));
	EXPECT_FALSE(PlayerTemplateRules::usesMpVariant(false, true));
	EXPECT_FALSE(PlayerTemplateRules::usesMpVariant(true, false));
	EXPECT_TRUE(PlayerTemplateRules::usesMpVariant(true, true));
}

TEST(PlayerTemplateRulesTest, GrantsInitialUpgradesOnlyInRetail)
{
	EXPECT_FALSE(PlayerTemplateRules::grantsInitialUpgrades(false));
	EXPECT_TRUE(PlayerTemplateRules::grantsInitialUpgrades(true));
}

TEST(PlayerTemplateRulesTest, SelectsMpVariantWithSpFallback)
{
	const std::string sp = "GoodSpellBook";
	const std::string mp = "MenSpellBook";
	const std::string empty;

	EXPECT_EQ(PlayerTemplateRules::selectVariant(false, mp, sp), "GoodSpellBook");
	EXPECT_EQ(PlayerTemplateRules::selectVariant(true, mp, sp), "MenSpellBook");
	EXPECT_EQ(PlayerTemplateRules::selectVariant(true, empty, sp), "GoodSpellBook");
}

TEST(PlayerTemplateRulesTest, SelectsLevelVariantWithSpFallback)
{
	EXPECT_EQ(PlayerTemplateRules::selectLevelVariant(false, 25, 10), 10);
	EXPECT_EQ(PlayerTemplateRules::selectLevelVariant(true, 25, 10), 25);
	EXPECT_EQ(PlayerTemplateRules::selectLevelVariant(true, 0, 10), 10);
}

TEST(PlayerTemplateRulesTest, ResourceModifierPercentFollowsAotrCurve)
{
	const std::vector<int> empty;
	EXPECT_EQ(PlayerTemplateRules::resourceModifierPercent(empty, 0), 100);
	EXPECT_EQ(PlayerTemplateRules::resourceModifierPercent(empty, 8), 100);

	const std::vector<int> aotr = { 100, 100, 100, 100, 100, 95, 90, 85, 80, 75, 71, 68, 66 };
	EXPECT_EQ(PlayerTemplateRules::resourceModifierPercent(aotr, 0), 100);
	EXPECT_EQ(PlayerTemplateRules::resourceModifierPercent(aotr, 1), 100);
	EXPECT_EQ(PlayerTemplateRules::resourceModifierPercent(aotr, 5), 100);
	EXPECT_EQ(PlayerTemplateRules::resourceModifierPercent(aotr, 6), 95);
	EXPECT_EQ(PlayerTemplateRules::resourceModifierPercent(aotr, 10), 75);
	EXPECT_EQ(PlayerTemplateRules::resourceModifierPercent(aotr, 13), 66);
	EXPECT_EQ(PlayerTemplateRules::resourceModifierPercent(aotr, 25), 66);
}
