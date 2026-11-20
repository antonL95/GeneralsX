#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace INILegacyLines
{
	inline constexpr std::size_t LINE_LIMIT = 1028;

	template <typename Visit>
	void forEach(std::string_view contents, const Visit& visit)
	{
		std::string line;
		std::size_t next = 0;
		for (;;)
		{
			line.clear();
			std::size_t length = 0;
			bool cut = false;
			bool endOfFile = false;
			while (length < LINE_LIMIT)
			{
				if (next == contents.size())
				{
					endOfFile = true;
					break;
				}
				const char c = contents[next++];
				if (c == '\n')
					break;
				++length;
				if (c == ';' || c == '\0')
					cut = true;
				if (!cut)
					line += (c > 0 && c < 32) ? ' ' : c;
			}
			visit(std::string_view(line));
			if (endOfFile)
				return;
		}
	}
}
