#include "W3DDevice/GameClient/W3DWater.h"
#include "W3DRetailWaterReflection.h"

#include "Common/GlobalData.h"
#include "Common/GameLOD.h"
#include "Common/RetailFileLayers.h"
#include "GameClient/Water.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/TerrainLogic.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/RetailEffectInput.h"
#include "W3DDevice/GameClient/W3DShroud.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
#include "WW3D2/assetmgr.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/mesh.h"
#include "WW3D2/meshmdl.h"
#include "WW3D2/texture.h"
#include "WW3D2/w3dfxmaterial.h"
#include "d3dx8math.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <locale>
#include <map>
#include <memory>
#include <sstream>

namespace
{

template<class T>
struct Release
{
	void operator()(T *value) const { value->Release_Ref(); }
};

template<class T>
using Reference = std::unique_ptr<T, Release<T>>;

constexpr std::array<std::string_view, 5> pcaTextureNames{
	"WaterPCATexture1", "WaterPCATexture2", "WaterPCATexture3", "PCANoiseTexture", "PCAFrothTexture"
};

constexpr unsigned pcaBasisCount = 12;
constexpr float noShoreWaveCenter = 100000.0f;
constexpr float shoreWaveDepth = 100.0f;

using PcaFrame = std::array<RetailFX::Register, 10>;

struct PcaWater
{
	float speed = 0.0f;
	std::vector<PcaFrame> frames;
	std::array<Reference<TextureClass>, pcaTextureNames.size()> textures;

