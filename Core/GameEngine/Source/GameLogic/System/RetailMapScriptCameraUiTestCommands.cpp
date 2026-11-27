#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include "Common/DataChunk.h"
#include "Common/MapReaderWriterInfo.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"

namespace
{
class CameraMapStream : public OutputStream, public ChunkInputStream
{
public:
	Int write(const void *data, Int count) override
	{
		const auto *bytes = static_cast<const char *>(data);
		m_bytes.insert(m_bytes.end(), bytes, bytes + count);
		return count;
	}

	Int read(void *data, Int count) override
	{
		const auto length = std::min<size_t>(count, m_bytes.size() - m_position);
		std::memcpy(data, m_bytes.data() + m_position, length);
		m_position += length;
		return static_cast<Int>(length);
	}

	UnsignedInt tell() override { return m_position; }
	Bool eof() override { return m_position == m_bytes.size(); }

	Bool absoluteSeek(UnsignedInt position) override
	{
		if (position > m_bytes.size())
			return FALSE;
		m_position = position;
		return TRUE;
	}

private:
	std::vector<char> m_bytes;
	UnsignedInt m_position = 0;
};

void writeCameraMap(CameraMapStream &stream, Bool malformed)
{
	DataChunkOutput output(&stream);
	output.openDataChunk("NamedCameras", 2);
	output.writeInt(1);
	if (!malformed)
	{
		output.writeReal(100.0f);
		output.writeReal(200.0f);
		output.writeReal(25.0f);
		output.writeAsciiString("CameraUiFixture");
		for (Real field : {0.6f, 0.2f, 1.4f, 1.0f, 0.8f, 65.0f})
			output.writeReal(field);
	}
	output.closeDataChunk();
}

void checkNamedCameras(const RetailTestCommands::Context &ctx)
{
	CameraMapStream valid;
	writeCameraMap(valid, FALSE);
	DataChunkInput validInput(&valid);
	const auto map = WorldHeightMap::readAuditMap(validInput);
	Bool matches = FALSE;
	if (map.Peek() && map->getNamedCameras().size() == 1)
	{
		const auto &camera = map->getNamedCameras().front();
		matches = camera.name == "CameraUiFixture" &&
			camera.lookAt.x == 100.0f && camera.lookAt.y == 200.0f && camera.lookAt.z == 25.0f &&
			camera.pitch == 0.6f && camera.roll == 0.2f && camera.yaw == 1.4f &&
			camera.zoom == 1.0f && camera.fieldOfView == 0.8f && camera.unknown == 65.0f;
	}
	CameraMapStream malformed;
	writeCameraMap(malformed, TRUE);
	Bool rejected = FALSE;
	try
	{
		DataChunkInput badInput(&malformed);
		rejected = WorldHeightMap::readAuditMap(badInput).Peek() == nullptr;
	}
	catch (ErrorCode)
	{
		rejected = TRUE;
	}
	std::fprintf(stderr, "TEST named-cameras frame=%u parsed=%d rejected=%d\n",
		ctx.frame, matches, rejected);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailMapScriptCameraUiTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("named-cameras-check", checkNamedCameras);
#endif
}
}
