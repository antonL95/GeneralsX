#pragma once

namespace UiPixelSpace
{
	constexpr int MinFollowWidth = 320;
	constexpr int MinFollowHeight = 240;

	struct WindowSize
	{
		int pointWidth = 0;
		int pointHeight = 0;
		int pixelWidth = 0;
		int pixelHeight = 0;
	};

	struct Viewport
	{
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;
	};

	struct Point
	{
		int x = 0;
		int y = 0;
	};

	Point pointToRender(double pointX, double pointY, const WindowSize& window, const Viewport& viewport,
		int renderWidth, int renderHeight);
	bool followsWindow(int pixelWidth, int pixelHeight, int renderWidth, int renderHeight);
}
