#include "GameClient/UiNineSlice.h"

#include <algorithm>
#include <cmath>

namespace UiNineSlice
{
	namespace
	{
		bool equalsNoCase(std::string_view lhs, std::string_view rhs)
		{
			return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), [](char a, char b) {
				const auto fold = [](char ch) { return ch >= 'a' && ch <= 'z' ? static_cast<char>(ch - 'a' + 'A') : ch; };
				return fold(a) == fold(b);
			});
		}

		void addPiece(std::vector<Piece>& pieces, const Span& x, const Span& y)
		{
			if (x.dest1 <= x.dest0 || y.dest1 <= y.dest0)
				return;
			pieces.push_back({ { x.dest0, y.dest0, x.dest1, y.dest1 }, { x.source0, y.source0, x.source1, y.source1 } });
		}
	}

	int roundToPixel(double value)
	{
		return static_cast<int>(std::floor(value + 0.5));
	}

	Axis splitAxis(int sourceSize, int insetLow, int insetHigh, int dest0, int dest1, double scale)
	{
		sourceSize = std::max(0, sourceSize);
		insetLow = std::clamp(insetLow, 0, sourceSize);
		insetHigh = std::clamp(insetHigh, 0, sourceSize - insetLow);
		dest1 = std::max(dest0, dest1);
		if (!(scale > 0.0))
			scale = 1.0;

		const int available = dest1 - dest0;
		int low = roundToPixel(insetLow * scale);
		int high = roundToPixel(insetHigh * scale);
		if (low + high > available)
		{
			low = roundToPixel(available * static_cast<double>(insetLow) / (insetLow + insetHigh));
			high = available - low;
		}

		Axis axis;
		axis.low = { dest0, dest0 + low, 0.0, static_cast<double>(insetLow) };
		axis.middle = { dest0 + low, dest1 - high, static_cast<double>(insetLow), static_cast<double>(sourceSize - insetHigh) };
		axis.high = { dest1 - high, dest1, static_cast<double>(sourceSize - insetHigh), static_cast<double>(sourceSize) };
		return axis;
	}

	std::vector<Span> fillSpans(const Span& middle, double scale, Fill fill)
	{
		std::vector<Span> spans;
		if (middle.dest1 <= middle.dest0)
			return spans;

		const double sourceLength = middle.source1 - middle.source0;
		const double tile = sourceLength * (scale > 0.0 ? scale : 1.0);
		const double count = tile >= 1.0 ? std::ceil((middle.dest1 - middle.dest0) / tile) : 0.0;
		if (fill == Fill::Stretch || count < 1.0 || count > MaxTilesPerAxis)
		{
			spans.push_back(middle);
			return spans;
		}

		for (int index = 0; index < static_cast<int>(count); ++index)
		{
			const int start = middle.dest0 + roundToPixel(index * tile);
			const int fullEnd = middle.dest0 + roundToPixel((index + 1) * tile);
			const int end = std::min(fullEnd, middle.dest1);
			if (end <= start)
				continue;
			const double sourceEnd = end == fullEnd ? middle.source1
				: middle.source0 + sourceLength * (end - start) / (fullEnd - start);
			spans.push_back({ start, end, middle.source0, sourceEnd });
		}
		return spans;
	}

	std::vector<Piece> slice(int sourceWidth, int sourceHeight, const Insets& insets, const Rect& dest,
		double scale, Fill edges, Fill center)
	{
		const Axis x = splitAxis(sourceWidth, insets.left, insets.right, dest.x0, dest.x1, scale);
		const Axis y = splitAxis(sourceHeight, insets.top, insets.bottom, dest.y0, dest.y1, scale);
		const std::vector<Span> xEdge = fillSpans(x.middle, scale, edges);
		const std::vector<Span> yEdge = fillSpans(y.middle, scale, edges);
		const std::vector<Span> xCenter = fillSpans(x.middle, scale, center);
		const std::vector<Span> yCenter = fillSpans(y.middle, scale, center);

		std::vector<Piece> pieces;
		addPiece(pieces, x.low, y.low);
		for (const Span& span : xEdge)
			addPiece(pieces, span, y.low);
		addPiece(pieces, x.high, y.low);

		for (const Span& span : yEdge)
			addPiece(pieces, x.low, span);
		for (const Span& row : yCenter)
		{
			for (const Span& column : xCenter)
				addPiece(pieces, column, row);
		}
		for (const Span& span : yEdge)
			addPiece(pieces, x.high, span);

		addPiece(pieces, x.low, y.high);
		for (const Span& span : xEdge)
			addPiece(pieces, span, y.high);
		addPiece(pieces, x.high, y.high);
		return pieces;
	}

	bool parseFill(std::string_view token, Fill& fill)
	{
		if (equalsNoCase(token, "STRETCH"))
		{
			fill = Fill::Stretch;
			return true;
		}
		if (equalsNoCase(token, "TILE"))
		{
			fill = Fill::Tile;
			return true;
		}
		return false;
	}
}
