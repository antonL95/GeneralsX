#pragma once

#include <algorithm>
#include <cmath>

namespace UiScale
{
	constexpr int DesignWidth = 1024;
	constexpr int DesignHeight = 768;

	struct PixelRect
	{
		int x0;
		int y0;
		int x1;
		int y1;
	};

	inline double scale(int width, int height)
	{
		if (width <= 0 || height <= 0)
			return 1.0;
		return std::min(static_cast<double>(width) / DesignWidth, static_cast<double>(height) / DesignHeight);
	}

	inline int toPixels(double designValue, double factor)
	{
		return static_cast<int>(std::floor(designValue * factor + 0.5));
	}

	inline int fontPixelSize(int designSize, int width, int height)
	{
		return std::max(1, toPixels(designSize, scale(width, height)));
	}

	inline PixelRect canvasToScreen(int x0, int y0, int x1, int y1, int width, int height)
	{
		const double factor = scale(width, height);
		const int originX = (width - toPixels(DesignWidth, factor)) / 2;
		const int originY = (height - toPixels(DesignHeight, factor)) / 2;
		return {originX + toPixels(x0, factor), originY + toPixels(y0, factor),
			originX + toPixels(x1, factor), originY + toPixels(y1, factor)};
	}
}
