#pragma once

#include "GameClient/UiScale.h"

#include <string>
#include <string_view>

namespace UiAnchor
{
	enum class Align
	{
		Low,
		Center,
		High,
		Stretch
	};

	enum class Keyword
	{
		None,
		Anchor,
		Offset,
		Size,
		Margin,
		MinSize,
		MaxSize,
		SafeArea,
		TabOrder
	};

	struct Spec
	{
		unsigned seen = 0;
		Align horizontal = Align::Low;
		Align vertical = Align::Low;
		int offset[2] = { 0, 0 };
		int size[2] = { 0, 0 };
		int margin[4] = { 0, 0, 0, 0 };
		int minSize[2] = { 0, 0 };
		int maxSize[2] = { 0, 0 };
		bool safeArea = false;
		int tabOrder = -1;

		bool has(Keyword keyword) const { return (seen & (1u << static_cast<unsigned>(keyword))) != 0; }
		bool anchored() const { return has(Keyword::Anchor); }
	};

	struct SafeAreaSettings
	{
		int aspectWidth = 16;
		int aspectHeight = 9;
		int margin = 0;
	};

	Keyword keywordOf(std::string_view name);
	bool applyField(Spec& spec, Keyword keyword, std::string_view value, std::string& error);
	bool validate(const Spec& spec, bool hasParent, std::string& error);

	UiScale::PixelRect screenArea(int width, int height);
	UiScale::PixelRect safeArea(int width, int height, const SafeAreaSettings& settings);
	UiScale::PixelRect resolve(const Spec& spec, const UiScale::PixelRect& parent, double scale);

	SafeAreaSettings& activeSafeArea();
}
