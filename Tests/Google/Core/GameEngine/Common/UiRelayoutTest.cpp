#include "GameClient/UiRelayout.h"
#include "GameClient/UiScale.h"

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

namespace
{
	using UiRelayout::Node;
	using UiRelayout::Rule;
	using UiRelayout::WindowRule;
	using UiScale::PixelRect;

	struct Size
	{
		int width;
		int height;
	};

	const Size Resolutions[] = {
		{ 1024, 768 },
		{ 1366, 768 },
		{ 1920, 1080 },
		{ 1920, 1200 },
		{ 2560, 1080 },
		{ 2560, 1440 },
		{ 3440, 1440 },
		{ 3840, 2160 },
		{ 800, 600 },
	};

	WindowRule anchored(std::initializer_list<std::pair<const char*, const char*>> fields)
	{
		WindowRule rule;
		rule.rule = Rule::Anchored;
		for (const auto& field : fields)
		{
			std::string error;
			EXPECT_TRUE(UiAnchor::applyField(rule.anchor, UiAnchor::keywordOf(field.first), field.second, error))
				<< field.first << " = " << field.second << ": " << error;
		}
		return rule;
	}

	WindowRule stretched(int x0, int y0, int x1, int y1, int creationWidth, int creationHeight)
	{
		WindowRule rule;
		rule.rule = Rule::Stretched;
		rule.region[0] = x0;
		rule.region[1] = y0;
		rule.region[2] = x1;
		rule.region[3] = y1;
		rule.resolution[0] = creationWidth;
		rule.resolution[1] = creationHeight;
		return rule;
	}

	struct Window
	{
		WindowRule rule;
		int parent;
		PixelRect fixedOffset;
	};

	std::vector<Window> sampleTree()
	{
		std::vector<Window> tree;
		tree.push_back({ anchored({ { "ANCHOR", "STRETCH STRETCH" }, { "SAFEAREA", "YES" } }), -1, {} });
		tree.push_back({ anchored({ { "ANCHOR", "CENTER MIDDLE" }, { "SIZE", "560 400" } }), 0, {} });
		tree.push_back({ anchored({ { "ANCHOR", "STRETCH TOP" }, { "OFFSET", "0 40" }, { "SIZE", "0 32" },
			{ "MARGIN", "16 0 16 0" } }), 1, {} });
		tree.push_back({ anchored({ { "ANCHOR", "STRETCH TOP" }, { "OFFSET", "0 72" }, { "SIZE", "0 32" },
			{ "MARGIN", "16 0 16 0" } }), 1, {} });
		tree.push_back({ anchored({ { "ANCHOR", "RIGHT BOTTOM" }, { "OFFSET", "16 16" }, { "SIZE", "170 52" } }), 0, {} });
		tree.push_back({ anchored({ { "ANCHOR", "STRETCH BOTTOM" }, { "OFFSET", "0 8" }, { "SIZE", "0 48" },
			{ "MARGIN", "24 0 24 0" }, { "MAXSIZE", "1200 0" } }), -1, {} });
		tree.push_back({ WindowRule(), 4, { 4, 6, 24, 30 } });
		tree.push_back({ anchored({ { "ANCHOR", "LEFT TOP" }, { "OFFSET", "2 2" }, { "SIZE", "10 10" } }), 6, {} });
		tree.push_back({ stretched(40, 500, 240, 560, 800, 600), -1, {} });
		tree.push_back({ stretched(50, 510, 230, 550, 800, 600), 8, {} });
		tree.push_back({ anchored({ { "ANCHOR", "RIGHT TOP" }, { "SIZE", "48 48" } }), -1, {} });
		return tree;
	}

