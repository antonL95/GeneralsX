#include "GameClient/UiNineSlice.h"
#include "GameClient/UiScale.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

namespace
{
	using UiNineSlice::Fill;
	using UiNineSlice::Insets;
	using UiNineSlice::Piece;
	using UiNineSlice::Rect;
	using UiNineSlice::Span;

	std::vector<int> coverage(const std::vector<Piece>& pieces, const Rect& rect)
	{
		const int width = rect.x1 - rect.x0;
		const int height = rect.y1 - rect.y0;
		std::vector<int> counts(static_cast<size_t>(width * height), 0);
		for (const Piece& piece : pieces)
		{
			for (int y = piece.dest.y0; y < piece.dest.y1; ++y)
			{
				for (int x = piece.dest.x0; x < piece.dest.x1; ++x)
				{
					if (x >= rect.x0 && x < rect.x1 && y >= rect.y0 && y < rect.y1)
						++counts[static_cast<size_t>((y - rect.y0) * width + (x - rect.x0))];
				}
			}
		}
		return counts;
	}

	void expectExactCover(const std::vector<Piece>& pieces, const Rect& rect)
	{
		for (const Piece& piece : pieces)
		{
			EXPECT_GE(piece.dest.x0, rect.x0);
			EXPECT_LE(piece.dest.x1, rect.x1);
			EXPECT_GE(piece.dest.y0, rect.y0);
			EXPECT_LE(piece.dest.y1, rect.y1);
		}
		const std::vector<int> counts = coverage(pieces, rect);
		EXPECT_TRUE(std::all_of(counts.begin(), counts.end(), [](int count) { return count == 1; }));
	}

	void expectContiguous(const std::vector<Span>& spans, int dest0, int dest1)
	{
		ASSERT_FALSE(spans.empty());
		EXPECT_EQ(spans.front().dest0, dest0);
		EXPECT_EQ(spans.back().dest1, dest1);
		for (size_t index = 1; index < spans.size(); ++index)
			EXPECT_EQ(spans[index].dest0, spans[index - 1].dest1);
	}
}

TEST(UiNineSlice, CornersScaleWithTheUiScale)
{
	const UiNineSlice::Axis axis = UiNineSlice::splitAxis(170, 24, 24, 100, 500, 1.40625);
	EXPECT_EQ(axis.low.dest0, 100);
	EXPECT_EQ(axis.low.dest1, 134);
	EXPECT_EQ(axis.middle.dest0, 134);
	EXPECT_EQ(axis.middle.dest1, 466);
	EXPECT_EQ(axis.high.dest0, 466);
	EXPECT_EQ(axis.high.dest1, 500);
	EXPECT_DOUBLE_EQ(axis.low.source1, 24.0);
	EXPECT_DOUBLE_EQ(axis.middle.source0, 24.0);
	EXPECT_DOUBLE_EQ(axis.middle.source1, 146.0);
	EXPECT_DOUBLE_EQ(axis.high.source1, 170.0);
}

TEST(UiNineSlice, CornersAtNativeScaleKeepSourcePixels)
{
	const UiNineSlice::Axis axis = UiNineSlice::splitAxis(52, 10, 12, 0, 52, 1.0);
	EXPECT_EQ(axis.low.dest1, 10);
	EXPECT_EQ(axis.high.dest0, 40);
	EXPECT_EQ(axis.middle.dest1 - axis.middle.dest0, 30);
}

TEST(UiNineSlice, SmallRectsShrinkCornersProportionally)
{
	const UiNineSlice::Axis axis = UiNineSlice::splitAxis(100, 30, 10, 0, 21, 2.0);
	EXPECT_EQ(axis.low.dest1, 16);
	EXPECT_EQ(axis.high.dest0, 16);
	EXPECT_EQ(axis.high.dest1, 21);
	EXPECT_EQ(axis.middle.dest0, axis.middle.dest1);
}

