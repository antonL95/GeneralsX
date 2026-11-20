#include <gtest/gtest.h>

#include "GameLogic/RotwkLocomotorRules.h"

namespace
{
RotwkLocomotorRules::Resolved zeroFallback()
{
	return {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
}
}

TEST(RotwkLocomotorRulesTest, DurationsRoundUpToRetailLogicFrames)
{
	EXPECT_EQ(RotwkLocomotorRules::retailDurationMs(0), 0u);
	EXPECT_EQ(RotwkLocomotorRules::retailDurationMs(1), 200u);
	EXPECT_EQ(RotwkLocomotorRules::retailDurationMs(200), 200u);
	EXPECT_EQ(RotwkLocomotorRules::retailDurationMs(201), 400u);
	EXPECT_EQ(RotwkLocomotorRules::retailDurationMs(1500), 1600u);
	EXPECT_EQ(RotwkLocomotorRules::retailDurationMs(1600), 1600u);
}

TEST(RotwkLocomotorRulesTest, DurationFramesUseTheRoundedRetailTime)
{
	EXPECT_EQ(RotwkLocomotorRules::durationFrames(1500, 30), 48u);
	EXPECT_EQ(RotwkLocomotorRules::durationFrames(1600, 30), 48u);
	EXPECT_EQ(RotwkLocomotorRules::durationFrames(100, 30), 6u);
	EXPECT_EQ(RotwkLocomotorRules::durationFrames(0, 30), 0u);
	EXPECT_EQ(RotwkLocomotorRules::durationFrames(1000, 30), 30u);
	EXPECT_EQ(RotwkLocomotorRules::durationFrames(1500, 5), 8u);
}

TEST(RotwkLocomotorRulesTest, AccelerationUsesTheDamagedSpeedWhenDamaged)
{
	RotwkLocomotorRules::Authored authored;
	authored.speedDamagedFraction = 0.5f;
	authored.accelerationFrames = 10;
	const auto resolved = RotwkLocomotorRules::resolve(authored, 2.0f, zeroFallback());
	EXPECT_FLOAT_EQ(resolved.maxSpeed, 2.0f);
	EXPECT_FLOAT_EQ(resolved.maxSpeedDamaged, 1.0f);
	EXPECT_FLOAT_EQ(resolved.acceleration, 0.2f);
	EXPECT_FLOAT_EQ(resolved.accelerationDamaged, 0.1f);
}

TEST(RotwkLocomotorRulesTest, UndamagedSpeedFallsBackToTheSetSpeed)
{
	RotwkLocomotorRules::Authored authored;
	authored.accelerationFrames = 4;
	const auto resolved = RotwkLocomotorRules::resolve(authored, 2.0f, zeroFallback());
	EXPECT_FLOAT_EQ(resolved.maxSpeedDamaged, 2.0f);
	EXPECT_FLOAT_EQ(resolved.accelerationDamaged, 0.5f);
}

TEST(RotwkLocomotorRulesTest, LiftIsAPercentOfTheBaseSpeed)
{
	RotwkLocomotorRules::Authored authored;
	authored.liftFraction = 2.0f;
	authored.liftDamagedFraction = 1.0f;
	const auto resolved = RotwkLocomotorRules::resolve(authored, 3.0f, zeroFallback());
	EXPECT_FLOAT_EQ(resolved.lift, 6.0f);
	EXPECT_FLOAT_EQ(resolved.liftDamaged, 3.0f);
}

TEST(RotwkLocomotorRulesTest, UnauthoredLiftKeepsTheTemplateValue)
{
	RotwkLocomotorRules::Resolved fallback = zeroFallback();
	fallback.lift = 0.7f;
	fallback.liftDamaged = 0.3f;
	const auto resolved = RotwkLocomotorRules::resolve(RotwkLocomotorRules::Authored(), 3.0f, fallback);
	EXPECT_FLOAT_EQ(resolved.lift, 0.7f);
	EXPECT_FLOAT_EQ(resolved.liftDamaged, 0.3f);
}

TEST(RotwkLocomotorRulesTest, TurnTimeIsOneFullTurn)
{
	RotwkLocomotorRules::Authored authored;
	authored.turnFrames = RotwkLocomotorRules::durationFrames(1500, 30);
	const auto resolved = RotwkLocomotorRules::resolve(authored, 2.0f, zeroFallback());
	EXPECT_FLOAT_EQ(resolved.turnRate, RotwkLocomotorRules::FULL_TURN / 48.0f);
	EXPECT_FLOAT_EQ(resolved.turnRateDamaged, resolved.turnRate);
}
