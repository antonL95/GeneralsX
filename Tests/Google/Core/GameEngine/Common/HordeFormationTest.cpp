#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>

#include "Common/HordeFormation.h"

namespace
{
std::vector<std::string> tokens(const std::string& line)
{
	std::string spaced = line;
	for (char& character : spaced)
	{
		if (character == ':' || character == '=' || character == '\t')
			character = ' ';
	}
	std::istringstream stream(spaced);
	std::vector<std::string> result;
	for (std::string token; stream >> token;)
		result.push_back(token);
	return result;
}

HordeRankInfo rank(const std::string& line)
{
	HordeRankInfo result;
	EXPECT_TRUE(HordeAttributes::parseRankInfo(tokens(line), result)) << line;
	return result;
}
}

TEST(HordeFormation, ParsesRankInfoWithLeadersAndWeaponConditions)
{
	const HordeRankInfo parsed = rank("RankNumber:2 UnitType:GondorFighter Position:X:30 Y:0 Leader 1 0\tPosition:X:30 Y:-20 Leader 1 2 RevokedWeaponCondition:WEAPONSET_TOGGLE_1 GrantedWeaponCondition:WEAPONSET_TOGGLE_2");
	EXPECT_EQ(parsed.rankNumber, 2);
	EXPECT_EQ(parsed.unitType, "GondorFighter");
	ASSERT_EQ(parsed.positions.size(), 2u);
	EXPECT_FLOAT_EQ(parsed.positions[1].point.x, 30.0f);
	EXPECT_FLOAT_EQ(parsed.positions[1].point.y, -20.0f);
	EXPECT_TRUE(parsed.positions[0].hasLeader);
	EXPECT_EQ(parsed.positions[1].leaderRank, 1);
	EXPECT_EQ(parsed.positions[1].leaderIndex, 2);
	EXPECT_EQ(parsed.revokedWeaponCondition, "WEAPONSET_TOGGLE_1");
	EXPECT_EQ(parsed.grantedWeaponCondition, "WEAPONSET_TOGGLE_2");
}

TEST(HordeFormation, AcceptsTheSpacingAndCaseVariantsOfTheCorpus)
{
	EXPECT_EQ(rank("RankNumber:1 UnitType:A Position:X: 25 Y:-25").positions.size(), 1u);
	EXPECT_EQ(rank("RankNumber:1 UnitType:A Position X:25 Y:0").positions.size(), 1u);
	EXPECT_FLOAT_EQ(rank("RankNumber:1 UnitType:A Position:x:7.5 Y:1").positions[0].point.x, 7.5f);
	const HordeRankInfo empty = rank("RankNumber:3 UnitType:A RevokedWeaponCondition: Position:X:1 Y:2");
	EXPECT_TRUE(empty.revokedWeaponCondition.empty());
	EXPECT_EQ(empty.positions.size(), 1u);
	EXPECT_EQ(rank("RankNumber:1 UnitType:A Position:X:1 Y:2 RevokedWeaponCondition: WEAPONSET_TOGGLE_1").revokedWeaponCondition, "WEAPONSET_TOGGLE_1");
}

TEST(HordeFormation, RejectsMalformedRankInfo)
{
	HordeRankInfo result;
	EXPECT_FALSE(HordeAttributes::parseRankInfo(tokens("UnitType:A Position:X:1 Y:2"), result));
	EXPECT_FALSE(HordeAttributes::parseRankInfo(tokens("RankNumber:1 Position:X:1 Y:2"), result));
	EXPECT_FALSE(HordeAttributes::parseRankInfo(tokens("RankNumber:1 UnitType:A Position:X:1"), result));
	EXPECT_FALSE(HordeAttributes::parseRankInfo(tokens("RankNumber:1 UnitType:A Leader 1 0"), result));
	EXPECT_FALSE(HordeAttributes::parseRankInfo(tokens("RankNumber:one UnitType:A"), result));
	EXPECT_FALSE(HordeAttributes::parseRankInfo(tokens("RankNumber:1 UnitType:A Bogus:3"), result));
}

