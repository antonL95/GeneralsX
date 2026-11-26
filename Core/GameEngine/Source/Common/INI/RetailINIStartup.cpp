#include "PreRTS.h"
#include "Common/RetailINIStartup.h"

#include "Common/FileSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <sstream>
#include <string_view>
#include <vector>

namespace
{
enum class EntryKind { File, DebugFile, Path, Exclude, Cinematics };

struct Entry
{
	EntryKind kind;
	AsciiString path;
};

struct LegendSection
{
	AsciiString loader;
	AsciiString extension;
	std::vector<Entry> entries;
};

std::map<std::string, LegendSection> legend;
std::array<std::string, 2> environmentFiles;
RetailINIStartup::ShadowSettings baseShadowSettings;
RetailINIStartup::ShadowSettings mapShadowSettings;

bool validShadowViewDistance(Real value)
{
	return value > 0.0f && std::isfinite(value) && std::isfinite(1.0f / value);
}

void readEnvironmentShadowSettings(const std::string& content)
{
	std::istringstream input(content);
	bool inShadowMap = false;
	for (std::string line; std::getline(input, line); )
	{
		std::istringstream row(line);
		std::string name;
		if (!(row >> name))
			continue;
		if (stricmp(name.c_str(), "ShadowMap") == 0)
		{
			inShadowMap = true;
			continue;
		}
		if (!inShadowMap)
			continue;
		if (stricmp(name.c_str(), "End") == 0)
			break;
		char separator = 0;
		Real value = 0.0f;
		if (!(row >> separator >> value) || separator != '=' || !std::isfinite(value))
			continue;
		if (stricmp(name.c_str(), "MapSize") == 0 && value >= 256 && value <= 4096)
			baseShadowSettings.mapSize = static_cast<Int>(value);
		else if (stricmp(name.c_str(), "MaxViewDistance") == 0 && validShadowViewDistance(value))
			baseShadowSettings.maxViewDistance = value;
		else if (stricmp(name.c_str(), "MinShadowedTerrainHeight") == 0)
			baseShadowSettings.minTerrainHeight = value;
	}
}

const char* const environmentPaths[] = {
	"Data\\INI\\Default\\Environment.ini",
	"Data\\INI\\Environment.ini"
};

std::string virtualPath(const char* path)
{
	std::string result(path);
	for (char& c : result)
	{
		if (c == '/')
			c = '\\';
		else if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
	}
	while (!result.empty() && result.back() == '\\')
		result.pop_back();
	return result;
}

template<EntryKind kind>
void parseEntry(INI* ini, void* instance, void*, const void*)
{
	AsciiString path;
	INI::parseQuotedAsciiString(ini, nullptr, &path, nullptr);
	if (path.isNotEmpty())
		static_cast<LegendSection*>(instance)->entries.push_back({kind, path});
}

void parseLegendSection(INI* ini)
{
	static const FieldParse fields[] = {
		{ "Loader", INI::parseAsciiString, nullptr, offsetof(LegendSection, loader) },
		{ "Extension", INI::parseAsciiString, nullptr, offsetof(LegendSection, extension) },
		{ "InitFile", parseEntry<EntryKind::File>, nullptr, 0 },
		{ "InitFileDebug", parseEntry<EntryKind::DebugFile>, nullptr, 0 },
		{ "InitPath", parseEntry<EntryKind::Path>, nullptr, 0 },
		{ "ExcludePath", parseEntry<EntryKind::Exclude>, nullptr, 0 },
		{ "IncludePathCinematics", parseEntry<EntryKind::Cinematics>, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	const std::string name = ini->getNextToken();
	ini->initFromINI(&legend[name], fields);
}

bool containsPath(std::string_view root, std::string_view file)
{
	return file == root || (file.starts_with(root) && file[root.size()] == '\\');
}

void traceSubsystem(const char* name)
{
	static const bool trace = std::getenv("GENERALSX_INI_TRACE_DEFINE") != nullptr;
	if (trace)
		fprintf(stderr, "[INI subsystem] %s\n", name);
}

}

namespace RetailINIStartup
{
bool visitSubsystemFiles(const char* name, const std::function<void(const char*)>& load)
{
	traceSubsystem(name);
	const auto found = legend.find(name);
	if (found == legend.end())
		return false;

	const LegendSection& section = found->second;
	if (section.loader.compareNoCase("INI") != 0)
		return false;

	std::vector<std::string> exclusions;
	for (const Entry& entry : section.entries)
	{
		if (entry.kind == EntryKind::Exclude)
			exclusions.push_back(virtualPath(entry.path.str()));
	}

	for (const Entry& entry : section.entries)
	{
		switch (entry.kind)
		{
			case EntryKind::DebugFile:
#if defined(RTS_DEBUG)
				[[fallthrough]];
#else
				break;
#endif
			case EntryKind::File:
				if (TheFileSystem->doesFileExist(entry.path.str()))
					load(entry.path.str());
				break;
			case EntryKind::Path:
			{
				const std::string root = virtualPath(entry.path.str()) + '\\';
				FilenameList files;
				TheFileSystem->getFileListInDirectory(root.c_str(), "*.ini", files, TRUE);
				std::vector<const char*> nested;
				for (const AsciiString& file : files)
				{
					const std::string path = virtualPath(file.str());
					if (!path.starts_with(root) || std::any_of(exclusions.begin(), exclusions.end(),
						[&](const std::string& exclusion) { return containsPath(exclusion, path); }))
						continue;

					if (path.find('\\', root.size()) == std::string::npos)
						load(file.str());
					else
						nested.push_back(file.str());
				}
				for (const char* file : nested)
					load(file);
				break;
			}
			case EntryKind::Exclude:
			case EntryKind::Cinematics:
				break;
		}
	}
	return true;
}

void loadLegend()
{
	legend.clear();
	INI::resetPreprocessor();
	const char* path = "Data\\INI\\Default\\SubsystemLegendExpansion1.ini";
	if (TheFileSystem->doesFileExist(path))
	{
		INI ini;
		ini.load(path, INI_LOAD_OVERWRITE, nullptr, "LoadSubsystem", parseLegendSection);
	}
}

bool hasSubsystem(const char* name)
{
	return legend.find(name) != legend.end();
}

bool loadSubsystem(const char* name, Xfer* xfer, const char* extraBlockToken, INIBlockParse extraBlockParse)
{
	INI ini;
	return visitSubsystemFiles(name, [&](const char* path) {
		if (extraBlockParse)
			ini.load(path, INI_LOAD_OVERWRITE, xfer, extraBlockToken, extraBlockParse);
		else
			ini.load(path, INI_LOAD_OVERWRITE, xfer);
	});
}

void preprocessSubsystem(const char* name)
{
	visitSubsystemFiles(name, [](const char* path) { INI::preprocessFile(path); });
}

void loadFile(const char* path, Xfer* xfer)
{
	if (TheFileSystem->doesFileExist(path))
	{
		INI ini;
		ini.load(path, INI_LOAD_OVERWRITE, xfer);
	}
}

void loadEnvironment()
{
	baseShadowSettings = {};
	for (std::size_t i = 0; i < environmentFiles.size(); ++i)
	{
		const char* path = environmentPaths[i];
		environmentFiles[i] = TheFileSystem->doesFileExist(path) ? INI::preprocessFile(path) : std::string();
		readEnvironmentShadowSettings(environmentFiles[i]);
	}
	resetMapShadowSettings();
}

void resetMapShadowSettings()
{
	mapShadowSettings = baseShadowSettings;
}

const ShadowSettings& shadowSettings()
{
	return mapShadowSettings;
}

void parseShadowMap(INI* ini)
{
	static const FieldParse fields[] = {
		{ "MapSize", INI::parseInt, nullptr, offsetof(ShadowSettings, mapSize) },
		{ "MaxViewDistance", INI::parseReal, nullptr, offsetof(ShadowSettings, maxViewDistance) },
		{ "MinShadowedTerrainHeight", INI::parseReal, nullptr, offsetof(ShadowSettings, minTerrainHeight) },
		{ nullptr, nullptr, nullptr, 0 },
	};
	ShadowSettings parsed = mapShadowSettings;
	ini->initFromINI(&parsed, fields);
	if (parsed.mapSize >= 256 && parsed.mapSize <= 4096)
		mapShadowSettings.mapSize = parsed.mapSize;
	if (validShadowViewDistance(parsed.maxViewDistance))
		mapShadowSettings.maxViewDistance = parsed.maxViewDistance;
	if (std::isfinite(parsed.minTerrainHeight))
		mapShadowSettings.minTerrainHeight = parsed.minTerrainHeight;
}

const std::array<std::string, 2>& environment()
{
	return environmentFiles;
}
}
