#include "Common/ArchiveFile.h"
#include "Common/BIGFileSystem.h"
#include "Common/BfmeAssetPath.h"
#include "Common/CommandLine.h"
#include "Common/CriticalSection.h"
#include "Common/DataChunk.h"
#include "Common/GlobalData.h"
#include "Common/MapReaderWriterInfo.h"
#include "Common/NameKeyGenerator.h"
#include "Common/RetailFileLayers.h"
#include "GameLogic/SidesList.h"
#include "StdDevice/Common/StdLocalFileSystem.h"
#include "W3DDevice/GameClient/W3DFileSystem.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
#include "WW3D2/assetmgr.h"
#include "WW3D2/ddsfile.h"
#include "WW3D2/textureimage.h"
#include "WW3D2/w3d_file.h"
#include "WWLib/TARGA.h"
#include "WWLib/chunkio.h"

#include <algorithm>
#include <bit>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
namespace fs = std::filesystem;

struct Entry
{
	ArchiveFile *archive;
	std::string path;
};

using Inventory = std::map<std::string, Entry>;
std::FILE *report;
std::set<std::string> references;
std::map<std::pair<unsigned, int>, unsigned> consumedChunks;
std::set<const Entry *> openModels;
const FileClass *observedFile;

std::string lowerPath(std::string name)
{
	for (char& c : name)
		c = c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return name;
}

std::string extension(const std::string& path)
{
	return fs::path(path).extension().string();
}

std::string clean(std::string value)
{
	for (char& c : value)
	{
		if (static_cast<unsigned char>(c) < 32)
			c = ' ';
	}
	return value;
}

void record(const char *kind, const std::string& path, const std::string& detail,
	const std::string& context = {}, unsigned count = 1)
{
	std::fprintf(report, "%s\t%s\t%s\t%s\t%u\n", kind, clean(path).c_str(),
		clean(detail).c_str(), clean(context).c_str(), count);
}

File *openEntry(const Entry& entry)
{
	const Int access = File::READ | File::BINARY;
	return entry.archive ? entry.archive->openFile(entry.path.c_str(), access)
		: TheLocalFileSystem->openFile(entry.path.c_str(), access);
}

class ProbeLocalFileSystem : public StdLocalFileSystem
{
public:
	File *openFile(const Char *path, Int access, size_t bufferSize) override
	{
		return StdLocalFileSystem::openFile(path, access,
			extension(lowerPath(path)) == ".big" ? 0 : bufferSize);
	}
};

class ProbeFile : public GameFileClass
{
public:
	explicit ProbeFile(const Entry *entry, const char *name) : m_entry(entry)
	{
		strlcpy(m_filename, name, sizeof(m_filename));
		m_fileExists = entry != nullptr;
	}

	~ProbeFile() override { Close(); }

	int Open(int rights = READ) override
	{
		Close();
		if (rights != READ || !m_entry || openModels.count(m_entry))
			return false;
		m_theFile = openEntry(*m_entry);
		if (m_theFile && extension(lowerPath(m_filename)) == ".w3d")
			openModels.insert(m_entry);
		return m_theFile != nullptr;
	}

	void Close() override
	{
		if (m_theFile)
			openModels.erase(m_entry);
		GameFileClass::Close();
	}

private:
	const Entry *m_entry;
};

Inventory::const_iterator resolveModel(const Inventory& inventory, const std::string& path)
{
	auto entry = inventory.find(path);
	if (entry == inventory.end() && extension(path) == ".w3d")
	{
		const auto name = fs::path(path).filename().string();
		entry = inventory.find("data/english/art/w3d/" + name);
		if (entry == inventory.end())
			entry = inventory.find("art/w3d/" + name);
		if (entry == inventory.end())
			entry = inventory.find(makeBfmeAssetPath("art/w3d/", name));
	}
	return entry;
}

class ProbeFileFactory : public FileFactoryClass
{
public:
	explicit ProbeFileFactory(const Inventory& files) : m_files(files) {}

