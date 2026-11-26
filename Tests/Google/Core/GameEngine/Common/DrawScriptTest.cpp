#include "W3DDevice/GameClient/Module/DrawScript.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
struct RecordingHost : DrawScript::Host
{
	std::string previous;
	double fraction = 0.0;
	std::string transition;
	bool allowed = false;

	DrawScript::Value call(const std::string& function, const std::vector<DrawScript::Value>& arguments) override
	{
		if (function == "CurDrawablePrevAnimationState")
			return previous.empty() ? DrawScript::Value() : DrawScript::Value::of(previous);
		if (function == "CurDrawablePrevAnimFraction")
			return DrawScript::Value::of(fraction);
		if (function == "CurDrawableAllowToContinue")
			allowed = true;
		if (function == "CurDrawableSetTransitionAnimState" && !arguments.empty())
			transition = arguments.front().text;
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
		script->run(host);
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
