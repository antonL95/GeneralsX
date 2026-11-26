#include "W3DDevice/GameClient/Module/DrawScript.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
struct RecordingHost : DrawScript::Host
{
	std::string previous;
	std::string previousAnimation;
	std::string targetKind;
	DrawScript::Value targetBearing;
	double fraction = 0.0;
	std::string transition;
	bool allowed = false;
	std::vector<std::string> subObjectLog;
	std::vector<std::string> clientActions;
	DrawScript::Value returned;

	DrawScript::Value call(const std::string& function, const std::vector<DrawScript::Value>& arguments) override
	{
		if (function == "CurDrawablePrevAnimationState")
			return previous.empty() ? DrawScript::Value() : DrawScript::Value::of(previous);
		if (function == "CurDrawablePrevAnimation")
			return previousAnimation.empty() ? DrawScript::Value() : DrawScript::Value::of(previousAnimation);
		if (function == "CurDrawablePrevAnimFraction")
			return DrawScript::Value::of(fraction);
		if (function == "CurDrawableAllowToContinue")
			allowed = true;
		if (function == "CurDrawableSetTransitionAnimState" && !arguments.empty())
			transition = arguments.front().text;
		if (function == "CurDrawableHideSubObject" && !arguments.empty())
			subObjectLog.push_back("hide:" + arguments.front().text);
		if (function == "CurDrawableShowSubObject" && !arguments.empty())
			subObjectLog.push_back("show:" + arguments.front().text);
		if (function == "CurDrawableHideSubObjectPermanently" && !arguments.empty())
			subObjectLog.push_back("hide_perm:" + arguments.front().text);
		if (function == "CurDrawableShowSubObjectPermanently" && !arguments.empty())
			subObjectLog.push_back("show_perm:" + arguments.front().text);
		if (function == "CurDrawableIsCurrentTargetKindof" && !arguments.empty())
			return DrawScript::Value::of(!targetKind.empty() && arguments.front().text == targetKind);
		if (function == "CurDrawableGetCurrentTargetBearing")
			return targetBearing;
		if (function == "CurDrawablePlaySound" || function == "CurDrawableHideModule" || function == "CurDrawableShowModule")
		{
			if (arguments.size() == 1 && arguments.front().type == DrawScript::Value::Type::String)
				clientActions.push_back(function + ":" + arguments.front().text);
		}
		return {};
	}
};

RecordingHost run(const char* source, const char* previous, double fraction = 0.0)
{
	RecordingHost host;
	host.previous = previous;
	host.fraction = fraction;
	const auto script = DrawScript::compile(source);
	EXPECT_NE(script, nullptr) << source;
	if (script)
		host.returned = script->run(host);
	return host;
}

const char* const TWO_BRANCHES =
	"Prev = CurDrawablePrevAnimationState()\n"
	"if Prev == \"A\" then CurDrawableSetTransitionAnimState(\"T1\") end\n"
	"if Prev == \"B\" then CurDrawableSetTransitionAnimState(\"T2\") end\n";

const char* const ALLOW_THEN_FRACTION =
	"Prev = CurDrawablePrevAnimationState()\n"
	"if Prev == \"A\" then CurDrawableAllowToContinue()\n"
	" if( CurDrawablePrevAnimFraction() > .89 ) then CurDrawableSetTransitionAnimState(\"T1\") end\n"
	"elseif Prev == \"B\" then CurDrawableAllowToContinue() end\n";
}

TEST(DrawScript, EachBranchSelectsItsTransition)
{
	EXPECT_EQ(run(TWO_BRANCHES, "A").transition, "T1");
	EXPECT_EQ(run(TWO_BRANCHES, "B").transition, "T2");
	EXPECT_EQ(run(TWO_BRANCHES, "C").transition, "");
	EXPECT_EQ(run(TWO_BRANCHES, "a").transition, "");
}

TEST(DrawScript, NestedFractionTestRunsAtEntry)
{
	EXPECT_EQ(run(ALLOW_THEN_FRACTION, "A", 0.5).transition, "");
	EXPECT_TRUE(run(ALLOW_THEN_FRACTION, "A", 0.5).allowed);
	EXPECT_EQ(run(ALLOW_THEN_FRACTION, "A", 0.95).transition, "T1");
	EXPECT_TRUE(run(ALLOW_THEN_FRACTION, "B").allowed);
	EXPECT_FALSE(run(ALLOW_THEN_FRACTION, "C").allowed);
}

