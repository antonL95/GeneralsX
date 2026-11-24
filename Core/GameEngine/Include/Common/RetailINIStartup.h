#pragma once

#include "Common/INI.h"

#include <array>
#include <functional>
#include <string>

namespace RetailINIStartup
{
	void loadLegend();
	bool hasSubsystem(const char* name);
	bool visitSubsystemFiles(const char* name, const std::function<void(const char*)>& load);
	bool loadSubsystem(const char* name, Xfer* xfer = nullptr,
		const char* extraBlockToken = nullptr, INIBlockParse extraBlockParse = nullptr);
	void preprocessSubsystem(const char* name);
	void loadFile(const char* path, Xfer* xfer = nullptr);
	void loadEnvironment();
	const std::array<std::string, 2>& environment();
}