	FileClass *Get_File(const char *name) override
	{
		const auto entry = resolveModel(m_files, lowerPath(name));
		return new ProbeFile(entry == m_files.end() ? nullptr : &entry->second, name);
	}

	void Return_File(FileClass *file) override { delete file; }

private:
	const Inventory& m_files;
};

class ProbeBIGFileSystem : public BIGFileSystem
{
public:
	const ArchiveFileMap& archives() const { return m_archiveFileMap; }
	using ArchiveFileSystem::getArchiveFile;
};

bool activeLoose(const fs::path& relative)
{
	static const std::set<std::string> rootExtensions = {".apt", ".const", ".dat", ".ini",
		".str", ".txt", ".tga", ".dds", ".csf"};
	if (relative.parent_path().empty())
		return rootExtensions.count(lowerPath(relative.extension().string())) != 0 &&
			lowerPath(relative.filename().string()) != "game.dat";
	static const std::set<std::string> roots = {"art", "data", "maps", "apt", "lang", "libraries",
		"bases", "livingworldmap", "palantir", "mainmenu_geometry", "skyrimmenu_geometry"};
	return roots.count(lowerPath(relative.begin()->string())) != 0 &&
		lowerPath(relative.extension().string()) != ".big";
}

void inventoryLoose(const fs::path& root, Inventory& inventory, bool localized = false)
{
	std::error_code error;
	if (!fs::is_directory(root, error))
		return;

	std::vector<fs::path> loosePaths;
	for (const auto& file : fs::recursive_directory_iterator(root))
	{
		if (!file.is_regular_file() || file.is_symlink())
			continue;
		const fs::path relative = file.path().lexically_relative(root);
		if (localized ? lowerPath(relative.extension().string()) != ".big" : activeLoose(relative))
			loosePaths.push_back(relative);
	}
	std::sort(loosePaths.begin(), loosePaths.end());
	for (const auto& relative : loosePaths)
	{
		const std::string name = lowerPath(relative.string());
		inventory.insert_or_assign(name, Entry{nullptr, (root / relative).string()});
	}
}

void inventoryArchives(const ProbeBIGFileSystem& big, Inventory& inventory)
{
	FilenameList files;
	for (const auto& [path, archive] : big.archives())
	{
		const auto archivePath = fs::absolute(path.str()).lexically_normal().string();
		unsigned char header[12] = {};
		if (std::FILE *file = std::fopen(archivePath.c_str(), "rb"))
		{
			const auto bytesRead = std::fread(header, 1, sizeof(header), file);
			std::fclose(file);
			if (bytesRead != sizeof(header))
			{
				record("archive", archivePath, "truncated-header", {}, 0);
				continue;
			}
		}
		else
		{
			record("archive", archivePath, "open-error", {}, 0);
			continue;
		}
		const std::string magic(reinterpret_cast<const char *>(header), 4);
		const unsigned count = (unsigned(header[8]) << 24) | (unsigned(header[9]) << 16) |
			(unsigned(header[10]) << 8) | header[11];

		record("archive", archivePath, "ok", magic, count);
		FilenameList names;
		archive->getFileListInDirectory("", "", "*", names, TRUE);
		record("archive-index", archivePath, "overlay", magic, names.size());
		files.merge(names);
	}
	for (const auto& path : files)
	{
		const std::string name = lowerPath(path.str());
		if (ArchiveFile* archive = big.getArchiveFile(path))
			inventory.emplace(name, Entry{archive, name});
	}
}

