#include "GameClient/UiPixelSpace.h"

#include <algorithm>
#include <cmath>

namespace UiPixelSpace
{
	namespace
	{
		int mapAxis(double point, int pointSize, int pixelSize, int viewportStart, int viewportSize, int renderSize)
		{
			const double pixel = point * static_cast<double>(pixelSize) / static_cast<double>(pointSize);
			const double local = std::clamp(pixel - viewportStart, 0.0, static_cast<double>(viewportSize));
			const double mapped = std::floor(local * static_cast<double>(renderSize) / static_cast<double>(viewportSize));
			return static_cast<int>(std::clamp(mapped, 0.0, static_cast<double>(renderSize - 1)));
		}
	}

	Point pointToRender(double pointX, double pointY, const WindowSize& window, const Viewport& viewport,
		int renderWidth, int renderHeight)
	{
		if (!std::isfinite(pointX) || !std::isfinite(pointY))
			return {};
		if (window.pointWidth <= 0 || window.pointHeight <= 0 || window.pixelWidth <= 0 || window.pixelHeight <= 0 ||
			renderWidth <= 0 || renderHeight <= 0)
			return { static_cast<int>(pointX), static_cast<int>(pointY) };

		Viewport area = viewport;
		if (area.width <= 0 || area.height <= 0)
			area = { 0, 0, window.pixelWidth, window.pixelHeight };

		return { mapAxis(pointX, window.pointWidth, window.pixelWidth, area.x, area.width, renderWidth),
			mapAxis(pointY, window.pointHeight, window.pixelHeight, area.y, area.height, renderHeight) };
	}

	bool followsWindow(int pixelWidth, int pixelHeight, int renderWidth, int renderHeight)
	{
		if (pixelWidth < MinFollowWidth || pixelHeight < MinFollowHeight)
			return false;
		return pixelWidth != renderWidth || pixelHeight != renderHeight;
	}
}
