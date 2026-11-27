#include "W3DDevice/GameClient/HeightMap.h"

#include "Common/GlobalData.h"
#include "Common/RetailFileLayers.h"
#include "GameClient/GameClient.h"
#include "GameClient/Snow.h"
#include "GameLogic/TerrainLogic.h"
#include "W3DDevice/GameClient/RetailEffectInput.h"
#include "W3DDevice/GameClient/RetailTerrainShadow.h"
#include "W3DDevice/GameClient/TerrainTex.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/texture.h"
#include "d3dx8math.h"

#include <algorithm>
#include <bit>
#include <cmath>
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
	std::unique_ptr<RetailFX::NativePass> shadowPass;
	std::vector<RetailFX::TerrainVertex> base;
	std::vector<RetailFX::TerrainVertex> blend;
	std::vector<RetailFX::TerrainVertex> third;
	std::vector<uint32_t> indices;
	std::vector<uint32_t> blendIndices;
	std::vector<uint32_t> thirdIndices;
	bool drew = false;
	bool drewShadow = false;
	bool failed = false;
};

bool HeightMapRenderObjClass::drawRetailTerrain(CameraClass &camera, bool &authoredShadow)
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
		{
			m_retailTerrainRuntime->pass = RetailFX::NativePass::makeTerrain(device, *effect, DX8Wrapper::Create_Effect_Shader9);
			m_retailTerrainRuntime->shadowPass = RetailFX::NativePass::makeTerrainWithShadows(device, *effect, DX8Wrapper::Create_Effect_Shader9);
		}
		if (!m_retailTerrainRuntime->pass && !m_retailTerrainRuntime->shadowPass)
			std::fprintf(stderr, "[RetailFX] Could not load Terrain Default\n");
	}
	auto &runtime = *m_retailTerrainRuntime;
	if (!runtime.pass && !runtime.shadowPass)
		return false;

	auto *baseTexture = RetailFX::nativeTexture(m_stageZeroTexture);
	auto *cloudTexture = RetailFX::nativeTexture(peekCloudTexture());
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
	RGBColor lightScale = TheSnowManager ? TheSnowManager->getRetailCloudyLightScale() : RGBColor{1.0f, 1.0f, 1.0f};
#if RTS_ZEROHOUR
	if (TheGameClient)
		lightScale *= TheGameClient->getScriptWorldLight(TheGameClient->getFrame());
