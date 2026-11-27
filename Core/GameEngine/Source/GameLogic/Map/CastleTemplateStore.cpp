#include "PreRTS.h"

#include "GameLogic/CastleTemplateStore.h"

#include <map>

#include "Common/DataChunk.h"
#include "Common/Errors.h"
#include "Common/INIAudit.h"
#include "Common/MapReaderWriterInfo.h"

namespace CastleTemplates
{
namespace
{
std::map<std::string, Camp> s_camps;

Bool readTemplates(DataChunkInput& file, DataChunkInfo* info, void* userData)
{
	if (info->dataSize < 8 || static_cast<std::size_t>(info->dataSize) > MAX_BODY_SIZE)
		return FALSE;

	auto& result = *static_cast<std::optional<Camp>*>(userData);
	if (result)
		return FALSE;

	std::vector<std::uint8_t> bytes(info->dataSize);
	file.readArrayOfBytes(reinterpret_cast<char*>(bytes.data()), info->dataSize);
	result = parse(bytes, info->version);
	return result.has_value();
}

std::optional<Camp> load(const AsciiString& path)
{
	CachedFileInputStream stream;
	if (!stream.open(path))
		return std::nullopt;

	DataChunkInput file(&stream);
	file.registerParser("CastleTemplates", AsciiString::TheEmptyString, readTemplates);
	std::optional<Camp> result;
	if (!file.parse(&result))
		return std::nullopt;

	if (!result)
		result.emplace();
	return result;
}

const Camp* reportFailure(const AsciiString& path)
{
	if (!INIAudit::isEnabled())
		throw ERROR_CORRUPT_FILE_FORMAT;
	INIAudit::record("error", "Cannot load castle template data", path.str(), 0);
	return nullptr;
}
}

const Camp* find(const AsciiString& camp, bool required)
{
	std::string key(camp.str());
	if (key.empty() || key == "." || key == ".." || key.size() > 255 ||
		key.find_first_of("/\\:") != std::string::npos)
		return required ? reportFailure(camp) : nullptr;

	for (char& ch : key)
		if (ch >= 'A' && ch <= 'Z')
			ch += 'a' - 'A';

	const auto found = s_camps.find(key);
	if (found != s_camps.end())
		return &found->second;

	AsciiString path;
	path.format("Bases\\%s\\%s.bse", camp.str(), camp.str());
	std::optional<Camp> result;
	try
	{
		result = load(path);
	}
	catch (ErrorCode)
	{
		return required ? reportFailure(path) : nullptr;
	}

	if (!result)
		return required ? reportFailure(path) : nullptr;
	return &s_camps.emplace(std::move(key), std::move(*result)).first->second;
}

void reset()
{
	s_camps.clear();
}
}
