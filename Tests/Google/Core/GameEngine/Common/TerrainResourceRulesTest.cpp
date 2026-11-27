#include "GameLogic/TerrainResourceRules.h"

#include <gtest/gtest.h>

TEST(TerrainResourceRules, ScansTheRetailFootprint)
{
	EXPECT_EQ(TerrainResourceRules::rowHalfWidths(0), (std::vector<int>{0}));
	EXPECT_EQ(TerrainResourceRules::rowHalfWidths(1), (std::vector<int>{1, 0}));
	EXPECT_EQ(TerrainResourceRules::rowHalfWidths(2), (std::vector<int>{2, 2, 1}));
	EXPECT_EQ(TerrainResourceRules::rowHalfWidths(15),
		(std::vector<int>{15, 15, 15, 15, 15, 14, 14, 14, 13, 12, 11, 10, 9, 8, 6, 3}));
}

TEST(TerrainResourceRules, ReservesUnfinishedClaimsPerPlayer)
{
	using TerrainResourceRules::ClaimOrder;
	const ClaimOrder foundation{false, false, 100, 5, 3};
	EXPECT_TRUE(TerrainResourceRules::blocks(foundation, ClaimOrder{false, false, 90, 9, 3}));
	EXPECT_FALSE(TerrainResourceRules::blocks(foundation, ClaimOrder{false, false, 90, 9, 4}));
	EXPECT_FALSE(TerrainResourceRules::blocks(foundation, ClaimOrder{false, false, 110, 1, 3}));
}

TEST(TerrainResourceRules, GivesCompletedClaimsToTheFirstFinisherAcrossPlayers)
{
	using TerrainResourceRules::ClaimOrder;
	const ClaimOrder completed{false, true, 100, 5, 3};
	const ClaimOrder earlierFoundation{false, false, 90, 9, 4};
	EXPECT_FALSE(TerrainResourceRules::blocks(completed, earlierFoundation));
	EXPECT_TRUE(TerrainResourceRules::blocks(earlierFoundation, completed));
	EXPECT_TRUE(TerrainResourceRules::blocks(completed, ClaimOrder{false, true, 90, 9, 4}));
	EXPECT_TRUE(TerrainResourceRules::blocks(completed, ClaimOrder{false, true, 100, 4, 4}));
	EXPECT_FALSE(TerrainResourceRules::blocks(completed, ClaimOrder{false, true, 100, 6, 4}));
}

TEST(TerrainResourceRules, RetainsHighPriorityClaimOrdering)
{
	using TerrainResourceRules::ClaimOrder;
	const ClaimOrder normal{false, true, 100, 5, 3};
	const ClaimOrder priority{true, false, 110, 1, 4};
	EXPECT_TRUE(TerrainResourceRules::blocks(normal, priority));
	EXPECT_FALSE(TerrainResourceRules::blocks(priority, normal));
	EXPECT_TRUE(TerrainResourceRules::blocks(ClaimOrder{true, true, 90, 9, 3}, priority));
}

TEST(TerrainResourceRules, SelectsTheAuthoredModifierAndExtendsItsTail)
{
	const std::vector<int> values{100, 98, 95, 66};
	EXPECT_FLOAT_EQ(TerrainResourceRules::incomeScale({}, 5), 1.0f);
	EXPECT_FLOAT_EQ(TerrainResourceRules::incomeScale(values, 0), 1.0f);
	EXPECT_FLOAT_EQ(TerrainResourceRules::incomeScale(values, 1), 0.98f);
	EXPECT_FLOAT_EQ(TerrainResourceRules::incomeScale(values, 2), 0.95f);
	EXPECT_FLOAT_EQ(TerrainResourceRules::incomeScale(values, 3), 0.66f);
	EXPECT_FLOAT_EQ(TerrainResourceRules::incomeScale(values, 4), 0.66f);
	EXPECT_FLOAT_EQ(TerrainResourceRules::incomeScale(values, 5), 0.64f);
	EXPECT_FLOAT_EQ(TerrainResourceRules::incomeScale(values, 100), 0.0f);
}

TEST(TerrainResourceRules, ScalesIncomeAndKeepsTheMinimumPositiveDeposit)
{
	EXPECT_EQ(TerrainResourceRules::income(25, 757, 757, 1, 1, 1), 25);
	EXPECT_EQ(TerrainResourceRules::income(25, 600, 757, 1, 1, 1), 20);
	EXPECT_EQ(TerrainResourceRules::income(25, 0, 757, 1, 1, 1), 0);
	EXPECT_EQ(TerrainResourceRules::income(25, 757, 757, 2, 1, 0.95f), 48);
	EXPECT_EQ(TerrainResourceRules::income(25, 757, 757, 1, 1, 0), 1);
	EXPECT_EQ(TerrainResourceRules::income(25, 0, 0, 1, 1, 1), 0);
}
