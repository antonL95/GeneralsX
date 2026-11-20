#include "GameClient/UiAnchor.h"
#include "GameClient/UiScale.h"

#include <gtest/gtest.h>

#include <string>

namespace
{
	using UiAnchor::Keyword;
	using UiAnchor::Spec;
	using UiScale::PixelRect;

	Spec parse(std::initializer_list<std::pair<const char*, const char*>> fields)
	{
		Spec spec;
		for (const auto& field : fields)
		{
			std::string error;
			EXPECT_TRUE(UiAnchor::applyField(spec, UiAnchor::keywordOf(field.first), field.second, error))
				<< field.first << " = " << field.second << ": " << error;
		}
		return spec;
	}

	std::string failure(std::initializer_list<std::pair<const char*, const char*>> fields)
	{
		Spec spec;
		std::string error;
		for (const auto& field : fields)
		{
			if (!UiAnchor::applyField(spec, UiAnchor::keywordOf(field.first), field.second, error))
				return error;
		}
		return std::string();
	}

	std::string invalid(const Spec& spec, bool hasParent)
	{
		std::string error;
		return UiAnchor::validate(spec, hasParent, error) ? std::string() : error;
	}

	void expectRect(const PixelRect& rect, int x0, int y0, int x1, int y1)
	{
		EXPECT_EQ(rect.x0, x0);
		EXPECT_EQ(rect.y0, y0);
		EXPECT_EQ(rect.x1, x1);
		EXPECT_EQ(rect.y1, y1);
	}
}

TEST(UiAnchor, KeywordsAreTheWndSpellings)
{
	EXPECT_EQ(UiAnchor::keywordOf("ANCHOR"), Keyword::Anchor);
	EXPECT_EQ(UiAnchor::keywordOf("OFFSET"), Keyword::Offset);
	EXPECT_EQ(UiAnchor::keywordOf("SIZE"), Keyword::Size);
	EXPECT_EQ(UiAnchor::keywordOf("MARGIN"), Keyword::Margin);
	EXPECT_EQ(UiAnchor::keywordOf("MINSIZE"), Keyword::MinSize);
	EXPECT_EQ(UiAnchor::keywordOf("MAXSIZE"), Keyword::MaxSize);
	EXPECT_EQ(UiAnchor::keywordOf("SAFEAREA"), Keyword::SafeArea);
	EXPECT_EQ(UiAnchor::keywordOf("TABORDER"), Keyword::TabOrder);
	EXPECT_EQ(UiAnchor::keywordOf("anchor"), Keyword::None);
	EXPECT_EQ(UiAnchor::keywordOf("SCREENRECT"), Keyword::None);
}

TEST(UiAnchor, ParsesEveryField)
{
	const Spec spec = parse({ { "ANCHOR", "right Middle" }, { "OFFSET", "-12, +8" }, { "SIZE", "170 52" },
		{ "MARGIN", "1 2 3 4" }, { "MINSIZE", "100 0" }, { "MAXSIZE", "300 60" }, { "SAFEAREA", "YES" }, { "TABORDER", "3" } });
	EXPECT_TRUE(spec.anchored());
	EXPECT_EQ(spec.horizontal, UiAnchor::Align::High);
	EXPECT_EQ(spec.vertical, UiAnchor::Align::Center);
	EXPECT_EQ(spec.offset[0], -12);
	EXPECT_EQ(spec.offset[1], 8);
	EXPECT_EQ(spec.size[0], 170);
	EXPECT_EQ(spec.margin[3], 4);
	EXPECT_EQ(spec.minSize[0], 100);
	EXPECT_EQ(spec.maxSize[1], 60);
	EXPECT_TRUE(spec.safeArea);
	EXPECT_EQ(spec.tabOrder, 3);
	EXPECT_EQ(invalid(spec, false), "");
}

TEST(UiAnchor, RejectsMalformedValues)
{
	EXPECT_NE(failure({ { "ANCHOR", "TOP LEFT" } }), "");
	EXPECT_NE(failure({ { "ANCHOR", "LEFT" } }), "");
	EXPECT_NE(failure({ { "ANCHOR", "LEFT TOP BOTTOM" } }), "");
	EXPECT_NE(failure({ { "OFFSET", "1" } }), "");
	EXPECT_NE(failure({ { "OFFSET", "1 2.5" } }), "");
	EXPECT_NE(failure({ { "SIZE", "-1 4" } }), "");
	EXPECT_NE(failure({ { "SIZE", "1 999999" } }), "");
	EXPECT_NE(failure({ { "MARGIN", "1 2 3" } }), "");
	EXPECT_NE(failure({ { "SAFEAREA", "MAYBE" } }), "");
	EXPECT_NE(failure({ { "TABORDER", "-2" } }), "");
	EXPECT_NE(failure({ { "SIZE", "1 2" }, { "SIZE", "3 4" } }), "");
	EXPECT_NE(failure({ { "BOGUS", "1" } }), "");
}