	PcaFrame sample(UnsignedInt elapsedFrames) const
	{
		const auto phase = std::fmod(static_cast<double>(elapsedFrames) * speed, static_cast<double>(frames.size()));
		const auto first = static_cast<size_t>(phase);
		const auto next = (first + 1) % frames.size();
		const auto fraction = static_cast<float>(phase - static_cast<double>(first));
		PcaFrame result{};
		for (size_t row = 0; row < result.size(); ++row)
			for (size_t channel = 0; channel < result[row].size(); ++channel)
				result[row][channel] = std::lerp(frames[first][row][channel], frames[next][row][channel], fraction);
		return result;
	}
};

struct WaterMaterial
{
	W3dFx::Material material;
	Reference<TextureClass> bump;
	Reference<TextureClass> environment;
	Reference<TextureClass> wave;
	std::optional<PcaWater> pca;
};

const W3dFx::Material *findMaterial(RenderObjClass &object)
{
	if (object.Class_ID() == RenderObjClass::CLASSID_MESH)
	{
		const auto *fx = static_cast<MeshClass &>(object).Peek_Model()->Peek_Fx_Materials();
		if (fx)
			for (const auto &material : fx->materials)
				if (material.effect == "watershader.fx")
					return &material;
	}
	for (int i = 0; i < object.Get_Num_Sub_Objects(); ++i)
	{
		const Reference<RenderObjClass> child(object.Get_Sub_Object(i));
		if (child)
			if (const auto *material = findMaterial(*child))
				return material;
	}
	return nullptr;
}

Reference<TextureClass> loadTexture(const std::string &name)
{
	if (name.empty() || name == "None")
		return {};
	return Reference<TextureClass>(WW3DAssetManager::Get_Instance()->Get_Texture(name.c_str()));
}

std::optional<PcaWater> loadPcaWater(const W3dFx::Material &material)
{
	auto name = material.get<std::string>(pcaTextureNames[0], {});
	const auto extension = name.find_last_of('.');
	if (extension == std::string::npos || !extension || name.find_first_of("/\\") != std::string::npos)
		return {};
	name.replace(extension, std::string::npos, ".basis");
	const auto path = "art/textures/" + name;
	auto file = std::unique_ptr<File, void (*)(File *)>(TheFileSystem->openFile(path.c_str(), File::READ | File::BINARY),
		[](File *value) { value->close(); });
	if (!file)
		return {};
	const Int size = file->size();
	if (size <= 0 || size > 1024 * 1024)
		return {};
	std::string content(static_cast<size_t>(size), '\0');
	if (file->read(content.data(), size) != size)
		return {};

	PcaWater result;
	std::istringstream input(std::move(content));
	input.imbue(std::locale::classic());
	std::string field;
	size_t frameCount = 0;
	if (!(input >> field >> frameCount) || field != "NumFrames:" || !frameCount ||
		frameCount > static_cast<size_t>(size) / ((pcaBasisCount + 1) * 3) ||
		!(input >> field >> result.speed) || field != "Speed:" ||
		!std::isfinite(result.speed) || result.speed < 0.0f)
		return {};

	result.frames.resize(frameCount);
	const auto readSamples = [&](std::string_view label, auto store) {
		if (!(input >> field) || field != label)
			return false;
		for (const auto axis : {0u, 2u, 1u})
			for (auto &frame : result.frames)
			{
				float value;
				if (!(input >> value) || !std::isfinite(value))
					return false;
				store(frame, axis, value / 255.0f);
			}
		return true;
	};
	if (!readSamples("Mean:", [](PcaFrame &frame, unsigned axis, float value) { frame[0][axis] = value; }))
		return {};
	for (unsigned basis = 0; basis < pcaBasisCount; ++basis)
	{
		if (!readSamples("Basis" + std::to_string(basis) + ':', [basis](PcaFrame &frame, unsigned axis, float value) {
			frame[1 + 3 * (basis / 4) + axis][basis % 4] = value;
		}))
			return {};
	}
	if (input >> field)
		return {};
	for (size_t i = 0; i < result.textures.size(); ++i)
	{
		result.textures[i] = loadTexture(material.get<std::string>(pcaTextureNames[i], {}));
		if (!result.textures[i])
			return {};
	}
	return result;
}

bool useUltraWater()
{
#if RTS_GENERALS
	return false;
#else
	const auto *settings = TheGameLODManager ? TheGameLODManager->getStaticRenderSettings() : nullptr;
	return settings && settings->m_waterQuality == 4;
#endif
}

std::optional<WaterMaterial> loadMaterial(const PolygonTrigger::StandingWaterSettings &settings)
{
	std::string name(settings.fxShader.str());
	if (const auto extension = name.find_last_of('.'); extension != std::string::npos)
		name.resize(extension);
	if (name.empty())
		return {};
	const Reference<RenderObjClass> object(WW3DAssetManager::Get_Instance()->Create_Render_Obj(name.c_str()));
	const auto *material = object ? findMaterial(*object) : nullptr;
	if (!material)
		return {};
	WaterMaterial result;
	result.material = *material;
	result.bump = loadTexture(material->get<std::string>("WaterBumpTexture", settings.bumpTexture.str()));
	result.environment = loadTexture(material->get<std::string>("WaterEnvTexture", settings.skyTexture.str()));
	result.wave = loadTexture(material->get<std::string>("WaveTexture", {}));
	if (material->technique == 5)
		result.pca = loadPcaWater(*material);
	if (!result.bump || !result.environment || !result.wave)
		return {};
	return result;
}

bool makeGeometry(const PolygonTrigger &polygon, std::vector<RetailFX::WaterVertex> &vertices,
	std::vector<uint16_t> &indices, bool pca)
{
	const float height = static_cast<float>(polygon.getPoint(0)->z);
	for (const auto &patch : polygon.getStandingWater()->patches)
	{
		const auto distance = [&](unsigned a, unsigned b) {
			return std::hypot(patch[a].x - patch[b].x, patch[a].y - patch[b].y);
		};
		const auto subdivisions = [](float size) {
			return static_cast<unsigned>(std::clamp(size / 20.0f, 1.0f, 192.0f));
		};
		const unsigned cols = subdivisions(std::max(distance(0, 1), distance(3, 2)));
		const unsigned rows = subdivisions(std::abs(patch[3].y - patch[0].y));
		const size_t base = vertices.size();
		if (base + (rows + 1) * (cols + 1) > 65536)
			return false;
		for (unsigned row = 0; row <= rows; ++row)
		{
			const float v = static_cast<float>(row) / static_cast<float>(rows);
			for (unsigned col = 0; col <= cols; ++col)
			{
				const float u = static_cast<float>(col) / static_cast<float>(cols);
				const float x = std::lerp(std::lerp(patch[0].x, patch[1].x, u), std::lerp(patch[3].x, patch[2].x, u), v);
				const float y = std::lerp(std::lerp(patch[0].y, patch[1].y, u), std::lerp(patch[3].y, patch[2].y, u), v);
				const auto ground = TheTerrainLogic->getGroundHeight(x, y);
				if (pca)
					vertices.push_back({x, y, height, {noShoreWaveCenter, noShoreWaveCenter, ground},
						{1.0f, 0.0f}, {shoreWaveDepth, 0.0f}});
				else
					vertices.push_back({x, y, height, {0.0f, 0.0f, ground}});
				if (row < rows && col < cols)
				{
					const auto a = static_cast<uint16_t>(base + row * (cols + 1) + col);
					const auto b = static_cast<uint16_t>(a + 1);
					const auto c = static_cast<uint16_t>(a + cols + 1);
					const auto d = static_cast<uint16_t>(c + 1);
					indices.insert(indices.end(), {a, d, c, a, b, d});
				}
			}
		}
	}
	return !indices.empty();
}

}

