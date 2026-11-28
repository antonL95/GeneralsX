#include "Common/WeightedAudio.h"

#include <gtest/gtest.h>

#include <limits>
#include <random>
#include <vector>

TEST(WeightedAudioRules, ParsesPositiveWeightsAndDefaultsOtherwise)
{
	const auto weighted = parseWeightedAudioToken("ClipA:250");
	EXPECT_EQ(weighted.name, "ClipA");
	EXPECT_EQ(weighted.authoredName, "ClipA:250");
	EXPECT_EQ(weighted.weight, 250);

	const auto bare = parseWeightedAudioToken("ClipB");
	EXPECT_EQ(bare.name, "ClipB");
	EXPECT_TRUE(bare.authoredName.empty());
	EXPECT_EQ(bare.weight, 1000);

	for (const char* invalid : {"ClipC:0", "ClipD:-5", "ClipE:abc", ":100"})
	{
		const auto parsed = parseWeightedAudioToken(invalid);
		EXPECT_EQ(parsed.name, invalid);
		EXPECT_TRUE(parsed.authoredName.empty());
		EXPECT_EQ(parsed.weight, 1000);
	}
}

TEST(WeightedAudioRules, SingleChildAlwaysSelectsZero)
{
	const std::vector<int> single{500};
	int calls = 0;
	const auto failInt = [&](int, int) { ++calls; return 0; };
	const auto failReal = [&](float, float) { ++calls; return 0.0f; };

	EXPECT_EQ(pickWeightedAudioIndex({}, -1, failInt, failReal), -1);
	EXPECT_EQ(pickWeightedAudioIndex(single, -1, failInt, failReal), 0);
	EXPECT_EQ(pickWeightedAudioIndex(single, 0, failInt, failReal), 0);
	EXPECT_EQ(calls, 0);
}

TEST(WeightedAudioRules, DrawsProportionallyToWeights)
{
	const std::vector<int> weights{100, 300, 600};
	auto pickAt = [&](int value)
	{
		int observedLo = -1;
		int observedHi = -1;
		const int idx = pickWeightedAudioIndex(
			weights,
			-1,
			[&](int lo, int hi) {
				observedLo = lo;
				observedHi = hi;
				return value;
			},
			[](float, float) { return 0.0f; });
		EXPECT_EQ(observedLo, 0);
		EXPECT_EQ(observedHi, 999);
		return idx;
	};

	EXPECT_EQ(pickAt(0), 0);
	EXPECT_EQ(pickAt(99), 0);
	EXPECT_EQ(pickAt(100), 1);
	EXPECT_EQ(pickAt(399), 1);
	EXPECT_EQ(pickAt(400), 2);
	EXPECT_EQ(pickAt(999), 2);
}

TEST(WeightedAudioRules, ExcludesPreviousChildOnTwoOrMoreChildren)
{
	const std::vector<int> twoSkewed{999, 1};
	int hiSeen = -1;
	EXPECT_EQ(
		pickWeightedAudioIndex(
			twoSkewed,
			0,
			[&](int lo, int hi) {
				EXPECT_EQ(lo, 0);
				hiSeen = hi;
				return hi;
			},
			[](float, float) { return 0.0f; }),
		1);
	EXPECT_EQ(hiSeen, 0);

	std::mt19937 rng(1453);
	const std::vector<int> multi{900, 200, 50};
	int previous = -1;
	int counts[3]{};
	for (int step = 0; step < 300; ++step)
	{
		const int next = pickWeightedAudioIndex(
			multi,
			previous,
			[&](int lo, int hi) {
				std::uniform_int_distribution<int> dist(lo, hi);
				return dist(rng);
			},
			[](float, float) { return 0.0f; });
		ASSERT_GE(next, 0);
		ASSERT_LT(next, 3);
		EXPECT_NE(next, previous);
		++counts[next];
		previous = next;
	}
	EXPECT_GT(counts[0], counts[1]);
	EXPECT_GT(counts[1], counts[2]);
}

TEST(WeightedAudioRules, UsesRealFallbackWhenWeightSumExceedsIntMax)
{
	const std::vector<int> huge{std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
	int intCalls = 0;
	const int pick = pickWeightedAudioIndex(
		huge,
		-1,
		[&](int, int) { ++intCalls; return 0; },
		[](float lo, float hi) {
			EXPECT_FLOAT_EQ(lo, 0.0f);
			EXPECT_FLOAT_EQ(hi, 1.0f);
			return 0.75f;
		});
	EXPECT_EQ(intCalls, 0);
	EXPECT_EQ(pick, 1);
}