#endif
	const bool scaleLight = lightScale.red < 1.0f || lightScale.green < 1.0f || lightScale.blue < 1.0f;
	const bool applyTaint = TheGlobalData->m_taintOn && TheTerrainLogic->hasRetailTaint();
	const RGBColor &taintColor = TheGlobalData->m_taintColor;
	const auto tintTerrainVertex = [&](uint32_t rgba, float wx, float wy) -> uint32_t {
		float redScale = lightScale.red;
		float greenScale = lightScale.green;
		float blueScale = lightScale.blue;
		bool tinted = scaleLight;
		if (applyTaint)
		{
			const float weight = TheTerrainLogic->retailTaintWeight(wx, wy);
			if (weight > 0.0f)
			{
				redScale *= 1.0f + (taintColor.red - 1.0f) * weight;
				greenScale *= 1.0f + (taintColor.green - 1.0f) * weight;
				blueScale *= 1.0f + (taintColor.blue - 1.0f) * weight;
				tinted = true;
			}
		}
		if (!tinted)
			return rgba;
		const auto scaleChannel = [rgba](unsigned shift, float factor) {
			const float scaled = static_cast<float>((rgba >> shift) & 255u) * factor;
			return static_cast<uint32_t>(std::clamp(scaled, 0.0f, 255.0f) + 0.5f) << shift;
		};
		return (rgba & 0xff000000u) | scaleChannel(16, redScale) |
			scaleChannel(8, greenScale) | scaleChannel(0, blueScale);
	};
	RGBColor sun = TheGlobalData->m_terrainDiffuse[0];
	if (scaleLight)
	{
		sun *= lightScale;
	}
	const auto &direction = TheGlobalData->m_terrainLightPos[0];
	Vector3 sunRay(-direction.x, -direction.y, -direction.z);
	sunRay.Normalize();
	const float encodingScale = m_map->usesRetailOverbright() ? 0.5f : 1.0f;
	const auto encodeFill = [&](uint32_t baked, const Coord3D &normal, float sourceScale) {
		const float direct = std::clamp(sunRay.X * normal.x + sunRay.Y * normal.y + sunRay.Z * normal.z, 0.0f, 1.0f);
		const auto channel = [baked, direct, sourceScale, encodingScale](unsigned shift, float intensity) {
			const float original = static_cast<float>((baked >> shift) & 255u) * sourceScale;
			const float fill = original - direct * intensity * encodingScale * 255.0f;
			return static_cast<uint32_t>(std::clamp(fill, 0.0f, 255.0f) + 0.5f) << shift;
		};
		return (baked & 0xff000000u) | channel(16, sun.red) | channel(8, sun.green) | channel(0, sun.blue);
	};
	D3DXMATRIX worldToShadow;
	float shadowTexel = 0.0f;
	void *shadowTexture = nullptr;
	const bool legacyShore = TheGlobalData->m_showSoftWaterEdge && m_numShoreLineTiles > 0;
	bool shadowReady = runtime.shadowPass && TheRetailTerrainShadow && !legacyShore &&
		TheGlobalData->m_use3WayTerrainBlends != 2 &&
		TheRetailTerrainShadow->getAuthoredShadow(shadowTexture, worldToShadow, shadowTexel);
	void *thirdAtlas = nullptr;
	runtime.third.clear();
	runtime.thirdIndices.clear();
	if (shadowReady && TheGlobalData->m_use3WayTerrainBlends && m_numExtraBlendTiles)
	{
		const Int minX = m_map->getDrawOrgX(), minY = m_map->getDrawOrgY();
		const Int maxX = std::min(minX + m_map->getDrawWidth(), m_map->getXExtent()) - 1;
		const Int maxY = std::min(minY + m_map->getDrawHeight(), m_map->getYExtent()) - 1;
		const Int border = m_map->getBorderSizeInline();
		const auto *heights = m_map->getDataPtr();
		for (Int i = 0; i < m_numExtraBlendTiles; ++i)
		{
			const Int x = m_extraBlendTilePositions[i] & 0xffff, y = m_extraBlendTilePositions[i] >> 16;
			if (x < minX || x >= maxX || y < minY || y >= maxY)
				continue;
			Real u[4], v[4];
			UnsignedByte alpha[4];
			Bool flip, cliff;
			if (!m_map->getExtraAlphaUVData(x, y, u, v, alpha, &flip, &cliff))
				continue;
			if (std::none_of(std::begin(alpha), std::end(alpha), [](auto value) { return value != 0; }))
			{
				shadowReady = false;
				break;
			}
			const auto first = static_cast<uint32_t>(runtime.third.size());
			const Int px[] = {x, x + 1, x + 1, x};
			const Int py[] = {y, y, y + 1, y + 1};
			for (Int corner = 0; corner < 4; ++corner)
			{
				const float wx = static_cast<float>(px[corner] - border) * MAP_XY_FACTOR;
				const float wy = static_cast<float>(py[corner] - border) * MAP_XY_FACTOR;
				const float z = m_map->toWorldHeight(heights[px[corner] + py[corner] * m_map->getXExtent()]);
				Coord3D normal{0.0f, 0.0f, 1.0f};
				TheTerrainLogic->getGroundHeight(wx, wy, &normal);
				const uint32_t color = tintTerrainVertex((getStaticDiffuse(px[corner], py[corner]) & 0xffffffu) | (uint32_t(alpha[corner]) << 24), wx, wy);
				runtime.third.push_back({{wx, wy, z}, {normal.x, normal.y, normal.z}, color, {u[corner], v[corner]},
					encodeFill(color, normal, encodingScale)});
			}
			const auto z = [&](int offset) { return runtime.third[first + offset].position[2]; };
			if (flip || (cliff && std::abs(z(0) - z(2)) > std::abs(z(1) - z(3))))
				runtime.thirdIndices.insert(runtime.thirdIndices.end(), {first + 1, first + 3, first, first + 1, first + 2, first + 3});
			else
				pushQuad(runtime.thirdIndices, first);
		}
		if (shadowReady && !runtime.thirdIndices.empty())
		{
			thirdAtlas = RetailFX::nativeTexture(m_stageOneTexture);
			shadowReady = thirdAtlas != nullptr;
		}
	}
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
		else if (name == "ShadowInfo" && shadowReady)
		{
			RetailFX::writeMatrix(floats, worldToShadow);
			if (floats.size() > 4)
				floats[4] = {0.0f, 0.0f, shadowTexel, shadowTexel};
		}
		else if (name == "MainDirectionalLight")
		{
			floats[0] = {sun.red, sun.green, sun.blue, 0.0f};
			if (floats.size() > 1)
				floats[1] = {-direction.x, -direction.y, -direction.z, 0.0f};
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
		if (name == "ShadowMap") return shadowTexture;
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
						const auto color = tintTerrainVertex(static_cast<uint32_t>(input.diffuse), input.x, input.y);
						const auto fill = shadowReady ? encodeFill(color, normal, 1.0f) : color;
						RetailFX::TerrainVertex vertex{{input.x, input.y, input.z}, {normal.x, normal.y, normal.z},
							color | 0xff000000u, {input.u1, input.v1}, fill | 0xff000000u};
						runtime.base.push_back(vertex);
						if (blended)
						{
							vertex.diffuse = color;
							vertex.fill = fill;
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

	authoredShadow = shadowReady && runtime.shadowPass->drawTerrain(device, runtime.base, runtime.indices,
		runtime.blend, runtime.blendIndices, setBinding, getTexture, runtime.third, runtime.thirdIndices, thirdAtlas);
	if (authoredShadow)
		m_numVisibleExtraBlendTiles = static_cast<Int>(runtime.third.size() / 4);
	const bool drawn = authoredShadow || (runtime.pass && runtime.pass->drawTerrain(device, runtime.base, runtime.indices,
		runtime.blend, runtime.blendIndices, setBinding, getTexture));
	auto &reported = authoredShadow ? runtime.drewShadow : runtime.drew;
	if (drawn && !reported)
	{
		std::fprintf(stderr, "[RetailFX] Drew Terrain Default%s\n", authoredShadow ? " with authored shadows" : "");
		reported = true;
	}
	if (!drawn && !runtime.failed)
	{
		std::fprintf(stderr, "[RetailFX] Could not bind or draw Terrain Default\n");
		runtime.failed = true;
	}
	return drawn;
}
