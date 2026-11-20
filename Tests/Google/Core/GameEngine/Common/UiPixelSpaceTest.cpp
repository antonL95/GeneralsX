#include "GameClient/UiPixelSpace.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace
{
	using UiPixelSpace::Point;
	using UiPixelSpace::Viewport;
	using UiPixelSpace::WindowSize;

	WindowSize window(int pointWidth, int pointHeight, int pixelWidth, int pixelHeight)
	{
		WindowSize size;
		size.pointWidth = pointWidth;
		size.pointHeight = pointHeight;
		size.pixelWidth = pixelWidth;
		size.pixelHeight = pixelHeight;
		return size;
	}

	int previousMapping(float raw, int windowSize, int renderSize)
	{
		float local = raw;
		if (local < 0.0f)
			local = 0.0f;
		if (local > static_cast<float>(windowSize))
			local = static_cast<float>(windowSize);
		return static_cast<int>(local * static_cast<float>(renderSize) / static_cast<float>(windowSize));
	}
}

TEST(UiPixelSpace, DensityOneMatchesThePreviousMappingInsideTheWindow)
{
	const struct { int windowWidth, windowHeight, renderWidth, renderHeight; } cases[] = {
		{ 1024, 768, 1024, 768 },
		{ 1920, 1080, 1920, 1080 },
		{ 1280, 960, 1024, 768 },
		{ 800, 600, 1600, 1200 },
	};
	for (const auto& entry : cases)
	{
		const WindowSize size = window(entry.windowWidth, entry.windowHeight, entry.windowWidth, entry.windowHeight);
		for (int step = 0; step < entry.windowWidth * 4; step += 3)
		{
			const float raw = static_cast<float>(step) / 4.0f;
			const Point point = UiPixelSpace::pointToRender(raw, raw * entry.windowHeight / entry.windowWidth, size, Viewport(),
				entry.renderWidth, entry.renderHeight);
			EXPECT_EQ(point.x, previousMapping(raw, entry.windowWidth, entry.renderWidth)) << raw;
		}
	}
}

TEST(UiPixelSpace, BackingScaleTwoMapsPointsToPixels)
{
	const WindowSize size = window(1280, 720, 2560, 1440);
	const Point point = UiPixelSpace::pointToRender(100.25, 50.5, size, Viewport(), 2560, 1440);
	EXPECT_EQ(point.x, 200);
	EXPECT_EQ(point.y, 101);

	const Point corner = UiPixelSpace::pointToRender(0.0, 0.0, size, Viewport(), 2560, 1440);
	EXPECT_EQ(corner.x, 0);
	EXPECT_EQ(corner.y, 0);
}

TEST(UiPixelSpace, BackingScaleTwoReachesEveryOddPixelThroughHalfPoints)
{
	const WindowSize size = window(1280, 720, 2560, 1440);
	for (int pixel = 0; pixel < 2560; ++pixel)
	{
		const Point point = UiPixelSpace::pointToRender((pixel + 0.25) / 2.0, 0.0, size, Viewport(), 2560, 1440);
		EXPECT_EQ(point.x, pixel);
	}
}

TEST(UiPixelSpace, PillarboxViewportIsMeasuredInPixels)
{
	const WindowSize size = window(1440, 900, 2880, 1800);
	Viewport viewport;
	viewport.x = 0;
	viewport.y = 90;
	viewport.width = 2880;
	viewport.height = 1620;

	const Point center = UiPixelSpace::pointToRender(720.0, 450.0, size, viewport, 1920, 1080);
	EXPECT_EQ(center.x, 960);
	EXPECT_EQ(center.y, 540);

	const Point band = UiPixelSpace::pointToRender(10.0, 20.0, size, viewport, 1920, 1080);
	EXPECT_EQ(band.x, 13);
	EXPECT_EQ(band.y, 0);

	const Point bottom = UiPixelSpace::pointToRender(1439.0, 899.0, size, viewport, 1920, 1080);
	EXPECT_EQ(bottom.x, 1918);
	EXPECT_EQ(bottom.y, 1079);
}

TEST(UiPixelSpace, EdgesStayInsideTheRenderTarget)
{
	const WindowSize size = window(1280, 720, 2560, 1440);
	const Point edge = UiPixelSpace::pointToRender(1280.0, 720.0, size, Viewport(), 2560, 1440);
	EXPECT_EQ(edge.x, 2559);
	EXPECT_EQ(edge.y, 1439);

	const Point outside = UiPixelSpace::pointToRender(-40.0, 5000.0, size, Viewport(), 2560, 1440);
	EXPECT_EQ(outside.x, 0);
	EXPECT_EQ(outside.y, 1439);
}

TEST(UiPixelSpace, UnknownWindowSizesPassThePointThrough)
{
	const Point point = UiPixelSpace::pointToRender(12.75, 34.5, window(0, 0, 0, 0), Viewport(), 1024, 768);
	EXPECT_EQ(point.x, 12);
	EXPECT_EQ(point.y, 34);

	const Point noRender = UiPixelSpace::pointToRender(12.75, 34.5, window(100, 100, 200, 200), Viewport(), 0, 0);
	EXPECT_EQ(noRender.x, 12);
	EXPECT_EQ(noRender.y, 34);
}

TEST(UiPixelSpace, NonFinitePointsMapToTheOrigin)
{
	const WindowSize size = window(1280, 720, 2560, 1440);
	const double nan = std::numeric_limits<double>::quiet_NaN();
	const double infinity = std::numeric_limits<double>::infinity();
	const Point a = UiPixelSpace::pointToRender(nan, 10.0, size, Viewport(), 2560, 1440);
	const Point b = UiPixelSpace::pointToRender(10.0, infinity, size, Viewport(), 2560, 1440);
	EXPECT_EQ(a.x, 0);
	EXPECT_EQ(a.y, 0);
	EXPECT_EQ(b.x, 0);
	EXPECT_EQ(b.y, 0);
}

TEST(UiPixelSpace, RenderFollowsTheWindowOnlyWhenItDiffersAndIsLargeEnough)
{
	EXPECT_TRUE(UiPixelSpace::followsWindow(2560, 1440, 1024, 768));
	EXPECT_FALSE(UiPixelSpace::followsWindow(1024, 768, 1024, 768));
	EXPECT_FALSE(UiPixelSpace::followsWindow(319, 768, 1024, 768));
	EXPECT_FALSE(UiPixelSpace::followsWindow(1024, 239, 1024, 768));
	EXPECT_TRUE(UiPixelSpace::followsWindow(320, 240, 1024, 768));
}
