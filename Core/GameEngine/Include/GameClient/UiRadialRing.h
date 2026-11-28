#pragma once

#include <cmath>
#include <vector>

namespace UiRadialRing
{
	constexpr double ButtonDesignSize = 55.0;
	constexpr double SpacingFactor = 0.9;
	constexpr double TwoPi = 6.283185307179586;

	struct PixelButton
	{
		int x;
		int y;
		int size;
	};

	inline double radius(int count, double buttonSize)
	{
		if (count <= 0)
			return 0.0;
		return (std::sqrt(count + 0.75) - 1.0) * buttonSize * SpacingFactor;
	}

	inline int clampShift(int minPos, int maxPos, int limit)
	{
		if (limit <= 0)
			return 0;
		if (maxPos - minPos > limit)
			return (limit - (minPos + maxPos)) / 2;
		if (minPos < 0)
			return -minPos;
		if (maxPos > limit)
			return limit - maxPos;
		return 0;
	}

	inline void clampToViewport(std::vector<PixelButton> &buttons, int viewportWidth, int viewportHeight)
	{
		if (buttons.empty() || (viewportWidth <= 0 && viewportHeight <= 0))
			return;
		int minX = buttons.front().x;
		int minY = buttons.front().y;
		int maxX = minX + buttons.front().size;
		int maxY = minY + buttons.front().size;
		for (const PixelButton &button : buttons)
		{
			if (button.x < minX)
				minX = button.x;
			if (button.y < minY)
				minY = button.y;
			if (button.x + button.size > maxX)
				maxX = button.x + button.size;
			if (button.y + button.size > maxY)
				maxY = button.y + button.size;
		}
		const int dx = clampShift(minX, maxX, viewportWidth);
		const int dy = clampShift(minY, maxY, viewportHeight);
		if (dx == 0 && dy == 0)
			return;
		for (PixelButton &button : buttons)
		{
			button.x += dx;
			button.y += dy;
		}
	}

	inline std::vector<PixelButton> layout(int count, int centerX, int centerY, double scale, bool compact = false,
		int viewportWidth = 0, int viewportHeight = 0)
	{
		std::vector<PixelButton> buttons;
		if (count <= 0 || scale <= 0.0)
			return buttons;
		const double size = ButtonDesignSize * scale;
		const int pixels = static_cast<int>(std::floor(size + 0.5));
		const double ring = compact && count > 1 ? size * 1.06 / (2.0 * std::sin(TwoPi / (2.0 * count))) : radius(count, size);
		buttons.reserve(count);
		for (int i = 0; i < count; ++i)
		{
			const double angle = TwoPi * i / count;
			const double x = centerX + std::sin(angle) * ring;
			const double y = centerY - std::cos(angle) * ring;
			buttons.push_back(PixelButton{ static_cast<int>(std::floor(x - pixels * 0.5 + 0.5)),
				static_cast<int>(std::floor(y - pixels * 0.5 + 0.5)), pixels });
		}
		clampToViewport(buttons, viewportWidth, viewportHeight);
		return buttons;
	}
}
