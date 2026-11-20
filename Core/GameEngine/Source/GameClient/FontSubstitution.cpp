#include "GameClient/FontSubstitution.h"

#include <algorithm>
#include <cctype>
#include <charconv>

namespace
{
	char foldChar(char ch)
	{
		return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
	}

	bool isSpace(char ch)
	{
		return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
	}

	std::string_view trim(std::string_view text)
	{
		while (!text.empty() && isSpace(text.front()))
			text.remove_prefix(1);
		while (!text.empty() && isSpace(text.back()))
			text.remove_suffix(1);
		return text;
	}

	std::string_view stripComment(std::string_view line)
	{
		bool quoted = false;
		for (std::size_t i = 0; i < line.size(); ++i)
		{
			if (line[i] == '"')
				quoted = !quoted;
			else if (!quoted && line[i] == ';')
				return line.substr(0, i);
			else if (!quoted && line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/')
				return line.substr(0, i);
		}
		return line;
	}

	std::string_view nextToken(std::string_view& text)
	{
		text = trim(text);
		std::size_t end = 0;
		while (end < text.size() && !isSpace(text[end]) && text[end] != '=')
			++end;
		std::string_view token = text.substr(0, end);
		text.remove_prefix(end);
		return token;
	}

	bool skipEquals(std::string_view& text)
	{
		text = trim(text);
		if (text.empty() || text.front() != '=')
			return false;
		text.remove_prefix(1);
		return true;
	}

	bool parseInt(std::string_view token, int& value)
	{
		if (token.empty())
			return false;
		const char* begin = token.data();
		const char* end = token.data() + token.size();
		if (*begin == '+')
			++begin;
		const auto result = std::from_chars(begin, end, value);
		return result.ec == std::errc() && result.ptr == end;
	}

	std::string unquote(std::string_view text)
	{
		text = trim(text);
		if (text.size() >= 2 && text.front() == '"')
		{
			const std::size_t close = text.find('"', 1);
			if (close != std::string_view::npos)
				return std::string(trim(text.substr(1, close - 1)));
		}
		return std::string(text);
	}

	bool keywordIs(std::string_view token, std::string_view keyword)
	{
		return fontNamesEqualNoCase(token, keyword);
	}

	void warn(std::vector<std::string>* warnings, std::size_t lineNumber, std::string_view message)
	{
		if (warnings)
			warnings->push_back("line " + std::to_string(lineNumber) + ": " + std::string(message));
	}
}

bool fontNamesEqualNoCase(std::string_view lhs, std::string_view rhs)
{
	return lhs.size() == rhs.size() &&
		std::equal(lhs.begin(), lhs.end(), rhs.begin(), [](char a, char b) { return foldChar(a) == foldChar(b); });
}

FontSubstitutionTable FontSubstitutionTable::parse(std::string_view text, std::vector<std::string>* warnings)
{
	FontSubstitutionTable table;
	std::string currentFont;
	bool inBlock = false;
	std::size_t lineNumber = 0;

	while (!text.empty())
	{
		const std::size_t newline = text.find('\n');
		std::string_view line = text.substr(0, newline);
		text.remove_prefix(newline == std::string_view::npos ? text.size() : newline + 1);
		++lineNumber;

		std::string_view rest = trim(stripComment(line));
		if (rest.empty())
			continue;

		const std::string_view keyword = nextToken(rest);
		if (!inBlock)
		{
			if (!keywordIs(keyword, "FontSubstitution"))
			{
				warn(warnings, lineNumber, "expected FontSubstitution");
				continue;
			}
			currentFont = unquote(rest);
			if (currentFont.empty())
			{
				warn(warnings, lineNumber, "FontSubstitution without a font name");
				continue;
			}
			inBlock = true;
			continue;
		}

		if (keywordIs(keyword, "End"))
		{
			inBlock = false;
			continue;
		}

		if (!keywordIs(keyword, "Size"))
		{
			warn(warnings, lineNumber, "unknown field");
			continue;
		}

		FontSubstitutionRule rule;
		if (!parseInt(nextToken(rest), rule.size) || !skipEquals(rest) ||
			!parseInt(nextToken(rest), rule.targetSize))
		{
			warn(warnings, lineNumber, "malformed Size line");
			continue;
		}

		std::string_view probe = trim(rest);
		std::string_view modifier = nextToken(probe);
		if (keywordIs(modifier, "+BOLD"))
		{
			rule.boldChange = 1;
			rest = probe;
		}
		else if (keywordIs(modifier, "-BOLD"))
		{
			rule.boldChange = -1;
			rest = probe;
		}

		rule.targetName = unquote(rest);
		if (rule.targetName.empty() || rule.size <= 0 || rule.targetSize <= 0)
		{
			warn(warnings, lineNumber, "malformed Size line");
			continue;
		}
		table.addRule(currentFont, rule);
	}

	if (inBlock)
		warn(warnings, lineNumber, "FontSubstitution without End");

	return table;
}

void FontSubstitutionTable::addRule(std::string_view fontName, const FontSubstitutionRule& rule)
{
	auto entry = std::find_if(m_entries.begin(), m_entries.end(),
		[&](const Entry& candidate) { return fontNamesEqualNoCase(candidate.fontName, fontName); });
	if (entry == m_entries.end())
	{
		m_entries.push_back(Entry{std::string(fontName), {}});
		entry = std::prev(m_entries.end());
	}

	auto& rules = entry->rules;
	const auto position = std::upper_bound(rules.begin(), rules.end(), rule.size,
		[](int size, const FontSubstitutionRule& candidate) { return size < candidate.size; });
	if (position != rules.begin() && std::prev(position)->size == rule.size)
		*std::prev(position) = rule;
	else
		rules.insert(position, rule);
}

std::size_t FontSubstitutionTable::ruleCount() const
{
	std::size_t count = 0;
	for (const Entry& entry : m_entries)
		count += entry.rules.size();
	return count;
}

std::vector<std::string> FontSubstitutionTable::targetNames() const
{
	std::vector<std::string> names;
	for (const Entry& entry : m_entries)
		for (const FontSubstitutionRule& rule : entry.rules)
			names.push_back(rule.targetName);
	return names;
}

bool FontSubstitutionTable::resolve(const FontRequest& request, FontRequest& result) const
{
	const auto entry = std::find_if(m_entries.begin(), m_entries.end(),
		[&](const Entry& candidate) { return fontNamesEqualNoCase(candidate.fontName, request.name); });
	if (entry == m_entries.end() || entry->rules.empty())
		return false;

	const auto& rules = entry->rules;
	const FontSubstitutionRule* chosen = &rules.front();
	int size = rules.front().targetSize;

	if (request.size >= rules.back().size)
	{
		chosen = &rules.back();
		size = chosen->targetSize;
	}
	else if (request.size > rules.front().size)
	{
		const auto upper = std::upper_bound(rules.begin(), rules.end(), request.size,
			[](int value, const FontSubstitutionRule& candidate) { return value < candidate.size; });
		const FontSubstitutionRule& high = *upper;
		const FontSubstitutionRule& low = *std::prev(upper);
		chosen = &low;
		size = low.targetSize +
			(request.size - low.size) * (high.targetSize - low.targetSize) / (high.size - low.size);
	}

	result.name = chosen->targetName;
	result.size = std::max(1, size);
	result.bold = chosen->boldChange > 0 ? true : (chosen->boldChange < 0 ? false : request.bold);
	return true;
}

std::vector<std::string> parseLocalFontFiles(std::string_view languageIni)
{
	std::vector<std::string> files;
	while (!languageIni.empty())
	{
		const std::size_t newline = languageIni.find('\n');
		std::string_view line = languageIni.substr(0, newline);
		languageIni.remove_prefix(newline == std::string_view::npos ? languageIni.size() : newline + 1);

		std::string_view rest = trim(stripComment(line));
		if (!keywordIs(nextToken(rest), "LocalFontFile"))
			continue;
		skipEquals(rest);
		const std::string file = unquote(rest);
		if (file.empty())
			continue;
		const bool known = std::any_of(files.begin(), files.end(),
			[&](const std::string& existing) { return fontNamesEqualNoCase(existing, file); });
		if (!known)
			files.push_back(file);
	}
	return files;
}
