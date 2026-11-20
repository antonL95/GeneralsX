#pragma once

#include "GameClient/UiAnchor.h"

#include <string>
#include <vector>

namespace UiRelayout
{
	enum class Rule
	{
		Fixed,
		Stretched,
		Anchored
	};

	struct WindowRule
	{
		Rule rule = Rule::Fixed;
		UiAnchor::Spec anchor;
		int region[4] = { 0, 0, 0, 0 };
		int resolution[2] = { 0, 0 };
		std::string fontName;
		int fontSize = 0;
		bool fontBold = false;

		bool hasFont() const { return !fontName.empty() && fontSize > 0; }
		bool movable() const;
	};

	struct Node
	{
		WindowRule rule;
		int parent = -1;
		UiScale::PixelRect current = { 0, 0, 0, 0 };
	};

	UiScale::PixelRect stretchedRect(const int region[4], const int resolution[2], int width, int height);
	UiScale::PixelRect anchorArea(const UiAnchor::Spec& spec, const UiScale::PixelRect* parent, int width, int height,
		const UiAnchor::SafeAreaSettings& safeArea);
	UiScale::PixelRect anchoredRect(const UiAnchor::Spec& spec, const UiScale::PixelRect* parent, int width, int height,
		const UiAnchor::SafeAreaSettings& safeArea);
	UiScale::PixelRect place(const WindowRule& rule, const UiScale::PixelRect& current, const UiScale::PixelRect* parentBefore,
		const UiScale::PixelRect* parentAfter, int width, int height, const UiAnchor::SafeAreaSettings& safeArea);
	std::vector<UiScale::PixelRect> resolve(const std::vector<Node>& nodes, int width, int height,
		const UiAnchor::SafeAreaSettings& safeArea);
}
