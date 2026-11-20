#include "Common/INIBlockStructure.h"

#include <gtest/gtest.h>

namespace
{
using namespace INIBlockStructure;

bool opens(std::string_view line, std::string_view parent = {})
{
	LineHead head;
	return parseLineHead(line, head) && opensBlock(head, parent);
}

void expectEnd(std::string_view text)
{
	LineHead header;
	const auto firstNewline = text.find('\n');
	ASSERT_TRUE(parseLineHead(text.substr(0, firstNewline), header));
	EXPECT_EQ(findBlockEnd(text, firstNewline + 1, header.keyword), text.rfind("END ROOT"));
}
}

TEST(INIBlockStructure, ReadsLineHeads)
{
	LineHead head;
	ASSERT_TRUE(parseLineHead(" \tColor = DefaultColor \r", head));
	EXPECT_EQ(head.keyword, "Color");
	EXPECT_EQ(head.shape, Shape::Assign);
	EXPECT_EQ(head.value, "DefaultColor");
	ASSERT_TRUE(parseLineHead("Sound\t", head));
	EXPECT_EQ(head.shape, Shape::Bare);
	ASSERT_TRUE(parseLineHead("Object Building", head));
	EXPECT_EQ(head.shape, Shape::Positional);
	EXPECT_FALSE(parseLineHead(" \t\r", head));
}

TEST(INIBlockStructure, SiegeTowerStateAtDrawColumn)
{
	expectEnd("Draw = W3DTruckDraw Tag\nModelConditionState = DYING\n  Model = None\nEnd\nEND ROOT");
}

TEST(INIBlockStructure, PorterAnimationInsideState)
{
	expectEnd("Draw = W3DScriptedModelDraw Tag\n AnimationState = X\n  Animation = A\n   AnimationName = A\n  End\n End\nEND ROOT");
}

TEST(INIBlockStructure, AngmarPorterMixesTabsAndSpaces)
{
	expectEnd("\tDraw = W3DScriptedModelDraw Tag\n  DefaultModelConditionState\n\t\tModel = Porter\n  End\nEND ROOT");
}

TEST(INIBlockStructure, DispositionAngleStaysScalar)
{
	expectEnd("ObjectCreationList First\n CreateObject\n  DispositionAngle = 270\n    Count = 1\n End\nEND ROOT");
	EXPECT_FALSE(opens("DispositionAngle = 270"));
	EXPECT_FALSE(opens("NeededUpgrade = Upgrade_X"));
}

TEST(INIBlockStructure, TrebuchetUsesOneColumn)
{
	expectEnd("Object GondorTrebuchet\nDraw = W3DScriptedModelDraw Tag\nAnimationState = X\nAnimation\nAnimationName = A\nend\nEND\nEnd StateName = X\nEND ROOT");
}

TEST(INIBlockStructure, RawScriptIgnoresLuaEndAndBlockKeywords)
{
	expectEnd("AnimationState = X\nBeginScript\nif X then\nend\nObject A\nEnd\neNdScRiPt\nEND ROOT");
	const std::string_view raw = "BeginScript\nEnd\nEND ROOT\nEndScript trailing\nEND ROOT";
	EXPECT_EQ(findBlockEnd(raw, raw.find('\n') + 1, "BeginScript"), raw.find("EndScript"));
	EXPECT_EQ(findBlockEnd("End\n", 0, "BeginScript"), std::string_view::npos);
}

TEST(INIBlockStructure, UsesShapeAndValueRules)
{
	EXPECT_TRUE(opens("Color = DefaultColor"));
	EXPECT_FALSE(opens("Color = R:1 G:2 B:3"));
	EXPECT_TRUE(opens("AddEmotion = OVERRIDE Name"));
	EXPECT_FALSE(opens("AddEmotion = Name"));
	EXPECT_TRUE(opens("AttackPriority Name"));
	EXPECT_FALSE(opens("AttackPriority - Name"));
	EXPECT_FALSE(opens("AttackPriority 5"));
	EXPECT_FALSE(opens("Color = 255 0 0"));
	EXPECT_FALSE(opens("Color = 5"));
	EXPECT_FALSE(opens("Color ="));
	EXPECT_FALSE(opens("AddEmotion = Name Delay:5"));
	EXPECT_FALSE(opens("AttackPriority +.5%"));
	EXPECT_TRUE(opens("AttackPriority 5x"));
	EXPECT_TRUE(opens("Sound SomeSound"));
	EXPECT_FALSE(opens("Sound = SomeSound"));
	EXPECT_TRUE(opens("ParticleSystem"));
	EXPECT_FALSE(opens("ParticleSystem = SomeSystem"));
	EXPECT_TRUE(opens("Object Building"));
	EXPECT_FALSE(opens("Object = Building"));
	EXPECT_FALSE(opens("Blank"));
	EXPECT_FALSE(opens("GeometryName"));
	EXPECT_FALSE(opens("StateName"));
	EXPECT_TRUE(opens("Radius = FCurve"));
	EXPECT_FALSE(opens("Radius = FCurve Other"));
	EXPECT_FALSE(opens("Radius = 3"));
	expectEnd("Behavior = PartTheHeavensUpdate\nRadius = FCurve\nKey = 1\nEnd\nEND ROOT");
}

TEST(INIBlockStructure, TransitionDependsOnImmediateParent)
{
	EXPECT_TRUE(opens("Transition = X", "wINDOW"));
	EXPECT_FALSE(opens("Transition = DOWN", "CommandMap"));
	expectEnd("WindowTransition Test\nWindow\nTransition = X\nEnd\nEnd\nEND ROOT");
	expectEnd("CommandMap Test\nTransition = DOWN\nEND ROOT");
}

TEST(INIBlockStructure, RecognizesSingleLineTopLevelTypes)
{
	EXPECT_TRUE(isSingleLineTopLevel("EvaEventForwardReference"));
	EXPECT_TRUE(isSingleLineTopLevel("AudioLowMHz"));
	EXPECT_TRUE(isSingleLineTopLevel("BenchProfile"));
	EXPECT_TRUE(isSingleLineTopLevel("ReallyLowMHz"));
	EXPECT_TRUE(isSingleLineTopLevel("LODPreset"));
	EXPECT_FALSE(isSingleLineTopLevel("Object"));
	const std::string_view input = "EvaEventForwardReference Example\nObject Following\nEnd";
	EXPECT_EQ(findBlockEnd(input, input.find("End"), "Object"), input.find("End"));
}

TEST(INIBlockStructure, FirstTokenEndsBlockEvenWithTrailingText)
{
	for (const std::string_view line : {"end", "END", "End StateName = X", "End =", "End 1"})
	{
		LineHead head;
		ASSERT_TRUE(parseLineHead(line, head));
		EXPECT_TRUE(isEnd(head));
	}
}

TEST(INIBlockStructure, MissingEndReturnsNpos)
{
	EXPECT_EQ(findBlockEnd("Draw = X\nAnimationState = X\nEnd\n", 9, "Draw"), std::string_view::npos);
	EXPECT_EQ(findBlockEnd("", 0, "Draw"), std::string_view::npos);
}
