#include "Common/INIRetailTolerance.h"

#include "Common/INIAudit.h"

namespace INIRetailTolerance
{
bool isOptionalRetailINI(std::string_view name)
{
	if (name == "Data\\INI\\Default\\Voice")
		return true;
	return name.starts_with("Data\\") && !name.starts_with("Data\\INI\\") &&
		(name.ends_with("\\Language") || name.ends_with("\\CommandMap"));
}

bool hasIndentedBlockEnd(std::string_view buffer, std::size_t next, std::size_t headerIndent)
{
	while (next < buffer.size())
	{
		const std::size_t end = buffer.find('\n', next);
		const std::string line(buffer.substr(next, end == std::string_view::npos ? end : end - next));
		const std::size_t indent = INIAudit::indentation(line);
		if (indent <= headerIndent && !INIAudit::trim(line).empty())
			return indent == headerIndent && INIAudit::closesSkippedBlock(line, headerIndent);
		if (end == std::string_view::npos)
			break;
		next = end + 1;
	}
	return false;
}
}
