#pragma once

#include "W3DDevice/GameClient/RetailEffectDraw.h"

namespace RetailFX
{

struct GpuParticleVertex
{
	Register position;
	Register velocity;
	std::array<float, 2> seedCorner;
};

class GpuParticlePass
{
public:
	static std::unique_ptr<GpuParticlePass> make(void *device, const Effect &effect, NativePass::CreateShader createShader);
	~GpuParticlePass();

	bool draw(void *device, void *atlas, std::span<const GpuParticleVertex> vertices,
		std::span<const uint16_t> indices, const SetBinding &setBinding);

private:
	struct Data;
	explicit GpuParticlePass(std::unique_ptr<Data> data);
	std::unique_ptr<Data> m_data;
};

}