TEST(HordeFormation, ParsesBannerSplitAndComboAttributes)
{
	HordeBannerCarrierPosition banner;
	ASSERT_TRUE(HordeAttributes::parseBannerCarrierPosition(tokens("UnitType:GondorFighter Pos:X:70.0 Y:0.0"), banner));
	EXPECT_EQ(banner.unitType, "GondorFighter");
	EXPECT_FLOAT_EQ(banner.position.x, 70.0f);
	EXPECT_FALSE(HordeAttributes::parseBannerCarrierPosition(tokens("UnitType:GondorFighter"), banner));

	HordeSplit split;
	ASSERT_TRUE(HordeAttributes::parseSplit(tokens("SplitResult:ElfHorde\tUnitType:Elf RankNumber:2"), split));
	EXPECT_EQ(split.result, "ElfHorde");
	EXPECT_EQ(split.unitType, "Elf");
	EXPECT_EQ(split.rankNumber, 2);
	EXPECT_FALSE(HordeAttributes::parseSplit(tokens("UnitType:Elf"), split));

	HordeCombo combo;
	ASSERT_TRUE(HordeAttributes::parseCombo(tokens("Target:Archers Result:ComboHorde InitiateVoice:Cheer"), combo));
	EXPECT_EQ(combo.initiateVoice, "Cheer");
	EXPECT_FALSE(HordeAttributes::parseCombo(tokens("Target:Archers"), combo));
}

TEST(HordeFormation, OrdersSlotsByRankNumberAndKeepsDeclarationOrderWithinARank)
{
	HordeLayout layout;
	layout.addRank(rank("RankNumber:2 UnitType:B Position:X:10 Y:0"));
	layout.addRank(rank("RankNumber:1 UnitType:A Position:X:50 Y:0 Position:X:50 Y:20"));
	layout.addRank(rank("RankNumber:2 UnitType:C Position:X:10 Y:20"));
	ASSERT_EQ(layout.slots().size(), 4u);
	EXPECT_EQ(layout.slots()[0].unitType, "A");
	EXPECT_EQ(layout.slots()[1].index, 1);
	EXPECT_EQ(layout.slots()[2].unitType, "B");
	EXPECT_EQ(layout.slots()[3].unitType, "C");
	EXPECT_EQ(layout.slots()[0].rankInfoIndex, 1u);
	EXPECT_EQ(layout.slots()[1].rankInfoIndex, 1u);
	EXPECT_EQ(layout.slots()[2].rankInfoIndex, 0u);
	EXPECT_EQ(layout.slots()[3].rankInfoIndex, 2u);
}

TEST(HordeFormation, AssignsMembersToMatchingSlotsThenAnyFreeSlot)
{
	HordeLayout layout;
	layout.addRank(rank("RankNumber:1 UnitType:Townsman Position:X:25 Y:-25 Position:X:25 Y:0"));
	layout.addRank(rank("RankNumber:2 UnitType:TownsWoman Position:X:25 Y:25"));
	layout.addPayload("townswoman", 1);
	layout.addPayload("Townsman", 3);
	const std::vector<HordeMemberSlot> members = layout.initialMembers(4);
	ASSERT_EQ(members.size(), 4u);
	EXPECT_EQ(members[0].slot, 2);
	EXPECT_EQ(members[1].slot, 0);
	EXPECT_EQ(members[2].slot, 1);
	EXPECT_EQ(members[3].slot, -1);
	EXPECT_EQ(members[3].payload, 1u);

	std::vector<bool> occupied = { true, false, false };
	EXPECT_EQ(layout.findSlot("Other", occupied), 1);
}

TEST(HordeFormation, CapacityLimitsTheInitialMembers)
{
	HordeLayout layout;
	layout.addRank(rank("RankNumber:1 UnitType:A Position:X:0 Y:0 Position:X:0 Y:10 Position:X:0 Y:20"));
	layout.addPayload("A", 15);
	EXPECT_EQ(layout.initialMembers(2).size(), 2u);
	EXPECT_TRUE(layout.initialMembers(layout.capacity()).empty());
	EXPECT_TRUE(layout.initialMembers(-1).empty());
}

TEST(HordeFormation, RotatesOffsetsIntoTheHordeFrame)
{
	const HordePoint same = HordeLayout::rotate({ 50.0f, 20.0f }, 1.0f, 0.0f);
	EXPECT_FLOAT_EQ(same.x, 50.0f);
	EXPECT_FLOAT_EQ(same.y, 20.0f);
	const HordePoint quarter = HordeLayout::rotate({ 50.0f, 20.0f }, 0.0f, 1.0f);
	EXPECT_FLOAT_EQ(quarter.x, -20.0f);
	EXPECT_FLOAT_EQ(quarter.y, 50.0f);
	const HordePoint half = HordeLayout::rotate({ 50.0f, 20.0f }, -1.0f, 0.0f);
	EXPECT_FLOAT_EQ(half.x, -50.0f);
	EXPECT_FLOAT_EQ(half.y, -20.0f);
}

