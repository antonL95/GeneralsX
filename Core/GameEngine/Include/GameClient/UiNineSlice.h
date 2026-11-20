#pragma once

#include <string_view>
#include <vector>

namespace UiNineSlice
{
	enum class Fill
	{
		Stretch,
		Tile
	};

	struct Insets
	{
		int left = 0;
		int top = 0;
		int right = 0;
		int bottom = 0;
	};

	struct Rect
	{
		int x0 = 0;
		int y0 = 0;
		int x1 = 0;
		int y1 = 0;
	};

	struct SourceRect
	{
		double x0 = 0.0;
		double y0 = 0.0;
		double x1 = 0.0;
		double y1 = 0.0;
	};

	struct Piece
	{
		Rect dest;
		SourceRect source;
	};

	struct Span
	{
		int dest0 = 0;
		int dest1 = 0;
		double source0 = 0.0;
		double source1 = 0.0;
	};

	struct Axis
	{
		Span low;
		Span middle;
		Span high;
	};

	constexpr int MaxPieces = 4096;
	constexpr double MinCornerScale = 0.25;
	constexpr double MaxCornerScale = 4.0;
	constexpr int MaxPixel = 1 << 24;

	int roundToPixel(double value);
	double clampCornerScale(double scale);
	Axis splitAxis(int sourceSize, int insetLow, int insetHigh, int dest0, int dest1, double scale);
	double tileFactor(const Span& middle, double scale, Fill fill);
	std::vector<Span> fillSpans(const Span& middle, double scale, Fill fill);
	std::vector<Span> fillSpansWithFactor(const Span& middle, double scale, Fill fill, double factor);
	double sliceTileFactor(const Axis& x, const Axis& y, double scale, Fill edges, Fill center);
	std::vector<Piece> slice(int sourceWidth, int sourceHeight, const Insets& insets, const Rect& dest,
		double scale, Fill edges, Fill center);
	bool parseFill(std::string_view token, Fill& fill);
}
