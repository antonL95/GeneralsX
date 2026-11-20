#pragma once

#include <string_view>

namespace INIRetailTolerance
{
bool isOptionalRetailINI(std::string_view name);
bool hasIndentedBlockEnd(std::string_view buffer, std::size_t next, std::size_t headerIndent);
}
