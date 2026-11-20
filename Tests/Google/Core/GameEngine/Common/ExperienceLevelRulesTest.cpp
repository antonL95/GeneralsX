#include <gtest/gtest.h>

#include "GameLogic/ExperienceLevelRules.h"

using ExperienceLevelRules::Candidate;

TEST(ExperienceLevelRulesTest, FirstLevelIsTheLowestRequirementAboveZero)
{
	const std::vector<Candidate> levels = {{3, false, false}, {1, false, false}, {2, false, false}};
	const auto first = ExperienceLevelRules::firstLevel(levels, true);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(*first, 1u);
}

TEST(ExperienceLevelRulesTest, ZeroRequirementIsNeverTheFirstLevel)
{
	const std::vector<Candidate> levels = {{0, false, false}, {5, false, false}};
	const auto first = ExperienceLevelRules::firstLevel(levels, true);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(*first, 1u);
	EXPECT_FALSE(ExperienceLevelRules::firstLevel({{0, false, false}}, true).has_value());
}

TEST(ExperienceLevelRulesTest, TiesKeepTheFirstDeclaredLevel)
{
	const std::vector<Candidate> levels = {{1, false, false}, {1, false, false}};
	const auto first = ExperienceLevelRules::firstLevel(levels, true);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(*first, 0u);
}

TEST(ExperienceLevelRulesTest, GameModeFiltersLevels)
{
	const std::vector<Candidate> levels = {{1, true, false}, {1, false, true}};
	EXPECT_EQ(ExperienceLevelRules::firstLevel(levels, true), std::optional<std::size_t>(1));
	EXPECT_EQ(ExperienceLevelRules::firstLevel(levels, false), std::optional<std::size_t>(0));
}

TEST(ExperienceLevelRulesTest, NextLevelSkipsReachedRequirements)
{
	const std::vector<Candidate> levels = {{1, false, false}, {4, false, false}, {2, false, false}};
	EXPECT_EQ(ExperienceLevelRules::nextLevel(levels, 1, true), std::optional<std::size_t>(2));
	EXPECT_FALSE(ExperienceLevelRules::nextLevel(levels, 4, true).has_value());
}
