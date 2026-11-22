#include <gtest/gtest.h>

#include "GameLogic/CreateAHeroRules.h"

namespace
{

TEST(CreateAHeroRulesTest, ParsesDefaultBlingPrefix)
{
	const auto defRef = CreateAHeroRules::parseBlingToken("@Upgrade_CHW06");
	EXPECT_TRUE(defRef.isDefault);
	EXPECT_EQ(defRef.upgradeName, "Upgrade_CHW06");

	const auto plainRef = CreateAHeroRules::parseBlingToken("Upgrade_CHW01");
	EXPECT_FALSE(plainRef.isDefault);
	EXPECT_EQ(plainRef.upgradeName, "Upgrade_CHW01");
}

TEST(CreateAHeroRulesTest, ValidatesFactionAndPowerEligibility)
{
	const std::vector<std::string> factions = { "Men", "Elves", "Dwarves" };
	EXPECT_TRUE(CreateAHeroRules::isFactionAllowed(factions, "dwarves"));
	EXPECT_FALSE(CreateAHeroRules::isFactionAllowed(factions, "Mordor"));
	EXPECT_TRUE(CreateAHeroRules::isFactionAllowed({}, "Mordor"));

	const std::vector<std::string> allowable = { "Upgrade_CreateAHero_ClassDwarf", "Upgrade_CreateAHero_ClassHeroOfTheWest" };
	EXPECT_TRUE(CreateAHeroRules::isPowerAllowedForUpgrades(allowable, { "Upgrade_CreateAHero_ClassDwarf" }));
	EXPECT_FALSE(CreateAHeroRules::isPowerAllowedForUpgrades(allowable, { "Upgrade_CreateAHero_ClassWraith" }));

	EXPECT_TRUE(CreateAHeroRules::isPowerEligibleAtLevel(1, 1, "None", {}));
	EXPECT_FALSE(CreateAHeroRules::isPowerEligibleAtLevel(3, 2, "None", {}));
	EXPECT_FALSE(CreateAHeroRules::isPowerEligibleAtLevel(3, 3, "Command_SpecialAbilityDreadPlagueLevel1", {}));
	EXPECT_TRUE(CreateAHeroRules::isPowerEligibleAtLevel(
		3, 3, "Command_SpecialAbilityDreadPlagueLevel1", { "Command_SpecialAbilityDreadPlagueLevel1" }));
}

TEST(CreateAHeroRulesTest, ComputesPowerAndRevivalDiscounts)
{
	EXPECT_EQ(CreateAHeroRules::discountedPowerCost(200, 1, 1, 10), 200);
	EXPECT_EQ(CreateAHeroRules::discountedPowerCost(200, 1, 2, 10), 180);
	EXPECT_EQ(CreateAHeroRules::discountedPowerCost(200, 1, 15, 10), 0);
	EXPECT_EQ(CreateAHeroRules::discountedRevivalCost(2000, 75), 500);
}

TEST(CreateAHeroRulesTest, ValidatesAttributePointBudget)
{
	const std::vector<CreateAHeroRules::AttributeRange> ranges = {
		{ "CreateAHero_ArmorAttribute", 7, 18, 17 },
		{ "CreateAHero_DamageMultAttribute", 4, 18, 15 },
		{ "CreateAHero_HealthMultAttribute", 5, 18, 13 }
	};
	EXPECT_TRUE(CreateAHeroRules::validateAttributeSpend(ranges, { 17, 15, 13 }, 30));
	EXPECT_FALSE(CreateAHeroRules::validateAttributeSpend(ranges, { 18, 18, 18 }, 30));
	EXPECT_FALSE(CreateAHeroRules::validateAttributeSpend(ranges, { 6, 15, 13 }, 30));
}

TEST(CreateAHeroRulesTest, RoundTripsEncodedSelection)
{
	CreateAHeroRules::SelectionData sel;
	sel.classUpgrade = "Upgrade_CreateAHero_ClassDwarf";
	sel.subClassUpgrade = "Upgrade_CreateAHero_SubClass_0";
	sel.heroName = "DainTaskmaster";
	sel.blingUpgrades = { "Upgrade_CHW06", "Upgrade_CHH02" };
	sel.attributeUpgrades = { "Upgrade_ArmorAttribute18", "Upgrade_DamageMultAttribute16" };
	sel.powerButtons = { "Command_CreateAHeroLeadership" };
	sel.primaryColor[0] = 120;
	sel.primaryColor[1] = 140;
	sel.primaryColor[2] = 160;

	const std::string encoded = CreateAHeroRules::encodeSelection(sel);
	EXPECT_EQ(encoded.find(';'), std::string::npos);
	EXPECT_EQ(encoded.find(':'), std::string::npos);

	CreateAHeroRules::SelectionData decoded;
	ASSERT_TRUE(CreateAHeroRules::decodeSelection(encoded, decoded));
	EXPECT_EQ(decoded.classUpgrade, sel.classUpgrade);
	EXPECT_EQ(decoded.subClassUpgrade, sel.subClassUpgrade);
	EXPECT_EQ(decoded.heroName, sel.heroName);
	EXPECT_EQ(decoded.blingUpgrades, sel.blingUpgrades);
	EXPECT_EQ(decoded.attributeUpgrades, sel.attributeUpgrades);
	EXPECT_EQ(decoded.powerButtons, sel.powerButtons);
	EXPECT_EQ(decoded.primaryColor[0], 120);
	EXPECT_EQ(decoded.primaryColor[1], 140);
	EXPECT_EQ(decoded.primaryColor[2], 160);

	CreateAHeroRules::SelectionData bad;
	EXPECT_FALSE(CreateAHeroRules::decodeSelection("invalid|data", bad));
}

}
