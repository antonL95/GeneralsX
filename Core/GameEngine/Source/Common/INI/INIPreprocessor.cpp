#include "Common/INIPreprocessor.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace
{
const char* const SEPARATORS = " \t\r\n=";

bool isSeparator(char c)
{
	return c != '\0' && std::strchr(SEPARATORS, c) != nullptr;
}

bool isBlank(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string trim(const std::string& text)
{
	std::size_t begin = 0;
	std::size_t end = text.size();
	while (begin < end && isBlank(text[begin]))
		++begin;
	while (end > begin && isBlank(text[end - 1]))
		--end;
	return text.substr(begin, end - begin);
}

bool startsWithKeyword(const std::string& line, const char* keyword)
{
	const std::size_t length = std::strlen(keyword);
	if (line.compare(0, length, keyword) != 0)
		return false;
	return line.size() == length || line[length] == ' ' || line[length] == '\t' || line[length] == '"';
}

bool isDefineLine(const std::string& text)
{
	return startsWithKeyword(trim(text), "#define");
}

std::size_t labelLength(const std::string& token)
{
	const std::size_t colon = token.find(':');
	if (colon == std::string::npos || colon == 0 || colon + 1 == token.size())
		return 0;
	for (std::size_t i = 0; i < colon; ++i)
	{
		const char c = token[i];
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || (i > 0 && c >= '0' && c <= '9')))
			return 0;
	}
	return colon + 1;
}
}

INIPreprocessor::INIPreprocessor(FileReader reader)
	: m_reader(std::move(reader))
{
}

std::string INIPreprocessor::process(const std::string& path, const std::string& contents, bool expandLabeledDefines)
{
	std::vector<Line> lines;
	expandIncludes(path, contents, lines, 0);

	for (const Line& line : lines)
	{
		if (isDefineLine(line.text))
			registerDefine(line);
	}

	std::string out;
	out.reserve(contents.size());
	for (const Line& line : lines)
	{
		if (!isDefineLine(line.text))
		{
			std::vector<Piece> pieces;
			if (!substitute(split(line.text), pieces, 0, expandLabeledDefines))
				warn(line, "#define expansion nested too deeply");
			out += join(pieces);
		}
		out += '\n';
	}
	return out;
}

void INIPreprocessor::clearDefines()
{
	m_defines.clear();
}

std::size_t INIPreprocessor::getDefineCount() const
{
	return m_defines.size();
}

const std::vector<std::string>& INIPreprocessor::getWarnings() const
{
	return m_warnings;
}

void INIPreprocessor::clearWarnings()
{
	m_warnings.clear();
}

std::string INIPreprocessor::stripComment(const std::string& line)
{
	std::size_t cut = line.find(';');
	std::size_t slashes = line.find("//");
	while (slashes != std::string::npos && slashes > 0 && line[slashes - 1] == ':')
		slashes = line.find("//", slashes + 2);
	if (slashes != std::string::npos && (cut == std::string::npos || slashes < cut))
		cut = slashes;
	return cut == std::string::npos ? line : line.substr(0, cut);
}

std::string INIPreprocessor::resolveIncludePath(const std::string& includingFile, const std::string& includeName)
{
	const std::size_t lastSeparator = includingFile.find_last_of("\\/");
	const std::string directory = lastSeparator == std::string::npos ? std::string() : includingFile.substr(0, lastSeparator);
	const std::string combined = (directory.empty() ? includeName : directory + "\\" + includeName) + "\\";

	std::vector<std::string> parts;
	std::string current;
	for (char c : combined)
	{
		if (c != '\\' && c != '/')
		{
			current += c;
			continue;
		}
		if (current == "..")
		{
			if (!parts.empty())
				parts.pop_back();
		}
		else if (!current.empty() && current != ".")
		{
			parts.push_back(current);
		}
		current.clear();
	}

	std::string result;
	for (std::size_t i = 0; i < parts.size(); ++i)
	{
		if (i != 0)
			result += '\\';
		result += parts[i];
	}
	return result;
}

