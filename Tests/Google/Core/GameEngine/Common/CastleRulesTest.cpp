#include <cmath>

#include <gtest/gtest.h>

#include "GameLogic/CastleRules.h"

TEST(CastleRulesTest, StartingUnitOffsetsUseTheRetailFrame)
{
	const float axis = std::sqrt(0.5f);
	const auto forward = CastleRules::startingUnitOffset(0.0f, 300.0f, 0.0f);
	EXPECT_NEAR(forward.x, 300.0f * axis, 0.001f);
	EXPECT_NEAR(forward.y, -300.0f * axis, 0.001f);

	const auto side = CastleRules::startingUnitOffset(300.0f, 0.0f, 0.0f);
	EXPECT_NEAR(side.x, 300.0f * axis, 0.001f);
	EXPECT_NEAR(side.y, 300.0f * axis, 0.001f);

	const auto back = CastleRules::startingUnitOffset(-300.0f, 0.0f, 0.0f);
	EXPECT_NEAR(back.x, -300.0f * axis, 0.001f);
	EXPECT_NEAR(back.y, -300.0f * axis, 0.001f);

	const auto behind = CastleRules::startingUnitOffset(0.0f, -300.0f, 0.0f);
	EXPECT_NEAR(behind.x, -300.0f * axis, 0.001f);
	EXPECT_NEAR(behind.y, 300.0f * axis, 0.001f);
}

TEST(CastleRulesTest, HeightTurnsTheOffsetLikeRetail)
{
	const auto porter = CastleRules::startingUnitOffset(1.0f, 130.0f, 50.0f);
	EXPECT_NEAR(porter.x, 127.2863f, 0.001f);
	EXPECT_NEAR(porter.y, -56.5614f, 0.001f);

	const auto mirrored = CastleRules::startingUnitOffset(-1.0f, 130.0f, 50.0f);
	EXPECT_NEAR(mirrored.x, 56.5614f, 0.001f);
	EXPECT_NEAR(mirrored.y, -127.2863f, 0.001f);

	const auto flat = CastleRules::startingUnitOffset(30.0f, 200.0f, 0.0f);
	EXPECT_NEAR(flat.x, 162.6346f, 0.001f);
	EXPECT_NEAR(flat.y, -120.2082f, 0.001f);

	const float length = std::hypot(1.0f, 130.0f, 50.0f);
	EXPECT_NEAR(std::hypot(porter.x, porter.y), length, 0.001f);
}

TEST(CastleRulesTest, OnlyAnAllZeroOffsetSearches)
{
	EXPECT_TRUE(CastleRules::isZeroOffset(0.0f, 0.0f, 0.0f));
	EXPECT_TRUE(CastleRules::isZeroOffset(-0.0f, 0.0f, 0.0f));
	EXPECT_FALSE(CastleRules::isZeroOffset(0.0f, 0.0f, 50.0f));
}

TEST(CastleRulesTest, SearchRadiusComesFromTheCastleOrTheBuilding)
{
	EXPECT_EQ(CastleRules::startingUnitSearchRadius(false, 70.0f, true, 130.0f), 50.0f);
	EXPECT_EQ(CastleRules::startingUnitSearchRadius(true, 70.0f, true, 130.0f), 130.0f);
	EXPECT_EQ(CastleRules::startingUnitSearchRadius(true, 70.0f, true, 0.0f), 0.0f);
	EXPECT_EQ(CastleRules::startingUnitSearchRadius(true, 70.0f, false, 130.0f), 70.0f);
	EXPECT_FLOAT_EQ(130.0f * CastleRules::START_UNIT_RADIUS_SPREAD, 182.0f);
}

TEST(CastleRulesTest, KeepLossKillsEverythingOnlyWhenAsked)
{
	using CastleRules::KeepAction;
	EXPECT_EQ(CastleRules::keepAction(true, false, true), KeepAction::None);
	EXPECT_EQ(CastleRules::keepAction(true, true, true), KeepAction::KillEverything);
	EXPECT_EQ(CastleRules::keepAction(false, false, true), KeepAction::KillEverything);
	EXPECT_EQ(CastleRules::keepAction(true, true, false), KeepAction::Lost);
	EXPECT_EQ(CastleRules::keepAction(false, false, false), KeepAction::Lost);
}

TEST(CastleRulesTest, ObjectFiltersCheckNamesBeforeKinds)
{
	CastleRules::FilterFacts structure{false, false, false, true, true, true};
	EXPECT_TRUE(CastleRules::filterMatches(structure));
	structure.excludedByKind = true;
	EXPECT_FALSE(CastleRules::filterMatches(structure));
	structure.includedByName = true;
	EXPECT_TRUE(CastleRules::filterMatches(structure));

	CastleRules::FilterFacts unit{false, false, false, false, false, false};
	EXPECT_TRUE(CastleRules::filterMatches(unit));
	unit.excludedByName = true;
	EXPECT_FALSE(CastleRules::filterMatches(unit));

	const CastleRules::FilterFacts none{false, false, false, false, false, true};
	EXPECT_FALSE(CastleRules::filterMatches(none));
	const CastleRules::FilterFacts notIncluded{false, false, false, true, false, false};
	EXPECT_FALSE(CastleRules::filterMatches(notIncluded));
}

TEST(CastleRulesTest, VictorySkipsDeadObjectsAndTheRetailKinds)
{
	const CastleRules::VictoryFacts keep{false, true, false, false, false, false};
	EXPECT_TRUE(CastleRules::countsAsStructure(keep, true));
	EXPECT_FALSE(CastleRules::countsAsStructure(keep, false));
	EXPECT_FALSE(CastleRules::countsAsUnit(keep, true));

	const CastleRules::VictoryFacts deadKeep{true, true, false, false, false, false};
	EXPECT_FALSE(CastleRules::countsAsStructure(deadKeep, true));

	const CastleRules::VictoryFacts mine{false, true, true, false, false, false};
	EXPECT_FALSE(CastleRules::countsAsStructure(mine, true));

	const CastleRules::VictoryFacts soldier{false, false, false, false, false, false};
	EXPECT_TRUE(CastleRules::countsAsUnit(soldier, true));
	EXPECT_FALSE(CastleRules::countsAsUnit(soldier, false));

	const CastleRules::VictoryFacts arrow{false, false, false, true, false, false};
	EXPECT_FALSE(CastleRules::countsAsUnit(arrow, true));
	const CastleRules::VictoryFacts ignored{false, false, false, false, true, false};
	EXPECT_FALSE(CastleRules::countsAsUnit(ignored, true));
	const CastleRules::VictoryFacts spawner{false, false, false, false, false, true};
	EXPECT_FALSE(CastleRules::countsAsUnit(spawner, true));
}

TEST(CastleRulesTest, ConstructionStepFinishesInBuildTimeFrames)
{
	EXPECT_FLOAT_EQ(CastleRules::constructionStep(900), 100.0f / 900.0f);
	EXPECT_EQ(CastleRules::constructionStep(0), 100.0f);
	float percent = 0.0f;
	int frames = 0;
	while (percent < 100.0f)
	{
		percent += CastleRules::constructionStep(900);
		++frames;
	}
	EXPECT_GE(frames, 900);
	EXPECT_LE(frames, 901);
}
