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
	int getConditionsYesCount() const { return static_cast<int>(required.size()); }
	const Flags& getNthConditionsYes(int index) const { return required[index]; }
};
}

TEST(RetailConditionMatch, SnowAloneCannotSelectIvoryTower)
{
	const std::array<State, 3> states{{{{0}}, {{3}}, {{2}}}};
	EXPECT_EQ(&states[0], findRetailConditionMatch(states, Flags(1), RetailEmptyState::Last));
	EXPECT_EQ(&states[0], findRetailConditionMatch(states, Flags(0), RetailEmptyState::Last));
	EXPECT_EQ(&states[1], findRetailConditionMatch(states, Flags(3), RetailEmptyState::Last));
	EXPECT_EQ(&states[2], findRetailConditionMatch(states, Flags(2), RetailEmptyState::Last));
}

TEST(RetailConditionMatch, FirstDeclaredSubsetWinsOverMoreSpecificState)
{
	const std::array<State, 3> states{{{{0}}, {{2, 4}}, {{5}}}};
	EXPECT_EQ(&states[1], findRetailConditionMatch(states, Flags(5), RetailEmptyState::Last));
}

TEST(RetailConditionMatch, ModelPrefersExplicitDefaultThenLastEmptyState)
{
	const std::array<State, 3> states{{{{0}}, {{0}}, {{3}}}};
	EXPECT_EQ(&states[0], findRetailConditionMatch(states, Flags(1), RetailEmptyState::Last, std::identity{}, &states[0]));
	EXPECT_EQ(&states[1], findRetailConditionMatch(states, Flags(1), RetailEmptyState::Last));
	EXPECT_EQ(&states[2], findRetailConditionMatch(states, Flags(3), RetailEmptyState::Last, std::identity{}, &states[0]));
}

TEST(RetailConditionMatch, AnimationFallsBackToAnIdleEmptyStateOnly)
{
	struct AnimationState
	{
		State draw;
		bool idle;
	};
	const std::array<AnimationState, 3> states{{{{{0}}, false}, {{{0}}, true}, {{{3}}, false}}};
	const auto idle = [](const AnimationState& state) { return state.idle; };
	EXPECT_EQ(&states[1], findRetailConditionMatchIf(states, Flags(1), idle, &AnimationState::draw));
	EXPECT_EQ(&states[2], findRetailConditionMatchIf(states, Flags(3), idle, &AnimationState::draw));

	const std::array<State, 1> conditionalOnly{{{{3}}}};
	EXPECT_EQ(nullptr, findRetailConditionMatch(conditionalOnly, Flags(0), RetailEmptyState::First));
}
