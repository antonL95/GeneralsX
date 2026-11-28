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

TEST(UiRadialRing, ClampsToViewportAcrossSupportedResolutions)
{
	struct Resolution
	{
		int width;
		int height;
	};
	const Resolution resolutions[] = {
		{ 1024, 768 },
		{ 1280, 720 },
		{ 1440, 900 },
		{ 1600, 900 },
		{ 1920, 1080 },
		{ 2560, 1440 },
	};

	for (const Resolution &res : resolutions)
	{
		const double scale = static_cast<double>(res.height) / 768.0;
		const std::pair<int, int> centers[] = {
			{ res.width / 2, 0 },
			{ res.width / 2, 20 },
			{ res.width / 2, res.height },
			{ 0, res.height / 2 },
			{ res.width, res.height / 2 },
			{ 10, 10 },
			{ res.width - 10, res.height - 10 },
		};
		for (const bool compact : { false, true })
		{
			for (const auto &[cx, cy] : centers)
			{
				const auto buttons = UiRadialRing::layout(8, cx, cy, scale, compact, res.width, res.height);
				ASSERT_EQ(buttons.size(), 8u);
				for (const UiRadialRing::PixelButton &b : buttons)
				{
					EXPECT_GE(b.x, 0);
					EXPECT_GE(b.y, 0);
					EXPECT_LE(b.x + b.size, res.width);
					EXPECT_LE(b.y + b.size, res.height);
				}
			}
		}
	}
}

TEST(UiRadialRing, PreservesRelativeOffsetsWhenClampingAndLeavesInteriorUntouched)
{
	const auto unclamped = UiRadialRing::layout(6, 960, 540, 1.4, false, 1920, 1080);
	const auto rawInterior = UiRadialRing::layout(6, 960, 540, 1.4, false);
	ASSERT_EQ(unclamped.size(), rawInterior.size());
	for (size_t i = 0; i < unclamped.size(); ++i)
	{
		EXPECT_EQ(unclamped[i].x, rawInterior[i].x);
		EXPECT_EQ(unclamped[i].y, rawInterior[i].y);
	}

	const auto clampedTop = UiRadialRing::layout(6, 640, 25, 1.0, false, 1280, 720);
	const auto rawTop = UiRadialRing::layout(6, 640, 25, 1.0, false);
	ASSERT_EQ(clampedTop.size(), rawTop.size());
	const int shiftY = clampedTop[0].y - rawTop[0].y;
	EXPECT_GT(shiftY, 0);
	EXPECT_EQ(clampedTop[0].y, 0);
	for (size_t i = 0; i < clampedTop.size(); ++i)
	{
		EXPECT_EQ(clampedTop[i].x, rawTop[i].x);
		EXPECT_EQ(clampedTop[i].y - rawTop[i].y, shiftY);
	}
}
