#include "PreRTS.h"

#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "GameClient/VideoPlayer.h"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <vector>

namespace
{
class CaptureBuffer final : public VideoBuffer
{
public:
	CaptureBuffer() : VideoBuffer(TYPE_A8R8G8B8) {}

	Bool allocate(UnsignedInt width, UnsignedInt height) override
	{
		if (!width || !height || width > std::numeric_limits<Int>::max() / 4 ||
			height > pixels.max_size() / (width * 4))
			return FALSE;
		m_width = m_textureWidth = width;
		m_height = m_textureHeight = height;
		m_pitch = width * 4;
		pixels.resize(static_cast<std::size_t>(m_pitch) * height);
		return TRUE;
	}

	void free() override { pixels.clear(); }
	void* lock() override { return pixels.data(); }
	void unlock() override {}
	Bool valid() override { return !pixels.empty(); }

	void measureAlpha(unsigned& minimum, unsigned& maximum) const
	{
		for (std::size_t i = 3; i < pixels.size(); i += 4)
		{
			minimum = std::min(minimum, static_cast<unsigned>(pixels[i]));
			maximum = std::max(maximum, static_cast<unsigned>(pixels[i]));
		}
	}

private:
	std::vector<UnsignedByte> pixels;
};

void traceVideoAsset(const RetailTestCommands::Context& ctx)
{
	const char* name = ctx.argument;
	VideoStreamInterface* stream = TheVideoPlayer ? TheVideoPlayer->open(name) : nullptr;
	if (!stream)
	{
		std::fprintf(stderr, "VIDEO PROBE %s unavailable\n", name);
		return;
	}

	CaptureBuffer buffer;
	unsigned minimum = 255;
	unsigned maximum = 0;
	unsigned frames = 0;
	if (stream->width() > 0 && stream->height() > 0 && buffer.allocate(stream->width(), stream->height()))
	{
		for (; frames < 32; )
		{
			stream->frameRender(&buffer);
			buffer.measureAlpha(minimum, maximum);
			++frames;
			const Int index = stream->frameIndex();
			stream->frameNext();
			if (stream->frameIndex() <= index)
				break;
		}
	}
	std::fprintf(stderr, "VIDEO PROBE %s decoded %dx%d %u alpha %u %u total %d\n",
		name, stream->width(), stream->height(), frames, minimum, maximum, stream->frameCount());
	stream->close();
}
}
#endif

namespace RetailTestCommands
{
void registerRetailVideoAssetsTestCommands()
{
#if !RTS_GENERALS
	registerAction("video-asset-probe", traceVideoAsset);
#endif
}
}
