#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

struct FontSubstitutionRule
{
	int size = 0;
	int targetSize = 0;
	std::string targetName;
	int boldChange = 0;
};

struct FontRequest
{
	std::string name;
	int size = 0;
	bool bold = false;
};

class FontSubstitutionTable
{
public:
	static FontSubstitutionTable parse(std::string_view text, std::vector<std::string>* warnings = nullptr);

	void addRule(std::string_view fontName, const FontSubstitutionRule& rule);
	bool resolve(const FontRequest& request, FontRequest& result) const;
	bool empty() const { return m_entries.empty(); }
	std::size_t fontCount() const { return m_entries.size(); }
	std::size_t ruleCount() const;
	std::vector<std::string> targetNames() const;

private:
	struct Entry
	{
		std::string fontName;
		std::vector<FontSubstitutionRule> rules;
	};

	std::vector<Entry> m_entries;
};

std::vector<std::string> parseLocalFontFiles(std::string_view languageIni);
bool fontNamesEqualNoCase(std::string_view lhs, std::string_view rhs);