bool childResult(const char *kind, const std::string& name, auto&& probe)
{
	std::fflush(report);
	const pid_t child = fork();
	if (child < 0)
	{
		record(kind, name, "fork-error");
		return false;
	}
	if (child == 0)
	{
		TheAsciiStringCriticalSection = nullptr;
		TheUnicodeStringCriticalSection = nullptr;
		TheDmaCriticalSection = nullptr;
		TheMemoryPoolCriticalSection = nullptr;
		alarm(20);
		try
		{
			const bool ok = probe();
			record(kind, name, ok ? "ok" : "failed");
		}
		catch (...)
		{
			record(kind, name, "exception");
		}
		std::fflush(report);
		_exit(0);
	}
	int status;
	pid_t waited;
	do
	{
		waited = waitpid(child, &status, 0);
	} while (waited == -1 && errno == EINTR);
	if (waited != child)
	{
		record(kind, name, "wait-error");
		return false;
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
	{
		const std::string error = WIFSIGNALED(status)
			? "signal-" + std::to_string(WTERMSIG(status))
			: "exit-" + std::to_string(WEXITSTATUS(status));
		record(kind, name, error);
	}
	return true;
}

std::string hexId(unsigned id)
{
	char out[16];
	std::snprintf(out, sizeof(out), "0x%08x", id);
	return out;
}

void observeChunk(FileClass *file, unsigned id, int end, unsigned used)
{
	if (file == observedFile)
		consumedChunks[{id, end}] = used;
}

struct ModelChunk
{
	unsigned parent;
	unsigned id;
	int end;
	unsigned length;
	bool unknownMotionScalar;
};

void reference(const std::string& file, const std::string& name, const char *source)
{
	const std::string normalized = lowerPath(name);
	if (!normalized.empty() && normalized != "none")
		record("texture-ref", file, normalized, source);
}

void modelTexture(ChunkLoadClass& chunks, const std::string& path, unsigned id, unsigned length)
{
	if (length == 0 || length > 4096)
		return;
	if (id == W3D_CHUNK_TEXTURE_NAME)
	{
		std::string name(length, '\0');
		if (chunks.Read(name.data(), length) == length)
			reference(path, name.c_str(), "w3d");
	}
	else if (id == 0x53 && length >= 12)
	{
		unsigned type, propertyLength, valueLength;
		if (chunks.Read(&type, 4) == 4 && type == 1 &&
			chunks.Read(&propertyLength, 4) == 4 && propertyLength <= length - 12 &&
			chunks.Seek(propertyLength) == propertyLength &&
			chunks.Read(&valueLength, 4) == 4 && valueLength <= length - 12 - propertyLength)
		{
			std::string name(valueLength, '\0');
			if (chunks.Read(name.data(), valueLength) == valueLength)
				reference(path, name.c_str(), "w3d-shader");
		}
	}
	else if (id == W3D_CHUNK_EMITTER_INFO && length >= 260)
	{
		char name[261] = {};
		if (chunks.Read(name, 260) == 260)
			reference(path, name, "w3d-emitter");
	}
	else if ((id == W3D_CHUNK_RING || id == W3D_CHUNK_SPHERE) && length >= 148)
	{
		char name[33] = {};
		if (chunks.Seek(116) == 116 && chunks.Read(name, 32) == 32)
			reference(path, name, "w3d-primitive");
	}
}

bool catalogModel(ProbeFile& file, ChunkLoadClass& chunks, const std::string& path,
	int end, unsigned parent, std::vector<ModelChunk>& catalog)
{
	while (file.Seek(0, SEEK_CUR) < end)
	{
		const int start = file.Seek(0, SEEK_CUR);
		if (end - start < 8 || !chunks.Open_Chunk())
			return false;
		const unsigned id = chunks.Cur_Chunk_ID();
		const unsigned length = chunks.Cur_Chunk_Length();
		if (length > static_cast<unsigned>(end - start - 8))
			return false;
		const int chunkEnd = start + 8 + length;
		bool unknownMotionScalar = false;
		if (id == W3D_CHUNK_COMPRESSED_ANIMATION_MOTION_CHANNEL && length >= 8)
		{
			uint8 header[4];
			unknownMotionScalar = chunks.Read(header, sizeof(header)) == sizeof(header) && header[3] == 15;
		}
		catalog.push_back({parent, id, chunkEnd, length, unknownMotionScalar});
		if (chunks.Contains_Chunks() && chunks.Cur_Chunk_Depth() < 128)
		{
			if (!catalogModel(file, chunks, path, chunkEnd, id, catalog))
				return false;
		}
		else
		{
			modelTexture(chunks, path, id, length);
		}
		chunks.Close_Chunk();
	}
	return true;
}

bool probeModel(const std::string& name, const Entry& entry)
{
	ProbeFile file(&entry, name.c_str());
	if (!file.Open())
		return false;
	std::vector<ModelChunk> catalog;
	ChunkLoadClass chunks(&file);
	const bool intact = catalogModel(file, chunks, name, file.Size(), ~0u, catalog);
	file.Close();
	std::fflush(report);
	if (!intact)
		record("model-structure", name, "invalid chunk bounds");

	WW3DAssetManager manager;
	manager.Set_WW3D_Load_On_Demand(true);
	observedFile = &file;
	ChunkLoadClass::Audit_Observer = observeChunk;
	const bool loaded = manager.Load_3D_Assets(file);
	ChunkLoadClass::Audit_Observer = nullptr;
	unsigned unknownMotionScalars = 0;
	std::map<std::tuple<unsigned, unsigned, bool>, unsigned> unsupported;
	for (const auto& chunk : catalog)
	{
		const auto seen = consumedChunks.find({chunk.id, chunk.end});
		const bool unopened = seen == consumedChunks.end();
		if (chunk.unknownMotionScalar && !unopened && seen->second == chunk.length)
			++unknownMotionScalars;
		if (unopened || (chunk.length > 0 && seen->second == 0))
			++unsupported[{chunk.parent, chunk.id, unopened}];
	}
	for (const auto& [key, count] : unsupported)
	{
		const auto [parent, id, unopened] = key;
		record(unopened ? "model-unvisited" : "model-skipped", name, hexId(id),
			parent == ~0u ? "root" : hexId(parent), count);
	}
	if (unknownMotionScalars != 0)
		record("model-motion", name, "skipped-type-15", "", unknownMotionScalars);
	if (loaded && !intact && !catalog.empty())
		record("model-recovery", name, "partial");
	return loaded && intact && !catalog.empty();
}

struct MapRule
{
	std::string_view parent;
	std::string_view name;
	unsigned maxVersion;
};

const MapRule mapRules[] = {
	{"", "HeightMapData", K_HEIGHT_MAP_VERSION_5},
	{"", "BlendTileData", K_BLEND_TILE_VERSION_8},
	{"", "WorldInfo", K_WORLDDICT_VERSION_1},
	{"", "ObjectsList", K_OBJECTS_VERSION_3},
	{"", "PolygonTriggers", K_TRIGGERS_VERSION_4},
	{"", "SidesList", SidesList::MAX_MAP_SIDES_VERSION},
	{"", "Teams", 1},
	{"", "GlobalLighting", K_LIGHTING_VERSION_8},
	{"", "WaypointsList", K_WAYPOINTS_VERSION_1},
	{"", "MapPreview", K_MAPPREVIEW_VERSION_1},
	{"", "FUNKY_TILING", 1},
	{"ObjectsList", "Object", K_OBJECTS_VERSION_3},
	{"SidesList", "PlayerScriptsList", 5},
	{"PlayerScriptsList", "ScriptList", 1},
	{"ScriptList", "ScriptGroup", 2},
	{"ScriptList", "Script", 2},
	{"ScriptGroup", "Script", 2},
	{"Script", "OrCondition", 1},
	{"Script", "ScriptAction", 2},
	{"Script", "ScriptActionFalse", 2},
	{"OrCondition", "Condition", 4},
};

bool skipScriptPrefix(DataChunkInput& input, std::string_view label, unsigned version)
{
	if (label == "ScriptGroup")
	{
		input.readAsciiString();
		input.readByte();
		if (version >= 2)
			input.readByte();
	}
	else if (label == "Script")
	{
		for (int i = 0; i < 4; ++i)
			input.readAsciiString();
		for (int i = 0; i < 6; ++i)
			input.readByte();
		if (version >= 2)
			input.readInt();
		if (version == 5)
		{
			input.readByte();
			input.readInt();
		}
		if (version >= 3)
		{
			input.readByte();
			input.readByte();
			input.readInt();
			input.readByte();
			input.readAsciiString();
		}
		if (version >= 4)
			input.readAsciiString();
		if (version >= 6)
		{
			input.readInt();
			input.readByte();
			input.readByte();
		}
	}
	return !input.atEndOfChunk();
}

using MapTally = std::map<std::tuple<std::string, std::string, unsigned, std::string>, unsigned>;

bool walkMap(DataChunkInput& input, CachedFileInputStream& stream, const std::string& parent,
	unsigned end, bool parentHandled, MapTally& tally, unsigned depth = 0)
{
	while (stream.tell() < end)
	{
		if (depth >= 128 || end - stream.tell() < 10)
			return false;
		DataChunkVersionType version = 0;
		const std::string label = input.openDataChunk(&version).str();
		const unsigned size = input.getChunkDataSize();
		if (label.empty() || size > end - stream.tell())
			return false;
		const auto rule = std::find_if(std::begin(mapRules), std::end(mapRules), [&](const MapRule& row) {
			return row.name == label && row.parent == parent;
		});
		const bool versionKnown = rule != std::end(mapRules) && version > 0 && version <= rule->maxVersion &&
			(label != "GlobalLighting" || version <= K_LIGHTING_VERSION_3 || version >= K_LIGHTING_VERSION_7);
		const char *status = rule == std::end(mapRules) ? "unknown" :
			!versionKnown ? "version" : parentHandled ? "known" : "unreachable";
		++tally[{parent, label, version, status}];

		const unsigned chunkEnd = stream.tell() + size;
		const bool container = label == "ObjectsList" || label == "PlayerScriptsList" ||
			label == "ScriptList" || label == "ScriptGroup" || label == "Script" || label == "OrCondition" ||
			label == "MPPositionList" || label == "LibraryMapLists";
		if (container && size != 0 && skipScriptPrefix(input, label, version) &&
			!walkMap(input, stream, label, chunkEnd, parentHandled && versionKnown, tally, depth + 1))
			return false;
		input.closeDataChunk();
	}
	return stream.tell() == end;
}

class ProbeMapStream : public CachedFileInputStream
{
public:
	unsigned size() const { return m_size; }
};

bool probeMap(const std::string& name, const Entry& entry)
{
	File *file = openEntry(entry);
	if (!file)
		return false;
	char magic[4] = {};
	if (file->read(magic, sizeof(magic)) != sizeof(magic) || file->seek(0, File::START) != 0)
	{
		file->close();
		return false;
	}
	record("map-container", name, std::string(magic, sizeof(magic)));
	ProbeMapStream stream;
	if (!stream.open(file))
		return false;
	DataChunkInput input(&stream);
	if (!input.isValidFileType())
		return false;
	MapTally tally;
	const bool ok = walkMap(input, stream, "", stream.size(), true, tally);
	for (const auto& [key, count] : tally)
	{
		const auto& [parent, label, version, status] = key;
		record("map-chunk", name, label, (parent.empty() ? "root" : parent) + ":" +
			std::to_string(version) + ":" + status, count);
	}
	return ok;
}

bool writeMapHeights(const char *path, const WorldHeightMap &map)
{
	std::FILE *dump = std::fopen(path, "wb");
	if (!dump)
		return false;
	const Int width = map.getXExtent();
	const Int height = map.getYExtent();
	std::fprintf(dump, "%d %d %u %a\n", width, height, map.getHeightSampleBytes(), map.getHeightScale());
	for (Int y = 0; y < height; ++y)
	{
		for (Int x = 0; x < width; ++x)
		{
			const UnsignedShort sample = map.getHeight(x, y);
			const UnsignedInt worldBits = std::bit_cast<UnsignedInt>(map.toWorldHeight(sample));
			const UnsignedByte cell[] = {
				static_cast<UnsignedByte>(sample),
				static_cast<UnsignedByte>(sample >> 8),
				static_cast<UnsignedByte>(worldBits),
				static_cast<UnsignedByte>(worldBits >> 8),
				static_cast<UnsignedByte>(worldBits >> 16),
				static_cast<UnsignedByte>(worldBits >> 24)
			};
			std::fwrite(cell, sizeof(cell), 1, dump);
		}
	}
	const bool ok = !std::ferror(dump);
	return std::fclose(dump) == 0 && ok;
}

bool probeMapSides(const std::string& name, const Entry& entry, const char *heightDump = nullptr)
{
	CachedFileInputStream stream;
	if (!stream.open(openEntry(entry)))
	{
		record("map-error", name, "open");
		return false;
	}

	SidesList sides;
	TheSidesList = &sides;
	bool parsed = false;
	try
	{
		DataChunkInput input(&stream);
		const auto map = WorldHeightMap::readAuditMap(input);
		parsed = map.Peek() != nullptr;
		if (parsed)
		{
			record("map-sides", name, "SidesList", "", sides.getNumSides());
			record("map-teams", name, "Teams", "", sides.getNumTeams());
			record("map-side-flag", name, "SidesList", "", sides.getMapSideFlag());
			if (const DataChunkInfo *unsupported = input.getUnsupportedRequiredChunk())
				record("map-stop", name, unsupported->label.str(), std::to_string(unsupported->version));
			if (map->getXExtent() > 0)
				record("map-heights", name, std::to_string(map->getXExtent()), std::to_string(map->getYExtent()),
					map->getXExtent() * map->getYExtent());
			const auto opaque = map->getOpaqueBfmeLighting();
			if (!opaque.empty())
			{
				constexpr char digits[] = "0123456789abcdef";
				std::string hex;
				hex.reserve(2 * opaque.size());
				for (UnsignedByte byte : opaque)
				{
					hex += digits[byte >> 4];
					hex += digits[byte & 0xf];
				}
				record("map-lighting-opaque", name, std::move(hex), "", opaque.size());
			}
			if (heightDump && (map->getXExtent() == 0 || !writeMapHeights(heightDump, *map)))
			{
				record("map-error", name, "height-dump");
				TheSidesList = nullptr;
				return false;
			}
			const char *blendDump = TheGlobalData->m_initialFile.isNotEmpty()
				? std::getenv("GENERALSX_MAP_BLEND_DUMP") : nullptr;
			if (blendDump && !map->writeBlendTileChecksums(blendDump))
			{
				record("map-error", name, "blend-dump");
				TheSidesList = nullptr;
				return false;
			}
		}
	}
	catch (ErrorCode error)
	{
		record("map-error", name, "exception", std::to_string(static_cast<unsigned>(error)));
		TheSidesList = nullptr;
		return false;
	}
	TheSidesList = nullptr;
	if (!parsed)
		record("map-error", name, "parse");
	return parsed;
}

int probeMapSides(const Inventory& inventory)
{
	NameKeyGenerator keys;
	keys.init();
	TheNameKeyGenerator = &keys;

	const std::string selected = lowerPath(TheGlobalData->m_initialFile.str());
	bool ok = true;
	if (!selected.empty())
	{
		const auto entry = inventory.find(selected);
		if (entry == inventory.end())
		{
			record("map-error", selected, "missing");
			ok = false;
		}
		else
			ok = probeMapSides(entry->first, entry->second, std::getenv("GENERALSX_MAP_HEIGHT_DUMP"));
	}
	else
	{
		for (const auto& [name, entry] : inventory)
		{
			if (extension(name) == ".map" && !probeMapSides(name, entry))
				ok = false;
		}
	}
	TheNameKeyGenerator = nullptr;
	const int closeResult = std::fclose(report);
	return !ok || closeResult != 0;
}

void scanIni(const std::string& name, const Entry& entry)
{
	File *file = openEntry(entry);
	if (!file)
		return;
	const int size = file->size();
	if (size <= 0 || size > 16 * 1024 * 1024)
	{
		file->close();
		return;
	}

	std::unique_ptr<char[]> bytes(file->readEntireAndClose());
	if (!bytes)
		return;
	static const std::regex explicitName(R"asset((?:"([^"\r\n]+\.(?:dds|tga))"|([\w/\\.+!-]+\.(?:dds|tga)))(?![\w.]))asset",
		std::regex::icase);
	static const std::regex textureField(R"asset(^\s*(?:Texture(?:Name|[0-9]|ReallyDamaged|Damaged|Cross|Broken)?|\w+Texture)\s*=\s*(?:"([^"]+)"|([\w/\\.+!-]+)))asset",
		std::regex::icase);
	std::string data(bytes.get(), size);
	for (std::size_t start = 0; start < data.size();)
	{
		const auto newline = data.find('\n', start);
		std::string line = data.substr(start, newline - start);
		const auto comment = std::min(line.find(';'), line.find("//"));
		line.resize(std::min(line.size(), comment));
		for (std::sregex_iterator it(line.begin(), line.end(), explicitName), end; it != end; ++it)
			reference(name, (*it)[1].matched ? (*it)[1].str() : (*it)[2].str(), "ini");
		std::smatch field;
		if (std::regex_search(line, field, textureField))
		{
			const std::string token = lowerPath(field[1].matched ? field[1].str() : field[2].str());
			if (extension(token).empty() && token != "none" && token != "null" &&
				token != "yes" && token != "no" && token.find_first_not_of("0123456789+-") != std::string::npos)
				reference(name, token, "ini-implicit");
		}
		if (newline == std::string::npos)
			break;
		start = newline + 1;
	}
}

bool probeTexture(const std::string& path)
{
	if (extension(path) == ".jpg")
	{
		TextureImage texture;
		const bool loaded = texture.Load_Jpeg(path.c_str());
		if (loaded)
		{
			record("texture-format", path, texture.Has_Companion() ? "companion" : "opaque", "jpg");
			const auto* pixels = reinterpret_cast<const unsigned char*>(texture.GetImage());
			unsigned minimum = 255;
			unsigned maximum = 0;
			for (int i = 0; i < texture.Header.Width * texture.Header.Height; ++i)
			{
				const unsigned alpha = pixels[4 * i + 3];
				minimum = std::min(minimum, alpha);
				maximum = std::max(maximum, alpha);
			}
			record("texture-alpha", path, std::to_string(minimum), std::to_string(maximum));
		}
		return loaded;
	}

	if (extension(path) == ".dds")
	{
		file_auto_ptr file(_TheFileFactory, path.c_str());
		char header[88] = {};
		if (!file->Open() || file->Read(header, sizeof(header)) != sizeof(header) ||
			std::string_view(header, 4) != "DDS ")
			return false;
		const int size = file->Size();
		file->Close();
		const std::string fourCC(header + 84, 4);
		DDSFileClass texture(path.c_str(), 0);
		const WW3DFormat format = texture.Get_Format();
		record("texture-format", path, fourCC, "dds");
		if (format < WW3D_FORMAT_DXT1 || format > WW3D_FORMAT_DXT5 ||
			texture.Get_Width(0) == 0 || texture.Get_Height(0) == 0 || size < 128)
			return false;
		return texture.Get_Level_Size(0) <= static_cast<unsigned>(size - 128) && texture.Load();
	}
	Targa texture;
	const long result = texture.Load(path.c_str(), TGAF_IMAGE | TGAF_PAL);
	record("texture-format", path, std::to_string(result), "tga");
	return result == 0 && texture.GetImage() != nullptr;
}

std::string resolveTexture(const Inventory& inventory, const std::string& reference)
{
	const auto ext = extension(reference);
	const auto stem = ext.empty() ? reference : reference.substr(0, reference.size() - ext.size());
	for (const char *suffix : {".dds", ".tga"})
	{
		const auto name = stem + suffix;
		if (reference.find('/') != std::string::npos && inventory.count(name))
			return name;
		for (const char *prefix : {"data/english/art/textures/", "art/textures/", "art/terrain/", ""})
		{
			const std::string key = prefix + name;
			if (inventory.find(key) != inventory.end())
				return key;
		}
	}
	for (const char *suffix : {".dds", ".tga"})
	{
		const auto compiled = makeBfmeAssetPath("art/compiledtextures/", stem + suffix);
		if (inventory.count(compiled))
			return compiled;
	}
	const auto jpeg = lowerPath(makeBfmeJpegPath(stem + ".jpg"));
	if (inventory.count(jpeg))
		return jpeg;
	return {};
}

bool collectReferences(const char *reportPath, const Inventory& inventory)
{
	std::fflush(report);
	std::FILE *input = std::fopen(reportPath, "r");
	if (!input)
		return false;
	char *buffer = nullptr;
	std::size_t capacity = 0;
	while (getline(&buffer, &capacity, input) >= 0)
	{
		std::string_view line(buffer);
		if (!line.starts_with("texture-ref\t"))
			continue;
		const auto start = line.find('\t', 12) + 1;
		const auto end = line.find('\t', start);
		references.emplace(line.substr(start, end - start));
	}
	std::free(buffer);
	std::fclose(input);

	std::set<std::string> candidates;
	for (const auto& name : references)
	{
		const std::string resolved = resolveTexture(inventory, name);
		record("texture-resolution", name, resolved.empty() ? "missing" : "found", resolved);
		if (!resolved.empty())
			candidates.insert(resolved);
	}
	for (const auto& path : candidates)
	{
		if (!childResult("texture", path, [&] { return probeTexture(path); }))
			return false;
	}
	return true;
}
}

