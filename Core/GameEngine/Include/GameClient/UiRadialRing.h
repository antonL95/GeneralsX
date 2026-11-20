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

	inline std::vector<PixelButton> layout(int count, int centerX, int centerY, double scale)
	{
		std::vector<PixelButton> buttons;
		if (count <= 0 || scale <= 0.0)
			return buttons;
		const double size = ButtonDesignSize * scale;
		const int pixels = static_cast<int>(std::floor(size + 0.5));
		const double ring = radius(count, size);
		buttons.reserve(count);
		for (int i = 0; i < count; ++i)
		{
			const double angle = TwoPi * i / count;
			const double x = centerX + std::sin(angle) * ring;
			const double y = centerY - std::cos(angle) * ring;
			buttons.push_back(PixelButton{ static_cast<int>(std::floor(x - pixels * 0.5 + 0.5)),
				static_cast<int>(std::floor(y - pixels * 0.5 + 0.5)), pixels });
		}
		return buttons;
	}
}