void INIPreprocessor::expandIncludes(const std::string& path, const std::string& contents, std::vector<Line>& lines, int depth)
{
	std::size_t position = 0;
	int number = 0;
	while (position < contents.size())
	{
		std::size_t end = contents.find('\n', position);
		if (end == std::string::npos)
			end = contents.size();
		++number;

		Line line;
		line.text = stripComment(contents.substr(position, end - position));
		line.path = path;
		line.number = number;
		position = end + 1;

		const std::string trimmed = trim(line.text);
		if (!startsWithKeyword(trimmed, "#include"))
		{
			lines.push_back(line);
			continue;
		}

		std::string name = trim(trimmed.substr(std::strlen("#include")));
		if (!name.empty() && name[0] == '"')
		{
			const std::size_t close = name.find('"', 1);
			name = close == std::string::npos ? name.substr(1) : name.substr(1, close - 1);
		}

		Line blank = line;
		blank.text.clear();
		lines.push_back(blank);

		if (name.empty())
		{
			warn(line, "#include without a file name");
			continue;
		}
		if (depth >= MAX_INCLUDE_DEPTH)
		{
			warn(line, "#include nested too deeply: " + name);
			continue;
		}
		const std::string resolved = resolveIncludePath(path, name);
		std::string included;
		if (!m_reader || !m_reader(resolved, included))
		{
			warn(line, "#include file not found: " + resolved);
			continue;
		}
		expandIncludes(resolved, included, lines, depth + 1);
	}
}

void INIPreprocessor::registerDefine(const Line& line)
{
	std::vector<std::string> tokens;
	for (const Piece& piece : split(trim(line.text)))
	{
		if (piece.isToken)
			tokens.push_back(piece.text);
	}
	if (tokens.size() < 2)
	{
		warn(line, "#define without a name");
		return;
	}

	const std::string& name = tokens[1];
	std::string value;
	for (std::size_t i = 2; i < tokens.size(); ++i)
	{
		if (!value.empty())
			value += ' ';
		value += tokens[i];
	}
	if (value.empty())
	{
		warn(line, "#define " + name + " has no value");
		return;
	}
	for (char c : name)
	{
		if (c >= 'a' && c <= 'z')
		{
			warn(line, "#define " + name + " is not uppercase");
			break;
		}
	}

	std::map<std::string, std::string>::iterator it = m_defines.find(name);
	if (it == m_defines.end())
	{
		m_defines[name] = value;
		return;
	}
	if (it->second == value)
		return;
	warn(line, "#define " + name + " defined more than once with a different value");
	it->second = value;
}

std::vector<INIPreprocessor::Piece> INIPreprocessor::split(const std::string& text)
{
	std::vector<Piece> pieces;
	std::size_t i = 0;
	while (i < text.size())
	{
		const bool separator = isSeparator(text[i]);
		std::size_t j = i;
		while (j < text.size() && isSeparator(text[j]) == separator)
			++j;
		Piece piece;
		piece.text = text.substr(i, j - i);
		piece.isToken = !separator;
		pieces.push_back(piece);
		i = j;
	}
	return pieces;
}

std::string INIPreprocessor::join(const std::vector<Piece>& pieces)
{
	std::string out;
	for (const Piece& piece : pieces)
		out += piece.text;
	return out;
}

bool INIPreprocessor::substitute(const std::vector<Piece>& pieces, std::vector<Piece>& out, int depth, bool expandLabeledDefines) const
{
	bool complete = true;
	bool inQuotes = false;
	for (const Piece& piece : pieces)
	{
		if (!piece.isToken)
		{
			out.push_back(piece);
			continue;
		}
		const bool allowLabel = expandLabeledDefines && !inQuotes;
		if (expandLabeledDefines && (std::count(piece.text.begin(), piece.text.end(), '"') % 2) != 0)
			inQuotes = !inQuotes;

		auto it = m_defines.find(piece.text);
		const std::size_t prefix = it == m_defines.end() && allowLabel ? labelLength(piece.text) : 0;
		if (prefix != 0)
			it = m_defines.find(piece.text.substr(prefix));
		if (it == m_defines.end())
		{
			out.push_back(piece);
			continue;
		}
		if (depth >= MAX_DEFINE_DEPTH)
		{
			out.push_back(piece);
			complete = false;
			continue;
		}
		if (prefix != 0)
			out.push_back({piece.text.substr(0, prefix), true});
		complete = substitute(split(it->second), out, depth + 1, allowLabel) && complete;
	}
	return complete;
}

void INIPreprocessor::warn(const Line& line, const std::string& message)
{
	warn(line.path, line.number, message);
}

void INIPreprocessor::warn(const std::string& path, int lineNumber, const std::string& message)
{
	m_warnings.push_back(path + "(" + std::to_string(lineNumber) + "): " + message);
}
