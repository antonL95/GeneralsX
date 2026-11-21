#pragma once

#include "WW3D2/retailfx.h"
#include "WW3D2/retailfxprogram.h"

#include <memory>

namespace RetailFX
{

void installNormalMappedDraw();
void removeNormalMappedDraw();

struct WaterVertex
{
	float x;
	float y;
	float z;
	float terrain[3];
};

struct TerrainVertex
{
	float position[3];
	float normal[3];
	uint32_t diffuse;
	float uv[2];
};

struct NormalMappedVertex
{
	float position[3];
	float normal[3];
	float tangent[3];
	float bitangent[3];
	float uv[2];
};

class NativePass
{
public:
	~NativePass();

	using CreateShader = void *(*)(bool, const uint32_t *);
	static std::unique_ptr<NativePass> makeWater(void *device, const Effect &effect, CreateShader createShader);
	static std::unique_ptr<NativePass> makeTerrain(void *device, const Effect &effect, CreateShader createShader);
	static std::unique_ptr<NativePass> makeNormalMapped(void *device, const Effect &effect, CreateShader createShader);

	bool drawWater(void *device, std::span<const WaterVertex> vertices, std::span<const uint16_t> indices,
		const SetBinding &setBinding, const std::function<void *(std::string_view)> &getTexture, bool additive);
	bool drawTerrain(void *device, std::span<const TerrainVertex> base, std::span<const uint32_t> baseIndices,
		std::span<const TerrainVertex> blend, std::span<const uint32_t> blendIndices,
		const SetBinding &setBinding, const std::function<void *(std::string_view)> &getTexture);
	bool drawNormalMapped(void *device, std::span<const NormalMappedVertex> vertices, std::span<const uint32_t> indices,
		const SetBinding &setBinding, const std::function<void *(std::string_view)> &getTexture, bool alphaTest, bool twoSided);

private:
	enum class Kind { Water, Terrain, NormalMapped };
	struct Data;
	explicit NativePass(std::unique_ptr<Data> data);
	static std::unique_ptr<NativePass> make(void *device, const Effect &effect, CreateShader createShader, Kind kind);
	std::unique_ptr<Data> m_data;
};

}