TEST(HordeFormation, LeaderReferencesUseAuthoredRankAndIndex)
{
	HordeLayout layout;
	HordeRankInfo rear;
	rear.rankNumber = 2;
	rear.positions = {{{-10.0f, 3.0f}, true, 1, 1}, {{-10.0f, 0.0f}, true, 1, 9}};
	layout.addRank(rear);
	HordeRankInfo front;
	front.rankNumber = 1;
	front.positions = {{{0.0f, 0.0f}}, {{0.0f, 3.0f}}};
	layout.addRank(front);

	EXPECT_EQ(layout.leaderSlot(2), 1);
	EXPECT_EQ(layout.leaderSlot(3), -1);
	EXPECT_EQ(layout.leaderSlot(0), -1);
	EXPECT_EQ(layout.leaderSlot(-1), -1);
}

TEST(HordeFormation, AttackRankPolicyUsesAuthoredLists)
{
	EXPECT_TRUE(HordeLayout::rankCanAttack(1, {1, 2}, {3}));
	EXPECT_TRUE(HordeLayout::rankCanAttack(3, {1, 2}, {3}));
	EXPECT_FALSE(HordeLayout::rankCanAttack(4, {1, 2}, {3}));
	EXPECT_TRUE(HordeLayout::rankCanAttack(4, {}, {}));
}

TEST(HordeFormation, NearestSlotsKeepMembersInPlaceAfterAnAboutFace)
{
	HordeLayout layout;
	layout.addRank(rank("RankNumber:1 UnitType:Soldier Position:X:50 Y:0 Position:X:50 Y:20"));
	layout.addRank(rank("RankNumber:2 UnitType:Soldier Position:X:30 Y:0 Leader 1 0 Position:X:30 Y:20 Leader 1 1"));
	layout.addRank(rank("RankNumber:3 UnitType:Banner Position:X:10 Y:0"));
	const std::vector<int> current = { 0, 1, 2, 3, 4 };
	const std::vector<HordePoint> mirrored = { { 30.0f, 20.0f }, { 30.0f, 0.0f }, { 50.0f, 20.0f }, { 50.0f, 0.0f }, { 10.0f, 0.0f } };
	const std::vector<int> slots = layout.nearestSlots(current, mirrored);
	ASSERT_EQ(slots.size(), 5u);
	EXPECT_EQ(slots[0], 3);
	EXPECT_EQ(slots[1], 2);
	EXPECT_EQ(slots[2], 1);
	EXPECT_EQ(slots[3], 0);
	EXPECT_EQ(slots[4], 4);
}

TEST(HordeFormation, NearestSlotsKeepUnitTypesAndSkipMembersWithoutSlots)
{
	HordeLayout layout;
	layout.addRank(rank("RankNumber:1 UnitType:Soldier Position:X:0 Y:0 Position:X:0 Y:20"));
	layout.addRank(rank("RankNumber:2 UnitType:Banner Position:X:0 Y:40"));
	const std::vector<int> current = { 0, 2, -1 };
	const std::vector<HordePoint> positions = { { 0.0f, 40.0f }, { 0.0f, 0.0f }, { 0.0f, 20.0f } };
	const std::vector<int> slots = layout.nearestSlots(current, positions);
	ASSERT_EQ(slots.size(), 3u);
	EXPECT_EQ(slots[0], 1);
	EXPECT_EQ(slots[1], 2);
	EXPECT_EQ(slots[2], -1);
}

TEST(HordeFormation, NearestSlotsAvoidCrossingWhenTheBlockShifts)
{
	HordeLayout layout;
	layout.addRank(rank("RankNumber:1 UnitType:Soldier Position:X:50 Y:0"));
	layout.addRank(rank("RankNumber:2 UnitType:Soldier Position:X:30 Y:0"));
	layout.addRank(rank("RankNumber:3 UnitType:Soldier Position:X:10 Y:0"));
	const std::vector<int> current = { 0, 1, 2 };
	const std::vector<HordePoint> behind = { { -38.0f, 0.0f }, { -18.0f, 0.0f }, { 2.0f, 0.0f } };
	const std::vector<int> slots = layout.nearestSlots(current, behind);
	ASSERT_EQ(slots.size(), 3u);
	EXPECT_EQ(slots[0], 2);
	EXPECT_EQ(slots[1], 1);
	EXPECT_EQ(slots[2], 0);
}

TEST(HordeFormation, NearestSlotsKeepCasualtyReservations)
{
	HordeLayout layout;
	layout.addRank(rank("RankNumber:1 UnitType:Soldier Position:X:0 Y:0 Position:X:10 Y:0 Position:X:20 Y:0"));
	const std::vector<int> current = {0, 1};
	const std::vector<HordePoint> positions = {{20.0f, 0.0f}, {0.0f, 0.0f}};
	EXPECT_EQ(layout.nearestSlots(current, positions, {false, false, true}), (std::vector<int>{1, 0}));
}
