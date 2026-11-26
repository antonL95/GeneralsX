#include "GameLogic/BfmeScriptRules.h"
#include <gtest/gtest.h>

using namespace BfmeScriptRules;

TEST(BfmeScriptRules, KeepsWireIdsBelowTheInsertedType)
{
	for (int wire = 0; wire < WIRE_INSERTED; ++wire)
	{
		const WireType type = classifyWire(wire);
		EXPECT_EQ(type.kind, WireKind::Legacy);
		EXPECT_EQ(type.legacy, wire);
	}
}

TEST(BfmeScriptRules, ShiftsWireIdsAfterTheInsertedType)
{
	EXPECT_EQ(classifyWire(WIRE_INSERTED).kind, WireKind::Unknown);
	for (int wire = WIRE_INSERTED + 1; wire <= WIRE_LAST_LEGACY; ++wire)
	{
		const WireType type = classifyWire(wire);
		EXPECT_EQ(type.kind, WireKind::Legacy);
		EXPECT_EQ(type.legacy, wire - 1);
	}
	EXPECT_EQ(classifyWire(31).legacy, 30);
	EXPECT_EQ(classifyWire(48).legacy, 47);
	EXPECT_EQ(classifyWire(52).legacy, 51);
}

TEST(BfmeScriptRules, MapsTheRotwkParameterTypes)
{
	EXPECT_EQ(classifyWire(54).kind, WireKind::UnitReference);
	EXPECT_EQ(classifyWire(57).kind, WireKind::CounterArithmetic);
	EXPECT_EQ(classifyWire(58).kind, WireKind::ModelCondition);
	EXPECT_EQ(classifyWire(61).kind, WireKind::ObjectType);
	EXPECT_EQ(classifyWire(63).kind, WireKind::Emotion);
	EXPECT_EQ(classifyWire(76).kind, WireKind::EvaEvent);
	EXPECT_EQ(classifyWire(77).kind, WireKind::NotificationType);
	for (int wire : { -1, 53, 55, 62, 75, 78, 1000 })
		EXPECT_EQ(classifyWire(wire).kind, WireKind::Unknown) << wire;
}

TEST(BfmeScriptRules, ResolvesAudioNameInMapScripts)
{
	EXPECT_EQ(classifyWire(59).kind, WireKind::AudioName);
}

TEST(BfmeScriptRules, ResolvesReverbRoomInMapScripts)
{
	EXPECT_EQ(classifyWire(60).kind, WireKind::ReverbRoom);
}

TEST(BfmeScriptRules, ResolvesEveryParameterTypeOfTheSkirmishSet)
{
	for (int wire : { 0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 13, 14, 15, 18, 19, 22, 24, 25, 27, 31, 32, 33, 41, 47, 52, 54, 58, 61, 76 })
		EXPECT_NE(classifyWire(wire).kind, WireKind::Unknown) << wire;
}

TEST(BfmeScriptRules, ComparesWithTheScriptComparisonCodes)
{
	EXPECT_TRUE(compare(LESS_THAN, 4, 5));
	EXPECT_FALSE(compare(LESS_THAN, 5, 5));
	EXPECT_TRUE(compare(LESS_EQUAL, 5, 5));
	EXPECT_TRUE(compare(EQUAL, 5, 5));
	EXPECT_FALSE(compare(EQUAL, 4, 5));
	EXPECT_TRUE(compare(GREATER_EQUAL, 5, 5));
	EXPECT_FALSE(compare(GREATER, 5, 5));
	EXPECT_TRUE(compare(GREATER, 6, 5));
	EXPECT_TRUE(compare(NOT_EQUAL, 4, 5));
	EXPECT_FALSE(compare(6, 4, 5));
	EXPECT_FALSE(compare(-1, 4, 5));
}

TEST(BfmeScriptRules, ComparesThePlayerCountOfAGame)
{
	EXPECT_TRUE(compare(LESS_THAN, 2, 5));
	EXPECT_TRUE(compare(LESS_THAN, 4, 5));
	EXPECT_FALSE(compare(LESS_THAN, 5, 5));
	EXPECT_FALSE(compare(LESS_THAN, 8, 5));
}

