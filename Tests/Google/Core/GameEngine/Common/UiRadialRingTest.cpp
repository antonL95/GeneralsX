#include "GameClient/UiRadialRing.h"

#include <gtest/gtest.h>

#include <cmath>

TEST(UiRadialRing, EmptyRingHasNoButtons)
{
	EXPECT_TRUE(UiRadialRing::layout(0, 100, 100, 1.0).empty());
	EXPECT_TRUE(UiRadialRing::layout(6, 100, 100, 0.0).empty());
}

TEST(UiRadialRing, SixButtonsStartAtTwelveOClockAndRunClockwise)
{
	const auto buttons = UiRadialRing::layout(6, 512, 384, 1.0);
	ASSERT_EQ(buttons.size(), 6u);
	const int half = buttons[0].size / 2;
	EXPECT_EQ(buttons[0].size, 55);
	EXPECT_NEAR(buttons[0].x + half, 512, 1);
	EXPECT_LT(buttons[0].y + half, 384 - 70);
	EXPECT_GT(buttons[1].x, buttons[0].x);
	EXPECT_LT(buttons[1].y, 384);
	EXPECT_NEAR(buttons[3].x + half, 512, 1);
	EXPECT_GT(buttons[3].y + half, 384 + 70);
	EXPECT_LT(buttons[5].x, buttons[0].x);
}

TEST(UiRadialRing, RingIsSymmetricAroundTheCenter)
{
	const auto buttons = UiRadialRing::layout(6, 400, 300, 1.5);
	ASSERT_EQ(buttons.size(), 6u);
	for (int i = 0; i < 3; ++i)
	{
		const int sumX = buttons[i].x + buttons[i + 3].x + buttons[i].size;
		const int sumY = buttons[i].y + buttons[i + 3].y + buttons[i].size;
		EXPECT_NEAR(sumX, 800, 1);
		EXPECT_NEAR(sumY, 600, 1);
	}
}

TEST(UiRadialRing, NeighboursNeverOverlapUpToTwelveButtons)
{
	for (int count = 2; count <= 12; ++count)
	{
		const auto buttons = UiRadialRing::layout(count, 1000, 1000, 1.0);
		for (int i = 0; i < count; ++i)
		{
			const auto &a = buttons[i];
			const auto &b = buttons[(i + 1) % count];
			const double dx = a.x - b.x;
			const double dy = a.y - b.y;
			EXPECT_GE(std::sqrt(dx * dx + dy * dy), a.size) << count << " buttons, pair " << i;
		}
	}
}

TEST(UiRadialRing, ButtonsScaleWithTheInterface)
{
	const auto small = UiRadialRing::layout(6, 0, 0, 1.0);
	const auto large = UiRadialRing::layout(6, 0, 0, 2.0);
	EXPECT_EQ(large[0].size, 110);
	EXPECT_NEAR(large[0].y + large[0].size / 2, 2 * (small[0].y + small[0].size / 2), 2);
}
