#include "GameClient/UiAnchor.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <vector>

namespace UiAnchor
{
	namespace
	{
		constexpr int MaxDesignUnits = 100000;

		struct KeywordName
		{
			Keyword keyword;
			const char* name;
		};

		constexpr KeywordName KeywordNames[] = {
			{ Keyword::Anchor, "ANCHOR" },
			{ Keyword::Offset, "OFFSET" },
			{ Keyword::Size, "SIZE" },
			{ Keyword::Margin, "MARGIN" },
			{ Keyword::MinSize, "MINSIZE" },
			{ Keyword::MaxSize, "MAXSIZE" },
			{ Keyword::SafeArea, "SAFEAREA" },
			{ Keyword::TabOrder, "TABORDER" },
		};

		unsigned bit(Keyword keyword)
		{
			return 1u << static_cast<unsigned>(keyword);
		}

		const char* nameOf(Keyword keyword)
		{
			for (const KeywordName& entry : KeywordNames)
			{
				if (entry.keyword == keyword)
					return entry.name;
			}
			return "";
		}

		bool equalsNoCase(std::string_view lhs, std::string_view rhs)
		{
			return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), [](char a, char b) {
				const auto fold = [](char ch) { return ch >= 'a' && ch <= 'z' ? static_cast<char>(ch - 'a' + 'A') : ch; };
				return fold(a) == fold(b);
			});
		}

		std::vector<std::string_view> tokens(std::string_view value)
		{
			std::vector<std::string_view> result;
			std::size_t index = 0;
			while (index < value.size())
			{
				while (index < value.size() && (value[index] == ' ' || value[index] == ',' || value[index] == '\t'))
					++index;
				const std::size_t start = index;
				while (index < value.size() && value[index] != ' ' && value[index] != ',' && value[index] != '\t')
					++index;
				if (index > start)
					result.push_back(value.substr(start, index - start));
			}
			return result;
		}

		bool parseInts(std::string_view value, std::size_t count, int minimum, int* out, std::string& error)
		{
			const std::vector<std::string_view> parts = tokens(value);
			if (parts.size() != count)
			{
				error = "expects " + std::to_string(count) + " whole numbers";
				return false;
			}
			for (std::size_t index = 0; index < count; ++index)
			{
				const std::string_view part = parts[index];
				const char* begin = part.data();
				if (*begin == '+')
					++begin;
				int number = 0;
				const auto parsed = std::from_chars(begin, part.data() + part.size(), number);
				if (parsed.ec != std::errc() || parsed.ptr != part.data() + part.size())
				{
					error = "'" + std::string(part) + "' is not a whole number";
					return false;
				}
				if (number < minimum || number > MaxDesignUnits || number < -MaxDesignUnits)
				{
					error = "'" + std::string(part) + "' is out of range";
					return false;
				}
				out[index] = number;
			}
			return true;
		}

		bool parseAlign(std::string_view token, bool horizontal, Align& align)
		{
			if (equalsNoCase(token, "STRETCH"))
				align = Align::Stretch;
			else if (equalsNoCase(token, horizontal ? "LEFT" : "TOP"))
				align = Align::Low;
			else if (equalsNoCase(token, horizontal ? "CENTER" : "MIDDLE"))
				align = Align::Center;
			else if (equalsNoCase(token, horizontal ? "RIGHT" : "BOTTOM"))
				align = Align::High;
			else
				return false;
			return true;
		}

		int edge(double anchor, double design, double scale)
		{
			return static_cast<int>(std::floor(anchor + design * scale + 0.5));
		}

		struct AxisRule
		{
			Align align;
			int offset;
			int size;
			int marginLow;
			int marginHigh;
			int minSize;
			int maxSize;
		};

		double clampSize(double size, const AxisRule& rule, bool& clamped)
		{
			double result = std::max(0.0, size);
			if (rule.minSize > 0)
				result = std::max(result, static_cast<double>(rule.minSize));
			if (rule.maxSize > 0)
				result = std::min(result, static_cast<double>(rule.maxSize));
			clamped = result != size;
			return result;
		}

		void resolveAxis(const AxisRule& rule, int parent0, int parent1, double scale, int& out0, int& out1)
		{
			bool clamped = false;
			const double center = (static_cast<double>(parent0) + parent1) / 2.0;
			switch (rule.align)
			{
				case Align::Low:
				{
					const double size = clampSize(rule.size, rule, clamped);
					out0 = edge(parent0, rule.offset, scale);
					out1 = edge(parent0, rule.offset + size, scale);
					return;
				}
				case Align::High:
				{
					const double size = clampSize(rule.size, rule, clamped);
					out0 = edge(parent1, -(rule.offset + size), scale);
					out1 = edge(parent1, -static_cast<double>(rule.offset), scale);
					return;
				}
				case Align::Center:
				{
					const double size = clampSize(rule.size, rule, clamped);
					out0 = edge(center, rule.offset - size / 2.0, scale);
					out1 = edge(center, rule.offset + size / 2.0, scale);
					return;
				}
				case Align::Stretch:
				{
					const double available = (parent1 - parent0) / scale - rule.marginLow - rule.marginHigh;
					const double size = clampSize(available, rule, clamped);
					if (!clamped)
					{
						out0 = edge(parent0, rule.marginLow, scale);
						out1 = edge(parent1, -static_cast<double>(rule.marginHigh), scale);
						return;
					}
					const double middle = (rule.marginLow - rule.marginHigh) / 2.0;
					out0 = edge(center, middle - size / 2.0, scale);
					out1 = edge(center, middle + size / 2.0, scale);
					return;
				}
			}
		}
	}

	Keyword keywordOf(std::string_view name)
	{
		for (const KeywordName& entry : KeywordNames)
		{
			if (name == entry.name)
				return entry.keyword;
		}
		return Keyword::None;
	}

	bool applyField(Spec& spec, Keyword keyword, std::string_view value, std::string& error)
	{
		if (keyword == Keyword::None)
		{
			error = "unknown keyword";
			return false;
		}
		if (spec.has(keyword))
		{
			error = std::string(nameOf(keyword)) + " is given twice";
			return false;
		}

		bool parsed = false;
		std::string detail;
		switch (keyword)
		{
			case Keyword::Anchor:
			{
				const std::vector<std::string_view> parts = tokens(value);
				parsed = parts.size() == 2 && parseAlign(parts[0], true, spec.horizontal) && parseAlign(parts[1], false, spec.vertical);
				if (!parsed)
					detail = "expects LEFT, CENTER, RIGHT or STRETCH followed by TOP, MIDDLE, BOTTOM or STRETCH";
				break;
			}
			case Keyword::Offset:
				parsed = parseInts(value, 2, -MaxDesignUnits, spec.offset, detail);
				break;
			case Keyword::Size:
				parsed = parseInts(value, 2, 0, spec.size, detail);
				break;
			case Keyword::Margin:
				parsed = parseInts(value, 4, -MaxDesignUnits, spec.margin, detail);
				break;
			case Keyword::MinSize:
				parsed = parseInts(value, 2, 0, spec.minSize, detail);
				break;
			case Keyword::MaxSize:
				parsed = parseInts(value, 2, 0, spec.maxSize, detail);
				break;
			case Keyword::SafeArea:
			{
				const std::vector<std::string_view> parts = tokens(value);
				parsed = parts.size() == 1 && (equalsNoCase(parts[0], "YES") || equalsNoCase(parts[0], "NO"));
				if (parsed)
					spec.safeArea = equalsNoCase(parts[0], "YES");
				else
					detail = "expects YES or NO";
				break;
			}
			case Keyword::TabOrder:
				parsed = parseInts(value, 1, 0, &spec.tabOrder, detail);
				break;
			case Keyword::None:
				break;
		}

		if (!parsed)
		{
			error = std::string(nameOf(keyword)) + " " + detail;
			return false;
		}
		spec.seen |= bit(keyword);
		return true;
	}

	bool validate(const Spec& spec, bool hasParent, std::string& error)
	{
		const Keyword needAnchor[] = { Keyword::Offset, Keyword::Size, Keyword::Margin, Keyword::MinSize, Keyword::MaxSize, Keyword::SafeArea };
		if (!spec.anchored())
		{
			for (Keyword keyword : needAnchor)
			{
				if (spec.has(keyword))
				{
					error = std::string(nameOf(keyword)) + " needs ANCHOR";
					return false;
				}
			}
			return true;
		}
		if (spec.safeArea && hasParent)
		{
			error = "SAFEAREA applies to root windows only";
			return false;
		}
		if ((spec.horizontal != Align::Stretch || spec.vertical != Align::Stretch) && !spec.has(Keyword::Size))
		{
			error = "SIZE is required unless both axes STRETCH";
			return false;
		}
		for (int axis = 0; axis < 2; ++axis)
		{
			if (spec.minSize[axis] > 0 && spec.maxSize[axis] > 0 && spec.minSize[axis] > spec.maxSize[axis])
			{
				error = "MINSIZE is larger than MAXSIZE";
				return false;
			}
		}
		return true;
	}

	UiScale::PixelRect screenArea(int width, int height)
	{
		return { 0, 0, std::max(0, width), std::max(0, height) };
	}

	UiScale::PixelRect safeArea(int width, int height, const SafeAreaSettings& settings)
	{
		UiScale::PixelRect area = screenArea(width, height);
		if (settings.aspectWidth > 0 && settings.aspectHeight > 0)
		{
			const long long numerator = 2LL * area.y1 * settings.aspectWidth + settings.aspectHeight;
			const long long capped = numerator / (2LL * settings.aspectHeight);
			if (capped < area.x1)
			{
				area.x0 = static_cast<int>((area.x1 - capped) / 2);
				area.x1 = area.x0 + static_cast<int>(capped);
			}
		}
		if (settings.margin != 0)
		{
			const double scale = UiScale::scale(width, height);
			const int inset = UiScale::toPixels(settings.margin, scale);
			area.x0 += inset;
			area.y0 += inset;
			area.x1 = std::max(area.x0, area.x1 - inset);
			area.y1 = std::max(area.y0, area.y1 - inset);
		}
		return area;
	}

	UiScale::PixelRect resolve(const Spec& spec, const UiScale::PixelRect& parent, double scale)
	{
		if (!(scale > 0.0))
			scale = 1.0;
		const AxisRule horizontal = { spec.horizontal, spec.offset[0], spec.size[0], spec.margin[0], spec.margin[2],
			spec.minSize[0], spec.maxSize[0] };
		const AxisRule vertical = { spec.vertical, spec.offset[1], spec.size[1], spec.margin[1], spec.margin[3],
			spec.minSize[1], spec.maxSize[1] };
		UiScale::PixelRect rect = { 0, 0, 0, 0 };
		resolveAxis(horizontal, parent.x0, parent.x1, scale, rect.x0, rect.x1);
		resolveAxis(vertical, parent.y0, parent.y1, scale, rect.y0, rect.y1);
		return rect;
	}

	SafeAreaSettings& activeSafeArea()
	{
		static SafeAreaSettings settings;
		return settings;
	}
}
