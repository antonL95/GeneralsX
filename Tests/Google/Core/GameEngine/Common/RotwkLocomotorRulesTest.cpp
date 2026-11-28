#include <gtest/gtest.h>

#include "Common/RotwkDuration.h"
#include "GameLogic/RotwkLocomotorRules.h"

namespace
{
RotwkLocomotorRules::Resolved zeroFallback()
{
	return {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
}
}

TEST(RotwkDurationTest, DurationsRoundUpToRetailLogicFrames)
{
	EXPECT_EQ(RotwkDuration::retailFrames(0), 0u);
	EXPECT_EQ(RotwkDuration::retailFrames(5), 1u);
	EXPECT_EQ(RotwkDuration::retailFrames(10), 1u);
	EXPECT_EQ(RotwkDuration::retailFrames(200), 1u);
	EXPECT_EQ(RotwkDuration::retailFrames(201), 2u);
	EXPECT_EQ(RotwkDuration::retailFrames(1500), 8u);
	EXPECT_EQ(RotwkDuration::retailFrames(1600), 8u);
}

TEST(RotwkDurationTest, EngineFramesUseTheRoundedRetailTime)
{
	EXPECT_EQ(RotwkDuration::engineFrames(RotwkDuration::retailFrames(1500), 30), 48u);
	EXPECT_EQ(RotwkDuration::engineFrames(RotwkDuration::retailFrames(5), 30), 6u);
	EXPECT_EQ(RotwkDuration::engineFrames(RotwkDuration::retailFrames(0), 30), 0u);
	EXPECT_EQ(RotwkDuration::engineFrames(RotwkDuration::retailFrames(1000), 30), 30u);
	EXPECT_EQ(RotwkDuration::engineFrames(RotwkDuration::retailFrames(1500), 5), 8u);
}

TEST(RotwkDurationTest, EachEliteBowReloadRollSnapsInRetailFrames)
{
	const auto minimum = RotwkDuration::retailFrames(1500);
	const auto maximum = RotwkDuration::retailFrames(2000);
	ASSERT_EQ(maximum - minimum, 2u);
	EXPECT_EQ(RotwkDuration::engineFrames(RotwkDuration::clipReloadFrames(minimum), 30), 36u);
	EXPECT_EQ(RotwkDuration::engineFrames(RotwkDuration::clipReloadFrames(minimum + 1), 30), 54u);
	EXPECT_EQ(RotwkDuration::engineFrames(RotwkDuration::clipReloadFrames(maximum), 30), 54u);
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

TEST(RotwkLocomotorRulesTest, UnauthoredDamagedLiftUsesAuthoredLift)
{
	RotwkLocomotorRules::Authored authored;
	authored.liftFraction = 2.0f;
	const auto resolved = RotwkLocomotorRules::resolve(authored, 3.0f, zeroFallback());
	EXPECT_FLOAT_EQ(resolved.liftDamaged, 6.0f);
}

TEST(RotwkLocomotorRulesTest, TurnTimeIsOneFullTurn)
{
	RotwkLocomotorRules::Authored authored;
	authored.turnFrames = RotwkDuration::engineFrames(RotwkDuration::retailFrames(1500), 30);
	const auto resolved = RotwkLocomotorRules::resolve(authored, 2.0f, zeroFallback());
	EXPECT_FLOAT_EQ(resolved.turnRate, RotwkLocomotorRules::FULL_TURN / 48.0f);
	EXPECT_FLOAT_EQ(resolved.turnRateDamaged, resolved.turnRate);
}

TEST(RotwkLocomotorRulesTest, UnauthoredBackingUpSpeedDefaultsToThreeQuarters)
{
	RotwkLocomotorRules::Authored authored;
	EXPECT_FLOAT_EQ(authored.backingUpSpeedFraction, 0.75f);
}

TEST(RotwkLocomotorRulesTest, UnauthoredAccelDecelTriggerDefaultsToHalf)
{
	RotwkLocomotorRules::Authored authored;
	EXPECT_FLOAT_EQ(authored.accelDecelTrigger, 0.5f);
}
