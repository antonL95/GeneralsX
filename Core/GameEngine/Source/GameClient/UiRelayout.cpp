#include "GameClient/UiRelayout.h"

namespace UiRelayout
{
	namespace
	{
		int stretch(int value, float factor)
		{
			return static_cast<int>(static_cast<float>(value) * factor);
		}
	}

	bool WindowRule::movable() const
	{
		if (rule == Rule::Anchored)
			return true;
		return rule == Rule::Stretched && resolution[0] > 0 && resolution[1] > 0;
	}

	UiScale::PixelRect stretchedRect(const int region[4], const int resolution[2], int width, int height)
	{
		const float xScale = static_cast<float>(width) / static_cast<float>(resolution[0]);
		const float yScale = static_cast<float>(height) / static_cast<float>(resolution[1]);
		return { stretch(region[0], xScale), stretch(region[1], yScale), stretch(region[2], xScale), stretch(region[3], yScale) };
	}

	UiScale::PixelRect anchorArea(const UiAnchor::Spec& spec, const UiScale::PixelRect* parent, int width, int height,
		const UiAnchor::SafeAreaSettings& safeArea)
	{
		if (parent)
			return *parent;
		if (spec.safeArea)
			return UiAnchor::safeArea(width, height, safeArea);
		return UiAnchor::screenArea(width, height);
	}

	UiScale::PixelRect anchoredRect(const UiAnchor::Spec& spec, const UiScale::PixelRect* parent, int width, int height,
		const UiAnchor::SafeAreaSettings& safeArea)
	{
		return UiAnchor::resolve(spec, anchorArea(spec, parent, width, height, safeArea), UiScale::scale(width, height));
	}

	UiScale::PixelRect place(const WindowRule& rule, const UiScale::PixelRect& current, const UiScale::PixelRect* parentBefore,
		const UiScale::PixelRect* parentAfter, int width, int height, const UiAnchor::SafeAreaSettings& safeArea)
	{
		if (rule.rule == Rule::Anchored)
			return anchoredRect(rule.anchor, parentAfter, width, height, safeArea);
		if (rule.movable())
			return stretchedRect(rule.region, rule.resolution, width, height);
		if (parentBefore == nullptr || parentAfter == nullptr)
			return current;
		const int dx = parentAfter->x0 - parentBefore->x0;
		const int dy = parentAfter->y0 - parentBefore->y0;
		return { current.x0 + dx, current.y0 + dy, current.x1 + dx, current.y1 + dy };
	}

	std::vector<UiScale::PixelRect> resolve(const std::vector<Node>& nodes, int width, int height,
		const UiAnchor::SafeAreaSettings& safeArea)
	{
		std::vector<UiScale::PixelRect> result;
		result.reserve(nodes.size());
		for (const Node& node : nodes)
		{
			const bool hasParent = node.parent >= 0 && static_cast<std::size_t>(node.parent) < result.size();
			const UiScale::PixelRect* before = hasParent ? &nodes[static_cast<std::size_t>(node.parent)].current : nullptr;
			const UiScale::PixelRect* after = hasParent ? &result[static_cast<std::size_t>(node.parent)] : nullptr;
			result.push_back(place(node.rule, node.current, before, after, width, height, safeArea));
		}
		return result;
	}
}
