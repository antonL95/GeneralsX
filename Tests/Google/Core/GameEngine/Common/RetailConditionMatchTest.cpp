#include <array>
#include <bitset>
#include <vector>

#include <gtest/gtest.h>

#include "Common/RetailConditionMatch.h"

namespace
{
struct Flags : std::bitset<8>
{
	using std::bitset<8>::bitset;

	bool testForAll(const Flags& required) const
	{
		return (*this & required) == required;
	}
};

struct State
{
	std::vector<Flags> required;
	bool fallback = false;

	int getConditionsYesCount() const { return static_cast<int>(required.size()); }
	const Flags& getNthConditionsYes(int index) const { return required[index]; }
};
}

TEST(RetailConditionMatch, SnowAloneCannotSelectIvoryTower)
{
	const std::array<State, 3> states{{{{0}, true}, {{3}}, {{2}}}};
	const auto fallback = [](const State& state) { return state.fallback; };
	EXPECT_EQ(&states[0], findRetailConditionMatch(states, Flags(1), fallback));
	EXPECT_EQ(&states[0], findRetailConditionMatch(states, Flags(0), fallback));
	EXPECT_EQ(&states[1], findRetailConditionMatch(states, Flags(3), fallback));
	EXPECT_EQ(&states[2], findRetailConditionMatch(states, Flags(2), fallback));
}

TEST(RetailConditionMatch, FirstDeclaredSubsetWinsOverMoreSpecificState)
{
	const std::array<State, 3> states{{{{0}, true}, {{2, 4}}, {{5}}}};
	const auto fallback = [](const State& state) { return state.fallback; };
	EXPECT_EQ(&states[1], findRetailConditionMatch(states, Flags(5), fallback));
}

TEST(RetailConditionMatch, OnlyIdleStatesQualifyAsAnimationFallback)
{
	const std::array<State, 4> states{{{{0}}, {{0}, true}, {{0}, true}, {{3}}}};
	const auto fallback = [](const State& state) { return state.fallback; };
	EXPECT_EQ(&states[1], findRetailConditionMatch(states, Flags(1), fallback));
	EXPECT_EQ(&states[3], findRetailConditionMatch(states, Flags(3), fallback));

	const std::array<State, 1> noIdle{{{{0}}}};
	EXPECT_EQ(nullptr, findRetailConditionMatch(noIdle, Flags(0), fallback));
}