TEST(UiNineSlice, InsetsAreClampedToTheSource)
{
	const UiNineSlice::Axis axis = UiNineSlice::splitAxis(20, 15, 15, 0, 100, 1.0);
	EXPECT_DOUBLE_EQ(axis.low.source1, 15.0);
	EXPECT_DOUBLE_EQ(axis.high.source0, 15.0);
	EXPECT_DOUBLE_EQ(axis.high.source1, 20.0);
	EXPECT_EQ(axis.low.dest1, 15);
	EXPECT_EQ(axis.high.dest0, 95);
}

TEST(UiNineSlice, InvalidScaleFallsBackToOne)
{
	const UiNineSlice::Axis axis = UiNineSlice::splitAxis(40, 8, 8, 0, 100, 0.0);
	EXPECT_EQ(axis.low.dest1, 8);
	EXPECT_EQ(axis.high.dest0, 92);
}

TEST(UiNineSlice, StretchIsOneSpan)
{
	const Span middle = { 10, 200, 24.0, 146.0 };
	const std::vector<Span> spans = UiNineSlice::fillSpans(middle, 1.5, Fill::Stretch);
	ASSERT_EQ(spans.size(), 1u);
	EXPECT_EQ(spans[0].dest0, 10);
	EXPECT_EQ(spans[0].dest1, 200);
	EXPECT_DOUBLE_EQ(spans[0].source0, 24.0);
	EXPECT_DOUBLE_EQ(spans[0].source1, 146.0);
}

TEST(UiNineSlice, TilesRepeatAtScaledSizeAndClipTheLast)
{
	const Span middle = { 0, 100, 20.0, 50.0 };
	const std::vector<Span> spans = UiNineSlice::fillSpans(middle, 1.5, Fill::Tile);
	ASSERT_EQ(spans.size(), 3u);
	expectContiguous(spans, 0, 100);
	EXPECT_EQ(spans[0].dest1, 45);
	EXPECT_EQ(spans[1].dest1, 90);
	EXPECT_DOUBLE_EQ(spans[0].source1, 50.0);
	EXPECT_DOUBLE_EQ(spans[1].source1, 50.0);
	EXPECT_DOUBLE_EQ(spans[2].source0, 20.0);
	EXPECT_DOUBLE_EQ(spans[2].source1, 20.0 + 30.0 * 10.0 / 45.0);
}

TEST(UiNineSlice, FractionalTilesRoundByEdgesWithoutGaps)
{
	const Span middle = { 7, 300, 0.0, 13.0 };
	const std::vector<Span> spans = UiNineSlice::fillSpans(middle, 1.40625, Fill::Tile);
	expectContiguous(spans, 7, 300);
	for (const Span& span : spans)
	{
		EXPECT_GE(span.dest1 - span.dest0, 1);
		EXPECT_LE(span.dest1 - span.dest0, 19);
	}
}

TEST(UiNineSlice, TilesThinnerThanAPixelStretch)
{
	const Span middle = { 0, 50, 10.0, 10.5 };
	EXPECT_EQ(UiNineSlice::fillSpans(middle, 1.0, Fill::Tile).size(), 1u);
}

TEST(UiNineSlice, TooManyTilesStretch)
{
	const Span middle = { 0, 5000, 0.0, 2.0 };
	EXPECT_EQ(UiNineSlice::fillSpans(middle, 1.0, Fill::Tile).size(), 1u);
}

TEST(UiNineSlice, EmptyMiddleHasNoSpans)
{
	const Span middle = { 30, 30, 10.0, 20.0 };
	EXPECT_TRUE(UiNineSlice::fillSpans(middle, 1.0, Fill::Tile).empty());
	EXPECT_TRUE(UiNineSlice::fillSpans(middle, 1.0, Fill::Stretch).empty());
}