	std::vector<PixelRect> freshLayout(const std::vector<Window>& tree, int width, int height)
	{
		std::vector<PixelRect> rects;
		for (const Window& window : tree)
		{
			const PixelRect* parent = window.parent >= 0 ? &rects[static_cast<std::size_t>(window.parent)] : nullptr;
			if (window.rule.rule == Rule::Anchored)
				rects.push_back(UiRelayout::anchoredRect(window.rule.anchor, parent, width, height, UiAnchor::SafeAreaSettings()));
			else if (window.rule.rule == Rule::Stretched)
				rects.push_back(UiRelayout::stretchedRect(window.rule.region, window.rule.resolution, width, height));
			else
				rects.push_back({ parent->x0 + window.fixedOffset.x0, parent->y0 + window.fixedOffset.y0,
					parent->x0 + window.fixedOffset.x1, parent->y0 + window.fixedOffset.y1 });
		}
		return rects;
	}

	std::vector<Node> nodesAt(const std::vector<Window>& tree, const std::vector<PixelRect>& rects)
	{
		std::vector<Node> nodes;
		for (std::size_t index = 0; index < tree.size(); ++index)
			nodes.push_back({ tree[index].rule, tree[index].parent, rects[index] });
		return nodes;
	}

	void expectSame(const std::vector<PixelRect>& actual, const std::vector<PixelRect>& expected, const char* context)
	{
		ASSERT_EQ(actual.size(), expected.size());
		for (std::size_t index = 0; index < actual.size(); ++index)
		{
			EXPECT_EQ(actual[index].x0, expected[index].x0) << context << " window " << index;
			EXPECT_EQ(actual[index].y0, expected[index].y0) << context << " window " << index;
			EXPECT_EQ(actual[index].x1, expected[index].x1) << context << " window " << index;
			EXPECT_EQ(actual[index].y1, expected[index].y1) << context << " window " << index;
		}
	}
}

TEST(UiRelayout, RelayoutEqualsAFreshLayoutBetweenEveryPairOfResolutions)
{
	const std::vector<Window> tree = sampleTree();
	for (const Size& from : Resolutions)
	{
		const std::vector<PixelRect> before = freshLayout(tree, from.width, from.height);
		for (const Size& to : Resolutions)
		{
			const std::vector<PixelRect> relaid = UiRelayout::resolve(nodesAt(tree, before), to.width, to.height,
				UiAnchor::SafeAreaSettings());
			const std::string context = std::to_string(from.width) + "x" + std::to_string(from.height) + " to " +
				std::to_string(to.width) + "x" + std::to_string(to.height);
			expectSame(relaid, freshLayout(tree, to.width, to.height), context.c_str());
		}
	}
}

TEST(UiRelayout, ChainedRelayoutsReturnToTheStartingLayout)
{
	const std::vector<Window> tree = sampleTree();
	const std::vector<PixelRect> start = freshLayout(tree, 1024, 768);
	std::vector<PixelRect> current = start;
	for (const Size& size : Resolutions)
		current = UiRelayout::resolve(nodesAt(tree, current), size.width, size.height, UiAnchor::SafeAreaSettings());
	current = UiRelayout::resolve(nodesAt(tree, current), 1024, 768, UiAnchor::SafeAreaSettings());
	expectSame(current, start, "round trip");
}

TEST(UiRelayout, StretchedWindowsUseTheLegacyFloatFormula)
{
	const int region[4] = { 37, 101, 799, 599 };
	const int resolution[2] = { 800, 600 };
	for (const Size& size : Resolutions)
	{
		const float xScale = static_cast<float>(size.width) / 800.0f;
		const float yScale = static_cast<float>(size.height) / 600.0f;
		const PixelRect rect = UiRelayout::stretchedRect(region, resolution, size.width, size.height);
		EXPECT_EQ(rect.x0, static_cast<int>(37.0f * xScale));
		EXPECT_EQ(rect.y0, static_cast<int>(101.0f * yScale));
		EXPECT_EQ(rect.x1, static_cast<int>(799.0f * xScale));
		EXPECT_EQ(rect.y1, static_cast<int>(599.0f * yScale));
	}
}

TEST(UiRelayout, StretchedWindowsIgnoreTheirParent)
{
	const WindowRule rule = stretched(100, 100, 200, 150, 1024, 768);
	const PixelRect parentBefore = { 0, 0, 500, 500 };
	const PixelRect parentAfter = { 300, 300, 900, 900 };
	const PixelRect rect = UiRelayout::place(rule, { 0, 0, 0, 0 }, &parentBefore, &parentAfter, 2048, 1536,
		UiAnchor::SafeAreaSettings());
	EXPECT_EQ(rect.x0, 200);
	EXPECT_EQ(rect.y0, 200);
	EXPECT_EQ(rect.x1, 400);
	EXPECT_EQ(rect.y1, 300);
}