int RunAssetProbe(const char *reportPath, bool mapSidesOnly)
{
	report = std::fopen(reportPath, "w");
	if (!report)
		return 1;

	ProbeLocalFileSystem local;
	TheLocalFileSystem = &local;
	ProbeBIGFileSystem big;
	CommandLine::parseCommandLineForStartup();
	big.init();
	CommandLine::parseCommandLineForModInit();
	big.loadMods();
	if (mapSidesOnly)
		CommandLine::parseCommandLineForEngineInit();
	Inventory inventory;
	inventoryArchives(big, inventory);
	inventoryLoose(fs::current_path(), inventory);
	if (RetailFileLayers::enabled())
	{
		inventoryLoose(RetailFileLayers::languageDirectory().str(), inventory, true);
		if (TheGlobalData->m_modDir.isNotEmpty())
			inventoryLoose(TheGlobalData->m_modDir.str(), inventory);
	}
	if (mapSidesOnly)
		return probeMapSides(inventory);
	ProbeFileFactory factory(inventory);
	_TheFileFactory = &factory;

	std::set<std::string> modelNames;
	for (const auto& [name, entry] : inventory)
	{
		const std::string ext = extension(name);
		const auto source = fs::absolute(entry.archive ? entry.archive->getName().str() : entry.path)
			.lexically_normal().string();
		record("entry", name, ext, source);
		if (ext == ".w3d")
			modelNames.insert(fs::path(name).filename().string());
	}
	for (const auto& name : modelNames)
	{
		const auto entry = resolveModel(inventory, name);
		if (entry == inventory.end())
			record("model-resolution", name, "missing");
		else if (entry->first == makeBfmeAssetPath("art/w3d/", name))
			record("model-resolution", name, "bfme", entry->first);
		else
			record("model-resolution", name, "stock", entry->first);
	}

	for (const auto& [name, entry] : inventory)
	{
		const std::string ext = extension(name);
		if (ext == ".ini" || ext == ".inc")
			scanIni(name, entry);
		else if (ext == ".w3d" && !childResult("model", name, [&] { return probeModel(name, entry); }))
			return 1;
		else if (ext == ".map" && !childResult("map", name, [&] { return probeMap(name, entry); }))
			return 1;
	}

	if (!collectReferences(reportPath, inventory))
		return 1;
	record("probe", "", "done");
	const int result = std::fclose(report);
	_TheFileFactory = _TheSimpleFileFactory;
	TheLocalFileSystem = nullptr;
	return result != 0;
}