TEST(BfmeScriptRules, MatchesSkirmishOrMultiplayerAgainstTheWantedValue)
{
	EXPECT_TRUE(skirmishOrMultiplayerIs(true, false, true));
	EXPECT_TRUE(skirmishOrMultiplayerIs(false, true, true));
	EXPECT_TRUE(skirmishOrMultiplayerIs(true, true, true));
	EXPECT_FALSE(skirmishOrMultiplayerIs(false, false, true));
	EXPECT_TRUE(skirmishOrMultiplayerIs(false, false, false));
	EXPECT_FALSE(skirmishOrMultiplayerIs(true, false, false));
	EXPECT_FALSE(skirmishOrMultiplayerIs(false, true, false));
}

TEST(BfmeScriptRules, ReachesTheLevelCapAtTheLowerOfLimitAndRankCount)
{
	EXPECT_FALSE(levelCapReached(5, 1000, 10));
	EXPECT_TRUE(levelCapReached(10, 1000, 10));
	EXPECT_FALSE(levelCapReached(2, 3, 10));
	EXPECT_TRUE(levelCapReached(3, 3, 10));
	EXPECT_TRUE(levelCapReached(1, 1, 10));
}

TEST(BfmeScriptRules, ComparesDistanceFromTheSquaredDistance)
{
	EXPECT_TRUE(compareDistance(LESS_THAN, 299.0f * 299.0f, 300.0f));
	EXPECT_FALSE(compareDistance(LESS_THAN, 300.0f * 300.0f, 300.0f));
	EXPECT_TRUE(compareDistance(GREATER_EQUAL, 300.0f * 300.0f, 300.0f));
	EXPECT_FALSE(compareDistance(GREATER_EQUAL, 299.0f * 299.0f, 300.0f));
	EXPECT_TRUE(compareDistance(EQUAL, 0.0f, 0.0f));
	EXPECT_TRUE(compareDistance(GREATER, 1.0f, -1.0f));
}

TEST(BfmeScriptRules, ConvertsWholeSecondsToLogicFrames)
{
	EXPECT_EQ(secondsToFrames(20, 30), 600);
	EXPECT_EQ(secondsToFrames(0, 30), 0);
	EXPECT_EQ(secondsToFrames(-1, 30), -30);
}

TEST(BfmeScriptRules, KeepsPositiveClipDepthMultipliers)
{
	EXPECT_EQ(clipDepthMultiplier(10.0f), 10.0f);
	EXPECT_EQ(clipDepthMultiplier(5.0f), 5.0f);
	EXPECT_EQ(clipDepthMultiplier(0.83f), 0.83f);
	EXPECT_EQ(clipDepthMultiplier(0.0f), 1.0f);
	EXPECT_EQ(clipDepthMultiplier(-2.0f), 1.0f);
}

TEST(BfmeScriptRules, ScalesTheRetailFarPlaneByTheMultiplier)
{
	EXPECT_EQ(retailFarClip(1.0f), 1800.0f);
	EXPECT_EQ(retailFarClip(10.0f), 18000.0f);
	EXPECT_EQ(retailFarClip(5.0f), 9000.0f);
	EXPECT_FLOAT_EQ(retailFarClip(0.83f), 1494.0f);
	EXPECT_EQ(retailFarClip(0.0f), 1800.0f);
	EXPECT_EQ(retailFarClip(-2.0f), 1800.0f);
}

TEST(BfmeScriptRules, ConvertsCaptionSecondsToMilliseconds)
{
	EXPECT_EQ(militaryCaptionMilliseconds(10.0f), 10000);
	EXPECT_EQ(militaryCaptionMilliseconds(2.5f), 2500);
	EXPECT_EQ(militaryCaptionMilliseconds(0.25f), 250);
	EXPECT_EQ(militaryCaptionMilliseconds(0.0f), 0);
	EXPECT_EQ(militaryCaptionMilliseconds(-1.0f), 0);
}
