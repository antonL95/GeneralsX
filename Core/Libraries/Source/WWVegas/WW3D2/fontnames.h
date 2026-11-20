#pragma once

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace FontNames
{
	inline std::string Normalize(std::string_view name)
	{
		std::string normalized;
		normalized.reserve(name.size());
		for (const char ch : name)
		{
			if (ch == ' ' || ch == '-' || ch == '_' || ch == '\t')
				continue;
			normalized += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		}
		return normalized;
	}

	inline std::string Stem(std::string_view fileName)
	{
		const std::size_t slash = fileName.find_last_of("/\\");
		if (slash != std::string_view::npos)
			fileName.remove_prefix(slash + 1);
		const std::size_t dot = fileName.rfind('.');
		if (dot != std::string_view::npos)
			fileName = fileName.substr(0, dot);
		return std::string(fileName);
	}

	struct Candidate
	{
		std::vector<std::string> names;
		bool bold = false;
	};

	inline int Find(const std::vector<Candidate>& candidates, std::string_view requested, bool bold)
	{
		const std::string wanted = Normalize(requested);
		if (wanted.empty())
			return -1;

		int exact = -1;
		for (std::size_t index = 0; index < candidates.size(); ++index)
		{
			for (const std::string& name : candidates[index].names)
			{
				if (Normalize(name) != wanted)
					continue;
				if (candidates[index].bold == bold)
					return static_cast<int>(index);
				if (exact < 0)
					exact = static_cast<int>(index);
			}
		}
		if (exact >= 0)
			return exact;

		int prefix = -1;
		for (std::size_t index = 0; index < candidates.size(); ++index)
		{
			bool matches = false;
			for (const std::string& name : candidates[index].names)
				matches = matches || Normalize(name).rfind(wanted, 0) == 0;
			if (!matches)
				continue;
			if (prefix >= 0 && candidates[prefix].names.front() != candidates[index].names.front())
				return -1;
			if (prefix < 0 || candidates[index].bold == bold)
				prefix = static_cast<int>(index);
		}
		return prefix;
	}

	inline bool Same_Family(std::string_view requested, std::string_view family)
	{
		return !requested.empty() && Normalize(requested) == Normalize(family);
	}
}
