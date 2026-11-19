#pragma once

#include <algorithm>
#include <string_view>

class AsciiString;

namespace RetailFileLayers
{
	bool enabled();
	void enable();
	void setBaseDirectory(const char* path);
	const AsciiString& baseDirectory();
	const AsciiString& languageDirectory();
	bool hasLooseOverride(const char* filename);

	inline bool archiveNameLess(std::string_view lhs, std::string_view rhs)
	{
		const auto fold = [](unsigned char ch) {
			return ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : int(ch);
		};
		const auto mismatch = std::mismatch(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
			[&](char a, char b) { return fold(a) == fold(b); });
		if (mismatch.first == lhs.end() || mismatch.second == rhs.end())
			return lhs.size() == rhs.size() ? lhs < rhs : lhs.size() < rhs.size();
		return fold(*mismatch.first) < fold(*mismatch.second);
	}
}
