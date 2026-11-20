#include "GameClient/UiLayoutDump.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <tuple>

namespace
{
	std::string windowPath(const UiWindowRecord& record)
	{
		return record.parentPath.empty() ? record.name : record.parentPath + "/" + record.name;
	}

	auto sortKey(const UiWindowRecord& record)
	{
		return std::make_tuple(record.layoutFile, windowPath(record), record.screen[1], record.screen[0],
			record.screen[3], record.screen[2], record.type, record.fontName, record.fontSize);
	}

	void appendInts(std::string& out, const int* values, int count)
	{
		out += '[';
		for (int i = 0; i < count; ++i)
		{
			if (i)
				out += ", ";
			out += std::to_string(values[i]);
		}
		out += ']';
	}

	void appendString(std::string& out, std::string_view text)
	{
		out += '"';
		out += uiJsonEscape(text);
		out += '"';
	}

	std::string_view trim(std::string_view text)
	{
		while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
			text.remove_prefix(1);
		while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
			text.remove_suffix(1);
		return text;
	}
}

std::string uiJsonEscape(std::string_view text)
{
	std::string out;
	out.reserve(text.size());
	for (const char ch : text)
	{
		const unsigned char byte = static_cast<unsigned char>(ch);
		switch (ch)
		{
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (byte < 0x20)
				{
					char buffer[8];
					std::snprintf(buffer, sizeof(buffer), "\\u%04x", byte);
					out += buffer;
				}
				else
				{
					out += ch;
				}
		}
	}
	return out;
}

std::string formatUiLayoutJson(const UiLayoutSummary& summary, std::vector<UiWindowRecord> windows)
{
	std::stable_sort(windows.begin(), windows.end(),
		[](const UiWindowRecord& lhs, const UiWindowRecord& rhs) { return sortKey(lhs) < sortKey(rhs); });

	char scale[32];
	std::snprintf(scale, sizeof(scale), "%.6f", summary.scale);

	std::string out = "{\n";
	out += "  \"resolution\": [" + std::to_string(summary.width) + ", " + std::to_string(summary.height) + "],\n";
	out += "  \"scale\": ";
	out += scale;
	out += ",\n  \"layout\": ";
	appendString(out, summary.layout);
	out += ",\n  \"windowCount\": " + std::to_string(windows.size());
	out += ",\n  \"windows\": [";

	for (std::size_t i = 0; i < windows.size(); ++i)
	{
		const UiWindowRecord& window = windows[i];
		out += i ? ",\n    {" : "\n    {";
		out += "\"name\": ";
		appendString(out, window.name);
		out += ", \"type\": ";
		appendString(out, window.type);
		out += ", \"parent\": ";
		appendString(out, window.parentPath);
		out += ", \"layoutFile\": ";
		appendString(out, window.layoutFile);
		out += ", \"screen\": ";
		appendInts(out, window.screen, 4);
		out += ", \"layoutRect\": ";
		if (window.hasLayoutRect)
		{
			appendInts(out, window.layoutRect, 4);
			out += ", \"layoutResolution\": ";
			appendInts(out, window.layoutResolution, 2);
		}
		else
		{
			out += "null, \"layoutResolution\": null";
		}
		out += ", \"hidden\": ";
		out += window.hidden ? "true" : "false";
		out += ", \"visible\": ";
		out += window.visible ? "true" : "false";
		out += ", \"font\": ";
		if (window.hasFont)
		{
			out += "{\"name\": ";
			appendString(out, window.fontName);
			out += ", \"size\": " + std::to_string(window.fontSize);
			out += ", \"unit\": ";
			out += window.fontPixels ? "\"px\"" : "\"pt\"";
			out += ", \"bold\": ";
			out += window.fontBold ? "true" : "false";
			out += ", \"height\": " + std::to_string(window.fontHeight) + "}";
		}
		else
		{
			out += "null";
		}
		out += '}';
	}

	out += windows.empty() ? "]\n}\n" : "\n  ]\n}\n";
	return out;
}

std::vector<UiSpecimenLine> parseUiSpecimen(std::string_view spec)
{
	std::vector<UiSpecimenLine> lines;
	while (!spec.empty())
	{
		const std::size_t bar = spec.find('|');
		std::string_view entry = spec.substr(0, bar);
		spec.remove_prefix(bar == std::string_view::npos ? spec.size() : bar + 1);

		const std::size_t first = entry.find(';');
		if (first == std::string_view::npos)
			continue;
		const std::size_t second = entry.find(';', first + 1);
		if (second == std::string_view::npos)
			continue;

		UiSpecimenLine line;
		line.font = std::string(trim(entry.substr(0, first)));
		const std::string_view size = trim(entry.substr(first + 1, second - first - 1));
		const auto parsed = std::from_chars(size.data(), size.data() + size.size(), line.designSize);
		line.text = std::string(trim(entry.substr(second + 1)));
		if (line.font.empty() || parsed.ec != std::errc() || parsed.ptr != size.data() + size.size() ||
			line.designSize <= 0 || line.text.empty())
			continue;
		lines.push_back(std::move(line));
	}
	return lines;
}