TEST(UiRelayout, FixedWindowsKeepTheirOffsetFromTheParent)
{
	const PixelRect parentBefore = { 10, 20, 110, 220 };
	const PixelRect parentAfter = { 50, 70, 300, 400 };
	const PixelRect rect = UiRelayout::place(WindowRule(), { 15, 30, 45, 60 }, &parentBefore, &parentAfter, 1920, 1080,
		UiAnchor::SafeAreaSettings());
	EXPECT_EQ(rect.x0, 55);
	EXPECT_EQ(rect.y0, 80);
	EXPECT_EQ(rect.x1, 85);
	EXPECT_EQ(rect.y1, 110);
}

TEST(UiRelayout, FixedRootsStayWhereTheyAre)
{
	const PixelRect rect = UiRelayout::place(WindowRule(), { 15, 30, 45, 60 }, nullptr, nullptr, 3840, 2160,
		UiAnchor::SafeAreaSettings());
	EXPECT_EQ(rect.x0, 15);
	EXPECT_EQ(rect.y0, 30);
	EXPECT_EQ(rect.x1, 45);
	EXPECT_EQ(rect.y1, 60);
}

TEST(UiRelayout, SafeAreaRootsFollowTheNewSafeArea)
{
	const WindowRule rule = anchored({ { "ANCHOR", "STRETCH STRETCH" }, { "SAFEAREA", "YES" } });
	const PixelRect wide = UiRelayout::place(rule, { 0, 0, 1024, 768 }, nullptr, nullptr, 2560, 1080,
		UiAnchor::SafeAreaSettings());
	EXPECT_EQ(wide.x0, 320);
	EXPECT_EQ(wide.x1, 2240);
	EXPECT_EQ(wide.y0, 0);
	EXPECT_EQ(wide.y1, 1080);

	const PixelRect plain = UiRelayout::place(rule, wide, nullptr, nullptr, 1920, 1080, UiAnchor::SafeAreaSettings());
	EXPECT_EQ(plain.x0, 0);
	EXPECT_EQ(plain.x1, 1920);
}

TEST(UiRelayout, OnlyAnchoredAndLegacyWindowsMove)
{
	EXPECT_FALSE(WindowRule().movable());
	WindowRule withoutResolution;
	withoutResolution.rule = Rule::Stretched;
	EXPECT_FALSE(withoutResolution.movable());
	EXPECT_TRUE(stretched(0, 0, 10, 10, 800, 600).movable());
	EXPECT_TRUE(anchored({ { "ANCHOR", "LEFT TOP" }, { "SIZE", "10 10" } }).movable());
}

TEST(UiRelayout, StretchedRuleWithoutCreationResolutionActsFixed)
{
	WindowRule rule;
	rule.rule = Rule::Stretched;
	const PixelRect parentBefore = { 0, 0, 100, 100 };
	const PixelRect parentAfter = { 10, 10, 210, 210 };
	const PixelRect rect = UiRelayout::place(rule, { 5, 5, 15, 15 }, &parentBefore, &parentAfter, 2560, 1440,
		UiAnchor::SafeAreaSettings());
	EXPECT_EQ(rect.x0, 15);
	EXPECT_EQ(rect.y0, 15);
	EXPECT_EQ(rect.x1, 25);
	EXPECT_EQ(rect.y1, 25);
}

TEST(UiRelayout, FontRequestIsKeptWithTheRule)
{
	WindowRule rule = anchored({ { "ANCHOR", "LEFT TOP" }, { "SIZE", "10 10" } });
	EXPECT_FALSE(rule.hasFont());
	rule.fontName = "Albertus MT";
	rule.fontSize = 16;
	EXPECT_TRUE(rule.hasFont());
	rule.fontSize = 0;
	EXPECT_FALSE(rule.hasFont());
}