TEST(UiAnchor, ValidationRules)
{
	EXPECT_EQ(invalid(Spec(), false), "");
	EXPECT_EQ(invalid(parse({ { "TABORDER", "1" } }), true), "");
	EXPECT_NE(invalid(parse({ { "OFFSET", "1 2" } }), false), "");
	EXPECT_NE(invalid(parse({ { "SAFEAREA", "YES" } }), false), "");
	EXPECT_NE(invalid(parse({ { "ANCHOR", "LEFT TOP" } }), false), "");
	EXPECT_EQ(invalid(parse({ { "ANCHOR", "STRETCH STRETCH" } }), false), "");
	EXPECT_NE(invalid(parse({ { "ANCHOR", "STRETCH TOP" } }), false), "");
	EXPECT_NE(invalid(parse({ { "ANCHOR", "LEFT TOP" }, { "SIZE", "1 1" }, { "SAFEAREA", "YES" } }), true), "");
	EXPECT_EQ(invalid(parse({ { "ANCHOR", "LEFT TOP" }, { "SIZE", "1 1" }, { "SAFEAREA", "NO" } }), true), "");
	EXPECT_NE(invalid(parse({ { "ANCHOR", "STRETCH STRETCH" }, { "MINSIZE", "50 0" }, { "MAXSIZE", "40 0" } }), false), "");
	EXPECT_EQ(invalid(parse({ { "ANCHOR", "STRETCH STRETCH" }, { "MINSIZE", "0 50" }, { "MAXSIZE", "40 0" } }), false), "");
}

TEST(UiAnchor, SafeAreaCapsWidthAt16By9)
{
	const UiAnchor::SafeAreaSettings settings;
	expectRect(UiAnchor::safeArea(1024, 768, settings), 0, 0, 1024, 768);
	expectRect(UiAnchor::safeArea(1920, 1080, settings), 0, 0, 1920, 1080);
	expectRect(UiAnchor::safeArea(1920, 1200, settings), 0, 0, 1920, 1200);
	expectRect(UiAnchor::safeArea(3840, 2160, settings), 0, 0, 3840, 2160);
	expectRect(UiAnchor::safeArea(2560, 1080, settings), 320, 0, 2240, 1080);
	expectRect(UiAnchor::safeArea(3440, 1440, settings), 440, 0, 3000, 1440);
	expectRect(UiAnchor::safeArea(5120, 1440, settings), 1280, 0, 3840, 1440);
	expectRect(UiAnchor::safeArea(1366, 768, settings), 0, 0, 1365, 768);
	expectRect(UiAnchor::safeArea(1367, 768, settings), 1, 0, 1366, 768);
}

TEST(UiAnchor, SafeAreaCanBeTurnedOffOrInset)
{
	UiAnchor::SafeAreaSettings off;
	off.aspectWidth = 0;
	expectRect(UiAnchor::safeArea(2560, 1080, off), 0, 0, 2560, 1080);

	UiAnchor::SafeAreaSettings inset;
	inset.margin = 16;
	expectRect(UiAnchor::safeArea(2560, 1080, inset), 343, 23, 2217, 1057);
}

TEST(UiAnchor, CornersMeasureInwardFromTheirEdges)
{
	const double scale = UiScale::scale(1920, 1080);
	const PixelRect screen = UiAnchor::screenArea(1920, 1080);
	expectRect(UiAnchor::resolve(parse({ { "ANCHOR", "LEFT TOP" }, { "OFFSET", "16 16" }, { "SIZE", "170 52" } }), screen, scale),
		23, 23, 262, 96);
	expectRect(UiAnchor::resolve(parse({ { "ANCHOR", "RIGHT BOTTOM" }, { "OFFSET", "16 16" }, { "SIZE", "170 52" } }), screen, scale),
		1658, 984, 1898, 1058);
	expectRect(UiAnchor::resolve(parse({ { "ANCHOR", "RIGHT TOP" }, { "OFFSET", "0 0" }, { "SIZE", "100 100" } }), screen, scale),
		1779, 0, 1920, 141);
}

TEST(UiAnchor, CenteredPanelKeepsItsDesignProportions)
{
	const Spec panel = parse({ { "ANCHOR", "CENTER MIDDLE" }, { "SIZE", "880 477" } });
	expectRect(UiAnchor::resolve(panel, UiAnchor::screenArea(1024, 768), 1.0), 72, 146, 952, 623);
	expectRect(UiAnchor::resolve(panel, UiAnchor::screenArea(1920, 1080), UiScale::scale(1920, 1080)), 341, 205, 1579, 875);
	expectRect(UiAnchor::resolve(panel, UiAnchor::screenArea(3840, 2160), UiScale::scale(3840, 2160)), 683, 409, 3158, 1751);

	const Spec moved = parse({ { "ANCHOR", "CENTER MIDDLE" }, { "OFFSET", "-100 20" }, { "SIZE", "200 100" } });
	expectRect(UiAnchor::resolve(moved, UiAnchor::screenArea(1024, 768), 1.0), 312, 354, 512, 454);
}