TEST(DrawScript, ReturnStopsAndLaterCallWins)
{
	EXPECT_EQ(run("if true then CurDrawableSetTransitionAnimState(\"T1\") return end "
		"CurDrawableSetTransitionAnimState(\"T2\")", "").transition, "T1");
	EXPECT_EQ(run("CurDrawableSetTransitionAnimState(\"T1\") CurDrawableSetTransitionAnimState (\"T2\")", "").transition, "T2");
	EXPECT_EQ(run("if CurDrawablePrevAnimationState() then return \"X\" else CurDrawableSetTransitionAnimState(\"T\") end",
		"").transition, "T");
}

TEST(DrawScript, ComparisonsAndBooleans)
{
	const char* source =
		"Prev = CurDrawablePrevAnimationState() -- comment\n"
		"if Prev ~= \"A\" and Prev ~= \"B\" or Prev == nil then CurDrawableSetTransitionAnimState(\"T\") end";
	EXPECT_EQ(run(source, "C").transition, "T");
	EXPECT_EQ(run(source, "").transition, "T");
	EXPECT_EQ(run(source, "B").transition, "");
}

TEST(DrawScript, RuntimeErrorKeepsEarlierRequestAndStops)
{
	EXPECT_EQ(run("CurDrawableSetTransitionAnimState(\"T1\") if Missing > 1 then end "
		"CurDrawableSetTransitionAnimState(\"T2\")", "").transition, "T1");
}

TEST(DrawScript, RejectsWhatLuaRejects)
{
	EXPECT_EQ(DrawScript::compile("CurDrawableAllowToContinue() // note"), nullptr);
	EXPECT_EQ(DrawScript::compile("if x then"), nullptr);
	EXPECT_EQ(DrawScript::compile("CurDrawableSetTransitionAnimState(\"T"), nullptr);
	EXPECT_EQ(DrawScript::compile("end"), nullptr);
	EXPECT_EQ(DrawScript::compile("return CurDrawableAllowToContinue() CurDrawableAllowToContinue()"), nullptr);
	std::string deep;
	for (int i = 0; i < 10000; ++i)
		deep += "(";
	EXPECT_EQ(DrawScript::compile("x = " + deep), nullptr);
}

TEST(DrawScript, ReturnsAnimationLabelFromBranches)
{
	const char* source =
		"if CurDrawablePrevAnimationState() == \"A\" then return \"Bow\" "
		"else return (\"Sword\") end";
	const RecordingHost first = run(source, "A");
	EXPECT_EQ(first.returned.type, DrawScript::Value::Type::String);
	EXPECT_EQ(first.returned.text, "Bow");

	const RecordingHost second = run(source, "B");
	EXPECT_EQ(second.returned.type, DrawScript::Value::Type::String);
	EXPECT_EQ(second.returned.text, "Sword");

	const RecordingHost empty = run("if true then return end", "");
	EXPECT_EQ(empty.returned.type, DrawScript::Value::Type::Nil);
}

TEST(DrawScript, SubObjectActionsFollowControlFlow)
{
	const char* source =
		"Prev = CurDrawablePrevAnimationState()\n"
		"if Prev == \"Bow\" then\n"
		"  CurDrawableShowSubObject(\"arrow\")\n"
		"  CurDrawableHideSubObjectPermanently(\"sword\")\n"
		"  return \"WantSword\"\n"
		"end\n"
		"CurDrawableHideSubObject(\"arrow\")\n"
		"CurDrawableShowSubObjectPermanently(\"sword\")\n"
		"return \"Sword\"\n";

	const RecordingHost bow = run(source, "Bow");
	EXPECT_EQ(bow.returned.text, "WantSword");
	ASSERT_EQ(bow.subObjectLog.size(), 2u);
	EXPECT_EQ(bow.subObjectLog[0], "show:arrow");
	EXPECT_EQ(bow.subObjectLog[1], "hide_perm:sword");

	const RecordingHost idle = run(source, "Idle");
	EXPECT_EQ(idle.returned.text, "Sword");
	ASSERT_EQ(idle.subObjectLog.size(), 2u);
	EXPECT_EQ(idle.subObjectLog[0], "hide:arrow");
	EXPECT_EQ(idle.subObjectLog[1], "show_perm:sword");
}

