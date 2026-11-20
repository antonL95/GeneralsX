#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Common/HeroRules.h"

TEST(HeroRules, RosterPutsRingHeroesFirst)
{
	const std::vector<std::string> ring = { "RingHeroFrodo" };
	const std::vector<std::string> buildable = { "CreateAHero", "GondorPippin", "GondorBoromir" };
	const std::vector<std::string> roster = HeroRules::rosterOrder(ring, buildable);
	ASSERT_EQ(roster.size(), 4u);
	EXPECT_EQ(roster[0], "RingHeroFrodo");
	EXPECT_EQ(roster[1], "CreateAHero");
	EXPECT_EQ(roster[3], "GondorBoromir");
}

TEST(HeroRules, ReviveSlotsCountOnlyReviveEntries)
{
	const std::vector<bool> counted = { false, false, true, true, false, true };
	const std::vector<int> slots = HeroRules::reviveSlots(counted);
	const std::vector<int> expected = { -1, -1, 0, 1, -1, 2 };
	EXPECT_EQ(slots, expected);
}

TEST(HeroRules, MenFortressSlotsMatchTheFakeSlotLayout)
{
	std::vector<bool> counted(25, false);
	for (int entry = 14; entry <= 23; ++entry)
		counted[entry] = true;
	const std::vector<int> slots = HeroRules::reviveSlots(counted);
	EXPECT_EQ(slots[14], 0);
	EXPECT_EQ(slots[15], 1);
	EXPECT_EQ(slots[19], 5);
	EXPECT_EQ(slots[23], 9);
	EXPECT_EQ(slots[24], -1);
}

TEST(HeroRules, RevivePriceUsesHighestEntryNotAboveLevel)
{
	const HeroRules::Price base = { 1600, 3825 };
	const std::vector<HeroRules::ReviveEntry> entries = { { 2, 1700, 3825 }, { 3, 1800, 3825 }, { 10, 2500, 4000 } };
	EXPECT_EQ(HeroRules::revivePrice(base, entries, 1).cost, 1600);
	EXPECT_EQ(HeroRules::revivePrice(base, entries, 2).cost, 1700);
	EXPECT_EQ(HeroRules::revivePrice(base, entries, 7).cost, 1800);
	EXPECT_EQ(HeroRules::revivePrice(base, entries, 10).cost, 2500);
	EXPECT_EQ(HeroRules::revivePrice(base, entries, 10).frames, 4000u);
	EXPECT_EQ(HeroRules::revivePrice(base, {}, 5).cost, 1600);
}

TEST(HeroRules, LevelStartsAtOne)
{
	EXPECT_EQ(HeroRules::levelFromVeterancy(0), 1);
	EXPECT_EQ(HeroRules::levelFromVeterancy(3), 4);
	EXPECT_EQ(HeroRules::levelFromVeterancy(-1), 1);
}

TEST(HeroRules, NeededUpgradesAllOrAny)
{
	EXPECT_TRUE(HeroRules::neededUpgradesMet({}, false));
	EXPECT_TRUE(HeroRules::neededUpgradesMet({ true, true }, false));
	EXPECT_FALSE(HeroRules::neededUpgradesMet({ true, false }, false));
	EXPECT_TRUE(HeroRules::neededUpgradesMet({ true, false }, true));
	EXPECT_FALSE(HeroRules::neededUpgradesMet({ false, false }, true));
}

TEST(HeroRules, DeathOutcome)
{
	EXPECT_EQ(HeroRules::deathOutcome(false, true, false), HeroRules::State::Recruitable);
	EXPECT_EQ(HeroRules::deathOutcome(true, true, false), HeroRules::State::Revivable);
	EXPECT_EQ(HeroRules::deathOutcome(true, false, false), HeroRules::State::Lost);
	EXPECT_EQ(HeroRules::deathOutcome(true, true, true), HeroRules::State::Lost);
}

TEST(HeroRules, SlotStates)
{
	using HeroRules::Slot;
	using HeroRules::State;
	EXPECT_EQ(HeroRules::slotFor(false, true, State::Recruitable, false, false), Slot::Hidden);
	EXPECT_EQ(HeroRules::slotFor(true, true, State::Lost, false, false), Slot::Hidden);
	EXPECT_EQ(HeroRules::slotFor(true, false, State::Recruitable, false, false), Slot::Disabled);
	EXPECT_EQ(HeroRules::slotFor(true, true, State::Recruitable, true, false), Slot::Disabled);
	EXPECT_EQ(HeroRules::slotFor(true, true, State::Recruitable, false, true), Slot::Busy);
	EXPECT_EQ(HeroRules::slotFor(true, true, State::Recruitable, false, false), Slot::Recruit);
	EXPECT_EQ(HeroRules::slotFor(true, true, State::Revivable, false, false), Slot::Revive);
}