TEST(UiAnchor, StretchUsesMarginsAndIgnoresSize)
{
	const Spec bar = parse({ { "ANCHOR", "STRETCH BOTTOM" }, { "MARGIN", "8 0 8 0" }, { "OFFSET", "0 4" }, { "SIZE", "999 40" } });
	expectRect(UiAnchor::resolve(bar, UiAnchor::screenArea(2560, 1080), UiScale::scale(2560, 1080)), 11, 1018, 2549, 1074);
	const Spec fill = parse({ { "ANCHOR", "STRETCH STRETCH" } });
	expectRect(UiAnchor::resolve(fill, { 10, 20, 110, 220 }, 3.0), 10, 20, 110, 220);
}

TEST(UiAnchor, MinAndMaxSizeClampAndCenterOnStretchedAxes)
{
	const Spec column = parse({ { "ANCHOR", "STRETCH STRETCH" }, { "MARGIN", "0 0 0 0" }, { "MAXSIZE", "600 0" }, { "MINSIZE", "0 1000" } });
	const PixelRect rect = UiAnchor::resolve(column, UiAnchor::screenArea(1920, 1080), UiScale::scale(1920, 1080));
	expectRect(rect, 538, -163, 1382, 1243);
	EXPECT_EQ(rect.x1 - rect.x0, UiScale::toPixels(600, UiScale::scale(1920, 1080)));

	const Spec small = parse({ { "ANCHOR", "LEFT TOP" }, { "SIZE", "10 400" }, { "MINSIZE", "40 0" }, { "MAXSIZE", "0 300" } });
	expectRect(UiAnchor::resolve(small, UiAnchor::screenArea(1024, 768), 1.0), 0, 0, 40, 300);

	const Spec within = parse({ { "ANCHOR", "STRETCH TOP" }, { "SIZE", "0 10" }, { "MAXSIZE", "2000 0" } });
	expectRect(UiAnchor::resolve(within, UiAnchor::screenArea(1024, 768), 1.0), 0, 0, 1024, 10);
}

TEST(UiAnchor, MarginsWiderThanTheParentCollapseToTheMiddle)
{
	const Spec spec = parse({ { "ANCHOR", "STRETCH STRETCH" }, { "MARGIN", "60 0 60 0" } });
	expectRect(UiAnchor::resolve(spec, { 0, 0, 100, 50 }, 1.0), 50, 0, 50, 50);
}

TEST(UiAnchor, NeighboursShareEdgesAtEveryScale)
{
	const int resolutions[][2] = { { 1024, 768 }, { 1920, 1080 }, { 2560, 1080 }, { 1920, 1200 }, { 3840, 2160 }, { 1366, 768 } };
	for (const auto& resolution : resolutions)
	{
		const double scale = UiScale::scale(resolution[0], resolution[1]);
		const PixelRect parent = UiAnchor::safeArea(resolution[0], resolution[1], UiAnchor::SafeAreaSettings());
		int previous = parent.x0;
		for (int index = 0; index < 7; ++index)
		{
			Spec spec;
			std::string error;
			ASSERT_TRUE(UiAnchor::applyField(spec, Keyword::Anchor, "LEFT TOP", error));
			ASSERT_TRUE(UiAnchor::applyField(spec, Keyword::Offset, std::to_string(index * 37) + " 0", error));
			ASSERT_TRUE(UiAnchor::applyField(spec, Keyword::Size, "37 10", error));
			const PixelRect rect = UiAnchor::resolve(spec, parent, scale);
			EXPECT_EQ(rect.x0, previous);
			previous = rect.x1;
		}

		const PixelRect left = UiAnchor::resolve(parse({ { "ANCHOR", "CENTER TOP" }, { "OFFSET", "-50 0" }, { "SIZE", "101 10" } }), parent, scale);
		const PixelRect right = UiAnchor::resolve(parse({ { "ANCHOR", "CENTER TOP" }, { "OFFSET", "51 0" }, { "SIZE", "101 10" } }), parent, scale);
		EXPECT_EQ(left.x1, right.x0);
	}
}

TEST(UiAnchor, ChildrenResolveInsideTheirParent)
{
	const double scale = UiScale::scale(2560, 1080);
	const PixelRect safe = UiAnchor::safeArea(2560, 1080, UiAnchor::SafeAreaSettings());
	const PixelRect panel = UiAnchor::resolve(parse({ { "ANCHOR", "CENTER MIDDLE" }, { "SIZE", "600 400" } }), safe, scale);
	expectRect(panel, 858, 259, 1702, 821);
	const PixelRect close = UiAnchor::resolve(parse({ { "ANCHOR", "RIGHT TOP" }, { "OFFSET", "8 8" }, { "SIZE", "24 24" } }), panel, scale);
	expectRect(close, 1657, 270, 1691, 304);
	const PixelRect body = UiAnchor::resolve(parse({ { "ANCHOR", "STRETCH STRETCH" }, { "MARGIN", "16 40 16 16" } }), panel, scale);
	expectRect(body, 881, 315, 1680, 799);
}