TEST(DrawScript, GlobalsPersistAcrossExecutionsInEnvironment)
{
	DrawScript::Environment globals;
	const auto writer = DrawScript::compile("Prev = CurDrawablePrevAnimationState()");
	const auto reader = DrawScript::compile(
		"if Prev == \"STATE_Idle\" then CurDrawableSetTransitionAnimState(\"TRANS_FromSaved\") end");
	ASSERT_NE(writer, nullptr);
	ASSERT_NE(reader, nullptr);

	RecordingHost firstHost;
	firstHost.previous = "STATE_Idle";
	writer->run(firstHost, globals);

	RecordingHost secondHost;
	secondHost.previous = "STATE_Other";
	reader->run(secondHost, globals);
	EXPECT_EQ(secondHost.transition, "TRANS_FromSaved");

	DrawScript::Environment isolated;
	RecordingHost thirdHost;
	reader->run(thirdHost, isolated);
	EXPECT_EQ(thirdHost.transition, "");
}

TEST(DrawScript, TargetKindofSelectsReturnedAnimation)
{
	const char* source =
		"if CurDrawableIsCurrentTargetKindof(\"STRUCTURE\") or CurDrawableIsCurrentTargetKindof(\"MONSTER\") then\n"
		"  return (\"FrontKick\")\n"
		"else\n"
		"  return (\"StickSwipe\")\n"
		"end";
	const auto script = DrawScript::compile(source);
	ASSERT_NE(script, nullptr);

	RecordingHost structureHost;
	structureHost.targetKind = "STRUCTURE";
	EXPECT_EQ(script->run(structureHost).text, "FrontKick");

	RecordingHost monsterHost;
	monsterHost.targetKind = "MONSTER";
	EXPECT_EQ(script->run(monsterHost).text, "FrontKick");

	RecordingHost infantryHost;
	infantryHost.targetKind = "INFANTRY";
	EXPECT_EQ(script->run(infantryHost).text, "StickSwipe");

	RecordingHost noTargetHost;
	EXPECT_EQ(script->run(noTargetHost).text, "StickSwipe");
}

TEST(DrawScript, SameStateReentryReadsPreviousAnimationWithNilState)
{
	const char* source =
		"if CurDrawablePrevAnimationState() == nil and CurDrawablePrevAnimation() == \"IdleA\" then\n"
		"  return \"IdleB\"\n"
		"end\n"
		"return \"IdleA\"";
	const auto script = DrawScript::compile(source);
	ASSERT_NE(script, nullptr);

	RecordingHost reentryHost;
	reentryHost.previous = "";
	reentryHost.previousAnimation = "IdleA";
	EXPECT_EQ(script->run(reentryHost).text, "IdleB");
}

TEST(DrawScript, SoundRequestRunsOnlyThroughTheSelectedBranch)
{
	const RecordingHost host = run(
		"if CurDrawablePrevAnimationState() == \"Idle\" then CurDrawablePlaySound(\"Start\") "
		"else CurDrawablePlaySound(\"Stop\") end", "Idle");
	EXPECT_EQ(host.clientActions, (std::vector<std::string>{"CurDrawablePlaySound:Start"}));
}

TEST(DrawScript, HideModuleReceivesTheAuthoredTag)
{
	const RecordingHost host = run("CurDrawableHideModule(\"Draw_Overlay\")", "");
	EXPECT_EQ(host.clientActions, (std::vector<std::string>{"CurDrawableHideModule:Draw_Overlay"}));
}

TEST(DrawScript, ShowModuleReceivesTheAuthoredTag)
{
	const RecordingHost host = run("CurDrawableShowModule(\"Draw_Overlay\")", "");
	EXPECT_EQ(host.clientActions, (std::vector<std::string>{"CurDrawableShowModule:Draw_Overlay"}));
}

TEST(DrawScript, SignedBearingSelectsAnAnimation)
{
	const auto script = DrawScript::compile(
		"angle = CurDrawableGetCurrentTargetBearing() "
		"if angle == nil then return \"Idle\" "
		"elseif angle < -.10 then return \"Right\" "
		"elseif angle > .10 then return \"Left\" else return \"Straight\" end");
	ASSERT_NE(script, nullptr);

	RecordingHost host;
	EXPECT_EQ(script->run(host).text, "Idle");
	host.targetBearing = DrawScript::Value::of(-0.5);
	EXPECT_EQ(script->run(host).text, "Right");
	host.targetBearing = DrawScript::Value::of(0.5);
	EXPECT_EQ(script->run(host).text, "Left");
	host.targetBearing = DrawScript::Value::of(0.0);
	EXPECT_EQ(script->run(host).text, "Straight");
}
