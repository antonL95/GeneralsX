#include "Common/INIRetailTolerance.h"
namespace INIRetailTolerance
{
bool isOptionalRetailINI(std::string_view name)
{
	if (name == "Data\\INI\\Default\\Voice")
		return true;
	return name.starts_with("Data\\") && !name.starts_with("Data\\INI\\") &&
		(name.ends_with("\\Language") || name.ends_with("\\CommandMap"));
}
}
