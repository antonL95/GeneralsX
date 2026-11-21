#pragma once

#include "WW3D2/retailfx.h"
#include "WW3D2/retailfxprogram.h"

#include <memory>

namespace RetailFX
{

struct WaterVertex
{
	float x;
	float y;
	float z;
	float terrain[3];
};

class NativePass
{
public:
	~NativePass();

	using CreateShader = void *(*)(bool, const uint32_t *);
	static std::unique_ptr<NativePass> makeWater(void *device, const Effect &effect, CreateShader createShader);

	bool drawWater(void *device, std::span<const WaterVertex> vertices, std::span<const uint16_t> indices,
		const SetBinding &setBinding, const std::function<void *(std::string_view)> &getTexture, bool additive);

private:
	struct Data;
	explicit NativePass(std::unique_ptr<Data> data);
	std::unique_ptr<Data> m_data;
};

}
