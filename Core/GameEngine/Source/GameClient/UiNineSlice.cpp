#include "GameClient/UiNineSlice.h"

#include <algorithm>
#include <cmath>

namespace UiNineSlice
{
	namespace
	{
		constexpr double MaxSpanCount = 1.0e9;

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

		double validScale(double scale)
		{
			return scale > 0.0 && std::isfinite(scale) ? scale : 1.0;
		}

		bool repeats(const Span& middle, Fill fill)
		{
			return fill == Fill::Tile && middle.dest1 > middle.dest0 && middle.source1 > middle.source0;
		}

		double tileLength(const Span& middle, double scale)
		{
			return (middle.source1 - middle.source0) * scale;
		}

		double spanCount(const Span& middle, double scale, Fill fill, double factor)
		{
			if (middle.dest1 <= middle.dest0)
				return 0.0;
			if (!repeats(middle, fill))
				return 1.0;
			return std::min(MaxSpanCount, std::ceil((middle.dest1 - middle.dest0) / (tileLength(middle, scale) * factor)));
		}

		bool tilesCoverAPixel(const Span& middle, double scale, Fill fill, double factor)
		{
			return !repeats(middle, fill) || tileLength(middle, scale) * factor >= 1.0;
		}

		template <typename Fits>
		double searchSmallestFactor(Fits fits)
		{
			if (fits(1.0))
				return 1.0;
			double low = 1.0;
			double high = 2.0;
			int doublings = 1;
			while (!fits(high))
			{
				if (doublings >= MaxFactorDoublings)
					return high;
				low = high;
				high *= 2.0;
				++doublings;
			}
			while (high - low > 1.0)
			{
				const double middle = std::floor((low + high) / 2.0);
				if (fits(middle))
					high = middle;
				else
					low = middle;
			}
			return high;
		}
	}

	double smallestFactor(bool (*fits)(double factor, const void* context), const void* context)
	{
		return searchSmallestFactor([fits, context](double factor) { return fits(factor, context); });
	}

	int roundToPixel(double value)
	{
		if (std::isnan(value))
			return 0;
		const double rounded = std::floor(value + 0.5);
		return static_cast<int>(std::clamp(rounded, static_cast<double>(-MaxPixel), static_cast<double>(MaxPixel)));
	}

	double clampCornerScale(double scale)
	{
		if (!(scale > 0.0))
			return 1.0;
		return std::clamp(scale, MinCornerScale, MaxCornerScale);
	}

	Axis splitAxis(int sourceSize, int insetLow, int insetHigh, int dest0, int dest1, double scale)
	{
		sourceSize = std::max(0, sourceSize);
		insetLow = std::clamp(insetLow, 0, sourceSize);
		insetHigh = std::clamp(insetHigh, 0, sourceSize - insetLow);
		dest1 = std::max(dest0, dest1);
		scale = validScale(scale);

		const int available = dest1 - dest0;
		int low = roundToPixel(insetLow * scale);
		int high = roundToPixel(insetHigh * scale);
		if (static_cast<long long>(low) + high > available)
		{
			low = roundToPixel(available * static_cast<double>(insetLow) / (static_cast<double>(insetLow) + insetHigh));
			high = available - low;
		}

		Axis axis;
		axis.low = { dest0, dest0 + low, 0.0, static_cast<double>(insetLow) };
		axis.middle = { dest0 + low, dest1 - high, static_cast<double>(insetLow), static_cast<double>(sourceSize - insetHigh) };
		axis.high = { dest1 - high, dest1, static_cast<double>(sourceSize - insetHigh), static_cast<double>(sourceSize) };
		return axis;
	}

	double tileFactor(const Span& middle, double scale, Fill fill)
	{
		scale = validScale(scale);
		return searchSmallestFactor([&](double factor) {
			return tilesCoverAPixel(middle, scale, fill, factor) && spanCount(middle, scale, fill, factor) <= MaxPieces;
		});
	}

	std::vector<Span> fillSpans(const Span& middle, double scale, Fill fill)
	{
		return fillSpansWithFactor(middle, validScale(scale), fill, tileFactor(middle, scale, fill));
	}

	std::vector<Span> fillSpansWithFactor(const Span& middle, double scale, Fill fill, double factor)
	{
		std::vector<Span> spans;
		if (middle.dest1 <= middle.dest0)
			return spans;
		if (!repeats(middle, fill))
		{
			spans.push_back(middle);
			return spans;
		}
		scale = validScale(scale);
		if (!(factor >= 1.0) || !tilesCoverAPixel(middle, scale, fill, factor) || spanCount(middle, scale, fill, factor) > MaxPieces)
			factor = tileFactor(middle, scale, fill);

		const double sourceLength = middle.source1 - middle.source0;
		const double tile = tileLength(middle, scale) * factor;
		const int count = static_cast<int>(spanCount(middle, scale, fill, factor));
		spans.reserve(static_cast<size_t>(count));
		for (int index = 0; index < count; ++index)
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

	double sliceTileFactor(const Axis& x, const Axis& y, double scale, Fill edges, Fill center)
	{
		scale = validScale(scale);
		return searchSmallestFactor([&](double factor) {
			if (!tilesCoverAPixel(x.middle, scale, edges, factor) || !tilesCoverAPixel(y.middle, scale, edges, factor) ||
				!tilesCoverAPixel(x.middle, scale, center, factor) || !tilesCoverAPixel(y.middle, scale, center, factor))
				return false;
			const double edgePieces = 2.0 * (spanCount(x.middle, scale, edges, factor) + spanCount(y.middle, scale, edges, factor));
			const double centerPieces = spanCount(x.middle, scale, center, factor) * spanCount(y.middle, scale, center, factor);
			return 4.0 + edgePieces + centerPieces <= MaxPieces;
		});
	}

	std::vector<Piece> slice(int sourceWidth, int sourceHeight, const Insets& insets, const Rect& dest,
		double scale, Fill edges, Fill center)
	{
		const Axis x = splitAxis(sourceWidth, insets.left, insets.right, dest.x0, dest.x1, scale);
		const Axis y = splitAxis(sourceHeight, insets.top, insets.bottom, dest.y0, dest.y1, scale);
		const double tileScale = validScale(scale);
		const double factor = sliceTileFactor(x, y, tileScale, edges, center);
		const std::vector<Span> xEdge = fillSpansWithFactor(x.middle, tileScale, edges, factor);
		const std::vector<Span> yEdge = fillSpansWithFactor(y.middle, tileScale, edges, factor);
		const std::vector<Span> xCenter = fillSpansWithFactor(x.middle, tileScale, center, factor);
		const std::vector<Span> yCenter = fillSpansWithFactor(y.middle, tileScale, center, factor);

		std::vector<Piece> pieces;
		pieces.reserve(4 + 2 * (xEdge.size() + yEdge.size()) + xCenter.size() * yCenter.size());
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