TEST(UiNineSlice, StretchedFrameHasNinePiecesCoveringTheRect)
{
	const Rect rect = { 13, 21, 413, 121 };
	const Insets insets = { 24, 10, 24, 10 };
	const std::vector<Piece> pieces = UiNineSlice::slice(170, 52, insets, rect, 1.40625, Fill::Stretch, Fill::Stretch);
	ASSERT_EQ(pieces.size(), 9u);
	expectExactCover(pieces, rect);
	EXPECT_EQ(pieces[0].dest.x1 - pieces[0].dest.x0, 34);
	EXPECT_EQ(pieces[0].dest.y1 - pieces[0].dest.y0, 14);
	EXPECT_DOUBLE_EQ(pieces[0].source.x1, 24.0);
	EXPECT_DOUBLE_EQ(pieces[8].source.x0, 146.0);
	EXPECT_DOUBLE_EQ(pieces[8].source.y1, 52.0);
}

TEST(UiNineSlice, TiledFrameCoversTheRectAtEveryResolution)
{
	const Insets insets = { 20, 21, 20, 21 };
	const int resolutions[][2] = { { 1024, 768 }, { 1920, 1080 }, { 2560, 1440 }, { 3440, 1440 } };
	for (const auto& resolution : resolutions)
	{
		const double scale = UiScale::scale(resolution[0], resolution[1]);
		const UiScale::PixelRect pixels = UiScale::canvasToScreen(103, 97, 433, 371, resolution[0], resolution[1]);
		const Rect rect = { pixels.x0, pixels.y0, pixels.x1, pixels.y1 };
		const std::vector<Piece> pieces = UiNineSlice::slice(42, 200, insets, rect, scale, Fill::Tile, Fill::Tile);
		expectExactCover(pieces, rect);
	}
}

TEST(UiNineSlice, MixedFillsCoverTheRect)
{
	const Rect rect = { 0, 0, 301, 203 };
	const Insets insets = { 16, 16, 16, 16 };
	expectExactCover(UiNineSlice::slice(141, 141, insets, rect, 1.875, Fill::Tile, Fill::Stretch), rect);
	expectExactCover(UiNineSlice::slice(141, 141, insets, rect, 1.875, Fill::Stretch, Fill::Tile), rect);
}

TEST(UiNineSlice, ThreeSliceWithoutVerticalInsets)
{
	const Rect rect = { 0, 0, 300, 40 };
	const Insets insets = { 20, 0, 20, 0 };
	const std::vector<Piece> pieces = UiNineSlice::slice(196, 42, insets, rect, 1.0, Fill::Stretch, Fill::Stretch);
	ASSERT_EQ(pieces.size(), 3u);
	expectExactCover(pieces, rect);
	EXPECT_DOUBLE_EQ(pieces[1].source.y0, 0.0);
	EXPECT_DOUBLE_EQ(pieces[1].source.y1, 42.0);
}

TEST(UiNineSlice, DegenerateRectsProduceNothing)
{
	const Insets insets = { 4, 4, 4, 4 };
	EXPECT_TRUE(UiNineSlice::slice(32, 32, insets, { 10, 10, 10, 50 }, 1.0, Fill::Stretch, Fill::Stretch).empty());
	EXPECT_TRUE(UiNineSlice::slice(32, 32, insets, { 10, 50, 40, 10 }, 1.0, Fill::Tile, Fill::Tile).empty());
}

TEST(UiNineSlice, TinyRectsStillCoverWithoutOverlap)
{
	const Insets insets = { 24, 10, 24, 10 };
	for (int width = 1; width <= 60; ++width)
	{
		const Rect rect = { 5, 5, 5 + width, 5 + width / 2 + 1 };
		expectExactCover(UiNineSlice::slice(170, 52, insets, rect, 2.8125, Fill::Tile, Fill::Tile), rect);
	}
}

TEST(UiNineSlice, FillNamesParseCaseInsensitively)
{
	Fill fill = Fill::Stretch;
	EXPECT_TRUE(UiNineSlice::parseFill("tile", fill));
	EXPECT_EQ(fill, Fill::Tile);
	EXPECT_TRUE(UiNineSlice::parseFill("STRETCH", fill));
	EXPECT_EQ(fill, Fill::Stretch);
	EXPECT_FALSE(UiNineSlice::parseFill("REPEAT", fill));
	EXPECT_EQ(fill, Fill::Stretch);
}