class RetailStandingWater
{
public:
	std::unique_ptr<RetailFX::NativePass> pass;
	std::unique_ptr<RetailFX::NativePass> fullPass;
	std::unique_ptr<RetailFX::NativePass> pcaPass;
	RetailFX::WaterReflectionImage reflection;
	std::optional<unsigned> reflectionRevision;
	bool reflectionReady = false;
	std::map<std::array<std::string, 3>, std::optional<WaterMaterial>> materials;
	bool drew = false;
	bool drewFull = false;
	bool drewPca = false;
	bool failed = false;
};

bool WaterRenderObjClass::drawRetailStandingWater(const PolygonTrigger &polygon)
{
	if (!RetailFileLayers::enabled() || !TheTerrainLogic || !TheTerrainRenderObject || !TheGameLogic)
		return false;
	auto *device = DX8Wrapper::Get_Effect_Device9();
	if (!device)
		return false;

	if (!m_retailStandingWater)
	{
		m_retailStandingWater = std::make_shared<RetailStandingWater>();
		if (const auto effect = RetailFX::loadEffectFile("shaders/compiled/watershader.fxo"))
		{
			m_retailStandingWater->pass = RetailFX::NativePass::makeWater(device, *effect, DX8Wrapper::Create_Effect_Shader9);
			m_retailStandingWater->fullPass = RetailFX::NativePass::makeFullWater(device, *effect, DX8Wrapper::Create_Effect_Shader9);
			m_retailStandingWater->pcaPass = RetailFX::NativePass::makePcaWater(device, *effect, DX8Wrapper::Create_Effect_Shader9);
		}
		if (!m_retailStandingWater->pass && !m_retailStandingWater->fullPass && !m_retailStandingWater->pcaPass)
			std::fprintf(stderr, "[RetailFX] Could not load WaterShader passes\n");
	}

	auto &runtime = *m_retailStandingWater;
	if (!runtime.pass && !runtime.fullPass && !runtime.pcaPass)
		return false;
	const auto &settings = *polygon.getStandingWater();
	auto [entry, inserted] = runtime.materials.try_emplace(
		std::array<std::string, 3>{settings.fxShader.str(), settings.bumpTexture.str(), settings.skyTexture.str()});
	if (inserted)
	{
		entry->second = loadMaterial(settings);
		if (!entry->second)
			std::fprintf(stderr, "[RetailFX] Could not load standing water material %s\n", settings.fxShader.str());
	}
	if (!entry->second)
		return false;
	auto &water = *entry->second;
	const bool usePca = runtime.pcaPass && water.pca && useUltraWater();
	const bool useFull = runtime.fullPass && water.material.technique == 1;
	void *reflection = nullptr;
	if ((useFull || usePca) && m_retailReflectionSourceValid &&
		m_reflectionLevel == static_cast<Real>(polygon.getPoint(0)->z) && TheGlobalData && TheGlobalData->isWaterReflections())
	{
		if (runtime.reflectionRevision != m_reflectionRevision)
		{
			runtime.reflectionRevision = m_reflectionRevision;
			runtime.reflectionReady = runtime.reflection.prepare(device, RetailFX::nativeTexture(m_pReflectionTexture),
				{m_reflectionAreaMin.X, m_reflectionAreaMin.Y, m_reflectionAreaMax.X, m_reflectionAreaMax.Y});
		}
		if (runtime.reflectionReady)
			reflection = runtime.reflection.texture();
	}

	auto *shroud = TheTerrainRenderObject->getShroud();
	if (!shroud || !shroud->getShroudTexture() || shroud->getTextureWidth() <= 0 || shroud->getTextureHeight() <= 0 ||
		shroud->getCellWidth() <= 0.0f || shroud->getCellHeight() <= 0.0f)
		return false;
	const float scaleU = 1.0f / (shroud->getCellWidth() * static_cast<float>(shroud->getTextureWidth()));
	const float scaleV = 1.0f / (shroud->getCellHeight() * static_cast<float>(shroud->getTextureHeight()));
	const RetailFX::Register shroudUV{scaleU, scaleV,
		(shroud->getCellWidth() - shroud->getDrawOriginX()) * scaleU,
		(shroud->getCellHeight() - shroud->getDrawOriginY()) * scaleV};

	D3DXMATRIX view, projection, inverse, viewProjection, world;
	if (FAILED(m_pDev->GetTransform(D3DTS_VIEW, &view)) || FAILED(m_pDev->GetTransform(D3DTS_PROJECTION, &projection)) ||
		!D3DXMatrixInverse(&inverse, nullptr, &view))
		return false;
	D3DXMatrixMultiply(&viewProjection, &view, &projection);
	D3DXMatrixIdentity(&world);

	const auto fog = RetailFX::fogBindings(m_pDev);

	const auto *map = TheTerrainRenderObject->getMap();
	const float authoredDepth = map ? map->getRetailWaterMaxAlphaDepth() : -1.0f;
	const float authoredAlpha = map ? map->getRetailDeepWaterAlpha() : -1.0f;
	const float maxDepth = authoredDepth >= 0.0f ? authoredDepth :
		TheWaterTransparency ? TheWaterTransparency->m_transparentWaterDepth : 0.0f;
	const float time = static_cast<float>(TheGameLogic->getFrame()) / static_cast<float>(LOGICFRAMES_PER_SECOND);
	const auto pcaFrame = usePca && reflection
		? std::optional(water.pca->sample(TheGameLogic->getFrame())) : std::nullopt;
	const auto setBinding = [&](const RetailFX::Binding &binding, std::span<RetailFX::Register> values, std::span<int> booleans) {
		const auto name = std::string_view(binding.name);
		if (!booleans.empty())
		{
			if (name == "UseMySpecularDirection")
				booleans[0] = water.material.get<bool>(name, false);
			return;
		}
		if (values.empty())
			return;
		if (name == "WorldViewProjection")
			RetailFX::writeMatrix(values, viewProjection);
		else if (name == "World")
			RetailFX::writeMatrix(values, world);
		else if (name == "View")
			RetailFX::writeMatrix(values, view);
		else if (name == "Projection")
			RetailFX::writeMatrix(values, projection);
		else if (name == "ViewI")
			RetailFX::writeMatrix(values, inverse);
		else if (name == "PCA" && pcaFrame)
			std::copy_n(pcaFrame->begin(), std::min(pcaFrame->size(), values.size()), values.begin());
		else if (name == "Shroud")
			values[0] = shroudUV;
		else if (name == "Fog")
		{
			std::copy_n(fog.begin(), std::min(fog.size(), values.size()), values.begin());
		}
		else if (name == "DirectionalLight")
		{
			const auto count = std::min(values.size() / 2, static_cast<size_t>(TheGlobalData->m_numGlobalLights));
			for (size_t i = 0; i < count; ++i)
			{
				const auto &color = TheGlobalData->m_terrainDiffuse[i];
				const auto &direction = TheGlobalData->m_terrainLightPos[i];
				values[i * 2] = {color.red, color.green, color.blue, 0.0f};
				values[i * 2 + 1] = {-direction.x, -direction.y, -direction.z, 0.0f};
			}
		}
		else if (name == "Time")
			values[0][0] = time;
		else if (name == "MaxAlphaDepth")
			values[0][0] = maxDepth > 0.0f ? maxDepth : 1.0f;
		else if (name == "DeepWaterAlpha" && authoredAlpha >= 0.0f)
			values[0][0] = authoredAlpha;
		else if (name == "BlendUsingTerrainAlpha")
			values[0][0] = TheGlobalData->m_showSoftWaterEdge && maxDepth > 0.0f ? 1.0f : 0.0f;
		else if (const auto property = water.material.properties.find(name); property != water.material.properties.end())
		{
			if (const auto *scalar = std::get_if<float>(&property->second))
				values[0][0] = *scalar;
			else if (const auto *v = std::get_if<W3dFx::Float4>(&property->second))
				values[0] = *v;
			else if (const auto *v = std::get_if<W3dFx::Float3>(&property->second))
				std::copy(v->begin(), v->end(), values[0].begin());
			else if (const auto *v = std::get_if<W3dFx::Float2>(&property->second))
				std::copy(v->begin(), v->end(), values[0].begin());
		}
	};
	const auto getTexture = [&](std::string_view name) -> void * {
		if (name == "WaterBumpTexture")
			return RetailFX::nativeTexture(water.bump.get());
		if (name == "WaterEnvTexture")
			return RetailFX::nativeTexture(water.environment.get());
		if (name == "WaterReflectionTexture")
			return reflection;
		if (name == "WaveTexture")
			return RetailFX::nativeTexture(water.wave.get());
		if (name == "ShroudTexture")
			return RetailFX::nativeTexture(shroud->getShroudTexture());
		if (pcaFrame)
		{
			const auto property = std::find(pcaTextureNames.begin(), pcaTextureNames.end(), name);
			if (property != pcaTextureNames.end())
				return RetailFX::nativeTexture(water.pca->textures[property - pcaTextureNames.begin()].get());
		}
		return nullptr;
	};

	std::vector<RetailFX::WaterVertex> vertices;
	std::vector<uint16_t> indices;
	if (!makeGeometry(polygon, vertices, indices, pcaFrame.has_value()))
		return false;
	const bool pca = pcaFrame && runtime.pcaPass->drawWater(device, vertices, indices, setBinding, getTexture, settings.additive);
	if (pcaFrame && !pca)
		for (auto &vertex : vertices)
			vertex.terrain[0] = vertex.terrain[1] = 0.0f;
	const bool full = useFull && reflection && runtime.fullPass->drawWater(device, vertices, indices, setBinding, getTexture, settings.additive);
	const bool drawn = pca || full || (runtime.pass && runtime.pass->drawWater(device, vertices, indices, setBinding, getTexture, settings.additive));
	auto &reported = pca ? runtime.drewPca : full ? runtime.drewFull : runtime.drew;
	if (drawn && !reported)
	{
		std::fprintf(stderr, "[RetailFX] Drew WaterShader %s (%s)\n",
			pca ? "PCA_U" : full ? "BumpSpecular" : "BumpSpecular_M", settings.fxShader.str());
		if (std::getenv("GENERALSX_TRACE_RETAIL_EFFECTS"))
		{
			std::fprintf(stderr, "[RetailFX] Water reflection technique=%u type=%d source=%d enabled=%d plane=%g area=%d prepared=%d available=%d\n",
				static_cast<unsigned>(water.material.technique), static_cast<int>(m_waterType), m_retailReflectionSourceValid,
				TheGlobalData && TheGlobalData->isWaterReflections(), m_reflectionLevel, polygon.getPoint(0)->z,
				runtime.reflectionReady, static_cast<bool>(runtime.fullPass));
			const auto color = water.material.get<W3dFx::Float4>("MaterialColorDiffuse", {0.15f, 0.3f, 0.35f, 0.5f});
			const auto &light = TheGlobalData->m_terrainLightPos[0];
			std::fprintf(stderr,
				"[RetailFX] Water bump=%s environment=%s wave=%s diffuse=(%g,%g,%g,%g) legacyDiffuse=%08x "
				"maxAlphaDepth=%g deepAlpha=%g softEdge=%d lightToSurface=(%g,%g,%g)\n",
				water.bump->Get_Texture_Name().str(), water.environment->Get_Texture_Name().str(),
				water.wave->Get_Texture_Name().str(), color[0], color[1], color[2], color[3],
				static_cast<unsigned>(m_settings[m_tod].waterDiffuse), maxDepth, authoredAlpha,
				TheGlobalData->m_showSoftWaterEdge && maxDepth > 0.0f, light.x, light.y, light.z);
		}
		reported = true;
	}
	if (!drawn && !runtime.failed)
	{
		std::fprintf(stderr, "[RetailFX] Could not bind or draw WaterShader\n");
		runtime.failed = true;
	}
	return drawn;
}
