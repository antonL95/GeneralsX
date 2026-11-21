#include "W3DDevice/GameClient/HeightMap.h"

#include "Common/GlobalData.h"
#include "Common/RetailFileLayers.h"
#include "GameLogic/TerrainLogic.h"
#include "W3DDevice/GameClient/RetailEffectInput.h"
#include "W3DDevice/GameClient/TerrainTex.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/texture.h"
#include "d3dx8math.h"

#include <algorithm>
#include <bit>
#include <cstdio>
#include <memory>

namespace
{

void pushQuad(std::vector<uint32_t> &indices, uint32_t base)
{
	indices.insert(indices.end(), {base, base + 2, base + 3, base, base + 1, base + 2});
}

}

class RetailTerrainRuntime
{
public:
	std::unique_ptr<RetailFX::NativePass> pass;
	std::vector<RetailFX::TerrainVertex> base;
	std::vector<RetailFX::TerrainVertex> blend;
	std::vector<uint32_t> indices;
	std::vector<uint32_t> blendIndices;
	bool drew = false;
	bool failed = false;
};

bool HeightMapRenderObjClass::drawRetailTerrain(CameraClass &camera)
{
	if (!RetailFileLayers::enabled() || !m_map || !m_vertexBufferBackup || !TheTerrainLogic)
		return false;
	auto *device = DX8Wrapper::Get_Effect_Device9();
	if (!device)
		return false;

	if (!m_retailTerrainRuntime)
	{
		m_retailTerrainRuntime = std::make_shared<RetailTerrainRuntime>();
		if (const auto effect = RetailFX::loadEffectFile("shaders/compiled/terrain.fxo"))
			m_retailTerrainRuntime->pass = RetailFX::NativePass::makeTerrain(device, *effect, DX8Wrapper::Create_Effect_Shader9);
		if (!m_retailTerrainRuntime->pass)
			std::fprintf(stderr, "[RetailFX] Could not load Terrain Default\n");
	}
	auto &runtime = *m_retailTerrainRuntime;
	if (!runtime.pass)
		return false;

	auto *baseTexture = RetailFX::nativeTexture(m_stageZeroTexture);
	auto *cloudTexture = RetailFX::nativeTexture(m_stageTwoTexture);
	auto *macroTexture = RetailFX::nativeTexture(m_stageThreeTexture);
	if (!baseTexture || !cloudTexture || !macroTexture)
		return false;

	auto *device8 = DX8Wrapper::_Get_D3D_Device8();
	D3DXMATRIX world, view, projection, inverseView, worldView, inverseWorldView, worldViewProjection;
	if (FAILED(device8->GetTransform(D3DTS_WORLD, &world)) || FAILED(device8->GetTransform(D3DTS_VIEW, &view)) ||
		FAILED(device8->GetTransform(D3DTS_PROJECTION, &projection)) || !D3DXMatrixInverse(&inverseView, nullptr, &view))
		return false;
	D3DXMatrixMultiply(&worldView, &world, &view);
	D3DXMatrixMultiply(&worldViewProjection, &worldView, &projection);
	if (!D3DXMatrixInverse(&inverseWorldView, nullptr, &worldView))
		return false;

	const auto fog = RetailFX::fogBindings(device8);
	const auto cloud = W3DShaderManager::getRetailCloudUV();
	const auto setBinding = [&](const RetailFX::Binding &binding, std::span<RetailFX::Register> floats, std::span<int> bools) {
		if (!bools.empty() || floats.empty())
			return;
		const auto &name = binding.name;
		if (name == "World")
			RetailFX::writeMatrix(floats, world);
		else if (name == "WorldI")
			RetailFX::writeMatrix(floats, world);
		else if (name == "WorldViewI")
			RetailFX::writeMatrix(floats, inverseWorldView);
		else if (name == "WorldViewProjection")
			RetailFX::writeMatrix(floats, worldViewProjection);
		else if (name == "ViewI")
			RetailFX::writeMatrix(floats, inverseView);
		else if (name == "Cloud")
			floats[0] = cloud;
		else if (name == "MainDirectionalLight")
		{
			const auto &sun = TheGlobalData->m_terrainDiffuse[0];
			const auto &direction = TheGlobalData->m_terrainLightPos[0];
			floats[0] = {sun.red, sun.green, sun.blue, 0.0f};
			if (floats.size() > 1)
				floats[1] = {direction.x, direction.y, direction.z, 0.0f};
		}
		else if (name == "Fog")
			std::copy_n(fog.begin(), std::min(fog.size(), floats.size()), floats.begin());
		else if (name == "MapBorderWidth")
			floats[0][0] = m_map->getBorderSizeInline() * MAP_XY_FACTOR;
		else if (name == "MapSize")
		{
			const Int border = m_map->getBorderSizeInline() * 2;
			floats[0] = {static_cast<float>(m_map->getXExtent() - border) * MAP_XY_FACTOR,
				static_cast<float>(m_map->getYExtent() - border) * MAP_XY_FACTOR, 0.0f, 0.0f};
		}
		else if (name == "IsMacroTextureStrechedToMapSize")
			floats[0][0] = m_map->isRetailMacroStretched() ? 1.0f : 0.0f;
		else if (name == "IsRenderingOverbright")
			floats[0][0] = m_map->usesRetailOverbright() ? 1.0f : 0.0f;
	};
	const auto getTexture = [&](std::string_view name) -> void * {
		if (name == "BaseTexture") return baseTexture;
		if (name == "CloudTexture") return cloudTexture;
		if (name == "MacroTexture") return macroTexture;
		return nullptr;
	};

	runtime.base.clear();
	runtime.blend.clear();
	runtime.indices.clear();
	runtime.blendIndices.clear();
	for (Int y = 0; y < m_numVBTilesY; ++y)
	{
		for (Int x = 0; x < m_numVBTilesX; ++x)
		{
			const auto *source = getVertexBufferBackup(x, y);
			const Int rows = y + 1 == m_numVBTilesY && m_numBlockRowsInLastVB ? m_numBlockRowsInLastVB : VERTEX_BUFFER_TILE_LENGTH;
			const Int columns = x + 1 == m_numVBTilesX && m_numBlockColumnsInLastVB ? m_numBlockColumnsInLastVB : VERTEX_BUFFER_TILE_LENGTH;
			for (Int row = 0; row < rows; ++row)
			{
				for (Int col = 0; col < columns; ++col)
				{
					const auto *quad = source + (row * VERTEX_BUFFER_TILE_LENGTH + col) * 4;
					const bool blended = std::any_of(quad, quad + 4, [](const auto &vertex) { return vertex.diffuse & 0xff000000u; });
					pushQuad(runtime.indices, static_cast<uint32_t>(runtime.base.size()));
					if (blended)
						pushQuad(runtime.blendIndices, static_cast<uint32_t>(runtime.blend.size()));
					for (Int i = 0; i < 4; ++i)
					{
						const auto &input = quad[i];
						Coord3D normal{0.0f, 0.0f, 1.0f};
						TheTerrainLogic->getGroundHeight(input.x, input.y, &normal);
						RetailFX::TerrainVertex vertex{{input.x, input.y, input.z},
							{normal.x, normal.y, normal.z}, static_cast<uint32_t>(input.diffuse) | 0xff000000u, {input.u1, input.v1}};
						runtime.base.push_back(vertex);
						if (blended)
						{
							vertex.diffuse = static_cast<uint32_t>(input.diffuse);
							vertex.uv[0] = input.u2;
							vertex.uv[1] = input.v2;
							runtime.blend.push_back(vertex);
						}
					}
				}
			}
		}
	}
	const DWORD colorWrite = DX8Wrapper::Get_DX8_Render_State(D3DRS_COLORWRITEENABLE);
	const DWORD zWrite = DX8Wrapper::Get_DX8_Render_State(D3DRS_ZWRITEENABLE);
	const DWORD zFunc = DX8Wrapper::Get_DX8_Render_State(D3DRS_ZFUNC);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE, 0);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE, TRUE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	renderTerrainPass(&camera);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC, zFunc);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE, zWrite);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE, colorWrite);

	const bool drawn = runtime.pass->drawTerrain(device, runtime.base, runtime.indices, runtime.blend,
		runtime.blendIndices, setBinding, getTexture);
	if (drawn && !runtime.drew)
	{
		std::fprintf(stderr, "[RetailFX] Drew Terrain Default\n");
		runtime.drew = true;
	}
	if (!drawn && !runtime.failed)
	{
		std::fprintf(stderr, "[RetailFX] Could not bind or draw Terrain Default\n");
		runtime.failed = true;
	}
	return drawn;
}
