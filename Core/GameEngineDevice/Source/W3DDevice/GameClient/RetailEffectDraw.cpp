#include "W3DDevice/GameClient/RetailEffectDraw.h"

#if defined(__linux__) || defined(__APPLE__)
#include <d3d9.h>
#endif

#include <algorithm>
#include <bit>
#include <cstdio>
#include <utility>

namespace RetailFX
{

#if defined(__linux__) || defined(__APPLE__)
namespace
{

template<typename T>
struct Release
{
	void operator()(T *object) const { object->Release(); }
};

template<typename T>
using Com = std::unique_ptr<T, Release<T>>;

struct Sampler
{
	unsigned slot;
	std::string texture;
	std::vector<std::pair<D3DSAMPLERSTATETYPE, DWORD>> states;
	bool white = false;
};

std::optional<D3DRENDERSTATETYPE> renderState(uint32_t operation)
{
	switch (operation)
	{
		case 0: return D3DRS_ZENABLE;
		case 3: return D3DRS_ZWRITEENABLE;
		case 4: return D3DRS_ALPHATESTENABLE;
		case 6: return D3DRS_SRCBLEND;
		case 7: return D3DRS_DESTBLEND;
		case 8: return D3DRS_CULLMODE;
		case 9: return D3DRS_ZFUNC;
		case 10: return D3DRS_ALPHAREF;
		case 11: return D3DRS_ALPHAFUNC;
		case 13: return D3DRS_ALPHABLENDENABLE;
		case 14: return D3DRS_FOGENABLE;
		case 73: return D3DRS_COLORWRITEENABLE;
		default: return {};
	}
}

std::optional<Sampler> readSampler(const Effect &effect, const Binding &binding)
{
	const auto parameter = std::find_if(effect.parameters.begin(), effect.parameters.end(),
		[&](const auto &item) { return item.value.name == binding.name; });
	if (parameter == effect.parameters.end() || binding.count != 1)
		return {};
	Sampler sampler{binding.first, {}, {}};
	for (const auto &state : parameter->value.samplerStates)
	{
		if (state.operation == 164 && state.resourceUsage == ResourceUsage::Parameter)
			sampler.texture = state.parameterReference;
		else if (state.operation >= 165 && state.operation <= 175 && !state.resourceUsage && state.value.words.size() == 1)
			sampler.states.emplace_back(static_cast<D3DSAMPLERSTATETYPE>(state.operation - 164), state.value.words[0]);
		else
			return {};
	}
	if (sampler.texture.empty())
		return {};
	return sampler;
}

}

struct NativePass::Data
{
	Com<IDirect3DVertexShader9> vertex;
	Com<IDirect3DPixelShader9> pixel;
	Com<IDirect3DVertexDeclaration9> declaration;
	Com<IDirect3DTexture9> white;
	Com<IDirect3DStateBlock9> meshState;
	std::vector<std::pair<D3DRENDERSTATETYPE, DWORD>> states;
	std::vector<Sampler> samplers;
	Program vs;
	Program ps;
	bool useCategoryDepthAndCull = false;

	bool draw(IDirect3DDevice9 *device, const SetBinding &setBinding,
		const std::function<void *(std::string_view)> &getTexture,
		const std::function<bool()> &issueDraw, bool reuseState = false);
};

std::unique_ptr<NativePass> NativePass::makeWater(void *handle, const Effect &effect, CreateShader createShader)
{
	return make(handle, effect, createShader, Kind::Water);
}

std::unique_ptr<NativePass> NativePass::makeFullWater(void *handle, const Effect &effect, CreateShader createShader)
{
	return make(handle, effect, createShader, Kind::FullWater);
}

std::unique_ptr<NativePass> NativePass::makeTerrain(void *handle, const Effect &effect, CreateShader createShader)
{
	return make(handle, effect, createShader, Kind::Terrain);
}

std::unique_ptr<NativePass> NativePass::makeTerrainWithShadows(void *handle, const Effect &effect, CreateShader createShader)
{
	return make(handle, effect, createShader, Kind::ShadowTerrain);
}

std::unique_ptr<NativePass> NativePass::makeNormalMapped(void *handle, const Effect &effect, CreateShader createShader)
{
	return make(handle, effect, createShader, Kind::NormalMapped);
}

std::unique_ptr<NativePass> NativePass::makeDefaultMesh(void *handle, const Effect &effect, CreateShader createShader)
{
	return make(handle, effect, createShader, Kind::DefaultMesh);
}

std::unique_ptr<NativePass> NativePass::makeVegetation(void *handle, const Effect &effect, CreateShader createShader)
{
	return make(handle, effect, createShader, Kind::Vegetation);
}

std::unique_ptr<NativePass> NativePass::make(void *handle, const Effect &effect, CreateShader createShader, Kind kind)
{
	auto *device = static_cast<IDirect3DDevice9 *>(handle);
	const bool shadowTerrain = kind == Kind::ShadowTerrain;
	const bool terrain = kind == Kind::Terrain || shadowTerrain;
	const bool model = kind == Kind::NormalMapped;
	const bool defaultMesh = kind == Kind::DefaultMesh;
	const bool vegetation = kind == Kind::Vegetation;
	const bool fullWater = kind == Kind::FullWater;
	const std::string_view techniqueName = terrain || defaultMesh || vegetation ? "Default" :
		model ? "Default_M" : fullWater ? "BumpSpecular" : "BumpSpecular_M";
	const char *effectName = terrain ? "Terrain" : model ? "NormalMapped" :
		defaultMesh ? "DefaultW3D" : vegetation ? "Vegetation" : "WaterShader";
	const auto technique = std::find_if(effect.techniques.begin(), effect.techniques.end(),
		[&](const auto &item) { return item.name == techniqueName; });
	if (!device || technique == effect.techniques.end() || technique->passes.size() != 1)
		return nullptr;
	auto data = std::make_unique<Data>();
	data->useCategoryDepthAndCull = defaultMesh;
	for (const auto &state : technique->passes[0].states)
	{
		if (state.operation == 146 || state.operation == 147)
		{
			const bool vertex = state.operation == 146;
			const auto type = vertex ? ValueType::VertexShader : ValueType::PixelShader;
			if (state.value.type != type)
				return nullptr;
			const std::vector<uint8_t> *bytes = &state.resource;
			if (state.resourceUsage == ResourceUsage::ArraySelector)
			{
				const std::string_view array = terrain && !vertex ? "PSDefault_Array" :
					model ? (vertex ? "VS_M_Array" : "PS_M_Array") :
					defaultMesh ? (vertex ? "VS_Array" : "PS_Array") : vegetation && !vertex ? "PS_Array" :
					fullWater && !vertex ? "PSBumpSpecular_Array" : "";
				const unsigned variant = terrain ? (shadowTerrain ? 9 : 4) : vertex || vegetation || fullWater ? 0 : defaultMesh ? 37 : 1;
				if (array.empty() || state.parameterReference != array)
					return nullptr;
				const auto parameter = std::find_if(effect.parameters.begin(), effect.parameters.end(),
					[&](const auto &item) { return item.value.name == state.parameterReference; });
				if (parameter == effect.parameters.end() || parameter->value.elements.size() <= variant)
					return nullptr;
				const auto object = parameter->value.elements[variant].object;
				if (!object || *object >= effect.objects.size())
					return nullptr;
				bytes = &effect.objects[*object];
			}
			else if (state.resourceUsage != ResourceUsage::Program)
				return nullptr;
			auto shader = readProgram(*bytes, vertex);
			if (!shader)
			{
				std::fprintf(stderr, "[RetailFX] Unsupported %s %s bytecode\n", effectName, vertex ? "vertex" : "pixel");
				return nullptr;
			}
			(vertex ? data->vs : data->ps) = std::move(*shader);
		}
		else
		{
			const auto native = renderState(state.operation);
			if (state.resourceUsage == ResourceUsage::Program &&
				((terrain && (state.operation == 3 || state.operation == 13)) || (vegetation && state.operation == 13) ||
				 (model && (state.operation == 4 || state.operation == 13 || state.operation == 14 || state.operation == 16)) ||
				 (defaultMesh && (state.operation == 3 || state.operation == 4 || state.operation == 6 ||
					state.operation == 7 || state.operation == 8 || state.operation == 13 ||
					(state.operation >= 165 && state.operation <= 171)))))
				continue;
			if (!native || state.index || state.resourceUsage || state.value.words.size() != 1)
			{
				std::fprintf(stderr, "[RetailFX] Unsupported %s state %u\n", effectName, state.operation);
				return nullptr;
			}
			data->states.emplace_back(*native, state.value.words[0]);
		}
	}
	if (data->vs.tokens.empty() || data->ps.tokens.empty())
		return nullptr;
	for (const auto &binding : data->ps.bindings)
	{
		if (binding.bank != 3)
			continue;
		auto sampler = readSampler(effect, binding);
		if (!sampler)
		{
			std::fprintf(stderr, "[RetailFX] Unsupported %s sampler %s\n", effectName, binding.name.c_str());
			return nullptr;
		}
		sampler->white = terrain && sampler->texture == "ShroudTexture";
		data->samplers.push_back(std::move(*sampler));
	}
	if (terrain || defaultMesh || vegetation)
	{
		IDirect3DTexture9 *texture = nullptr;
		if (FAILED(device->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr)))
			return nullptr;
		data->white.reset(texture);
		D3DLOCKED_RECT texels{};
		if (FAILED(texture->LockRect(0, &texels, nullptr, 0)))
			return nullptr;
		*static_cast<uint32_t *>(texels.pBits) = 0xffffffffu;
		if (FAILED(texture->UnlockRect(0)))
			return nullptr;
	}

	const D3DVERTEXELEMENT9 declaration[] = {
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, static_cast<WORD>(offsetof(WaterVertex, terrain)), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
		D3DDECL_END()
	};
	const auto terrainColor = static_cast<WORD>(shadowTerrain ? offsetof(TerrainVertex, fill) : offsetof(TerrainVertex, diffuse));
	const D3DVERTEXELEMENT9 terrainDeclaration[] = {
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, static_cast<WORD>(offsetof(TerrainVertex, normal)), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
		{0, terrainColor, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
		{0, static_cast<WORD>(offsetof(TerrainVertex, uv)), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		D3DDECL_END()
	};
	const D3DVERTEXELEMENT9 modelDeclaration[] = {
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, static_cast<WORD>(offsetof(NormalMappedVertex, normal)), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
		{0, static_cast<WORD>(offsetof(NormalMappedVertex, tangent)), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0},
		{0, static_cast<WORD>(offsetof(NormalMappedVertex, bitangent)), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0},
		{0, static_cast<WORD>(offsetof(NormalMappedVertex, uv)), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		D3DDECL_END()
	};
	const D3DVERTEXELEMENT9 defaultDeclaration[] = {
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, static_cast<WORD>(offsetof(NormalMappedVertex, normal)), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
		{0, static_cast<WORD>(offsetof(NormalMappedVertex, uv)), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		{0, static_cast<WORD>(offsetof(NormalMappedVertex, uv1)), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
		{0, static_cast<WORD>(offsetof(NormalMappedVertex, color)), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
		D3DDECL_END()
	};
	const D3DVERTEXELEMENT9 vegetationDeclaration[] = {
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, static_cast<WORD>(offsetof(VegetationVertex, normal)), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
		{0, static_cast<WORD>(offsetof(VegetationVertex, uv)), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		{0, static_cast<WORD>(offsetof(VegetationVertex, sway)), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
		{0, static_cast<WORD>(offsetof(VegetationVertex, color)), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
		D3DDECL_END()
	};
	IDirect3DVertexDeclaration9 *decl = nullptr;
	if (FAILED(device->CreateVertexDeclaration(terrain ? terrainDeclaration : model ? modelDeclaration :
		defaultMesh ? defaultDeclaration : vegetation ? vegetationDeclaration : declaration, &decl)))
		return nullptr;
	data->declaration.reset(decl);
	data->vertex.reset(static_cast<IDirect3DVertexShader9 *>(createShader(true, data->vs.tokens.data())));
	data->pixel.reset(static_cast<IDirect3DPixelShader9 *>(createShader(false, data->ps.tokens.data())));
	if (!data->vertex || !data->pixel)
	{
		std::fprintf(stderr, "[RetailFX] D3D9 rejected %s shader creation\n", effectName);
		return nullptr;
	}
	return std::unique_ptr<NativePass>(new NativePass(std::move(data)));
}

bool NativePass::drawWater(void *handle, std::span<const WaterVertex> vertices, std::span<const uint16_t> indices,
	const SetBinding &setBinding, const std::function<void *(std::string_view)> &getTexture, bool additive)
{
	auto *device = static_cast<IDirect3DDevice9 *>(handle);
	if (vertices.empty() || indices.empty())
		return false;
	return m_data->draw(device, setBinding, getTexture, [&] {
		if (FAILED(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE)) ||
			(additive && FAILED(device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE))))
			return false;
		return SUCCEEDED(device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, static_cast<UINT>(vertices.size()),
			static_cast<UINT>(indices.size() / 3), indices.data(), D3DFMT_INDEX16, vertices.data(), sizeof(WaterVertex)));
	});
}

bool NativePass::drawTerrain(void *handle, std::span<const TerrainVertex> base, std::span<const uint32_t> baseIndices,
	std::span<const TerrainVertex> blend, std::span<const uint32_t> blendIndices,
	const SetBinding &setBinding, const std::function<void *(std::string_view)> &getTexture,
	std::span<const TerrainVertex> third, std::span<const uint32_t> thirdIndices, void *thirdAtlas)
{
	auto *device = static_cast<IDirect3DDevice9 *>(handle);
	if (base.empty() || baseIndices.empty() || (!thirdIndices.empty() && (!thirdAtlas || third.empty())))
		return false;
	return m_data->draw(device, setBinding, getTexture, [&] {
		const auto draw = [&](auto vertices, auto indices) {
			return SUCCEEDED(device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, static_cast<UINT>(vertices.size()),
				static_cast<UINT>(indices.size() / 3), indices.data(), D3DFMT_INDEX32, vertices.data(), sizeof(TerrainVertex)));
		};
		if (FAILED(device->SetRenderState(D3DRS_COLORWRITEENABLE, 7u)) ||
			FAILED(device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE)) ||
			FAILED(device->SetRenderState(D3DRS_DEPTHBIAS, std::bit_cast<uint32_t>(-0.000001f))) ||
			FAILED(device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE)) || !draw(base, baseIndices))
			return false;
		if ((!blendIndices.empty() || !thirdIndices.empty()) && FAILED(device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE)))
			return false;
		if (!blendIndices.empty() && !draw(blend, blendIndices))
			return false;
		if (!thirdIndices.empty() && (FAILED(device->SetTexture(0, static_cast<IDirect3DBaseTexture9 *>(thirdAtlas))) ||
			!draw(third, thirdIndices)))
			return false;
		return true;
	});
}

bool NativePass::drawVegetation(void *handle, std::span<const VegetationVertex> vertices, std::span<const uint16_t> indices,
	const SetBinding &setBinding, const std::function<void *(std::string_view)> &getTexture)
{
	auto *device = static_cast<IDirect3DDevice9 *>(handle);
	if (vertices.empty() || indices.empty())
		return false;
	return m_data->draw(device, setBinding, getTexture, [&] {
		return SUCCEEDED(device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE)) &&
			SUCCEEDED(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE)) &&
			SUCCEEDED(device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, static_cast<UINT>(vertices.size()),
				static_cast<UINT>(indices.size() / 3), indices.data(), D3DFMT_INDEX16, vertices.data(), sizeof(VegetationVertex)));
	});
}

bool NativePass::Data::draw(IDirect3DDevice9 *device, const SetBinding &setBinding,
	const std::function<void *(std::string_view)> &getTexture, const std::function<bool()> &issueDraw, bool reuseState)
{
	Registers vertexRegs, pixelRegs;
	if (!device || !fillRegisters(vs, setBinding, vertexRegs) || !fillRegisters(ps, setBinding, pixelRegs))
		return false;

	std::array<IDirect3DBaseTexture9 *, 16> textures{};
	for (const auto &sampler : samplers)
	{
		auto *texture = sampler.white ? white.get() : static_cast<IDirect3DBaseTexture9 *>(getTexture(sampler.texture));
		if (!texture && sampler.texture == "CloudTexture")
			texture = white.get();
		if (!texture)
			return false;
		textures[sampler.slot] = texture;
	}

	Com<IDirect3DStateBlock9> localState;
	auto &saved = reuseState ? meshState : localState;
	if (!saved)
	{
		IDirect3DStateBlock9 *block = nullptr;
		if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &block)))
			return false;
		saved.reset(block);
	}
	else if (FAILED(saved->Capture()))
		return false;
	const bool ready = [&] {
		for (const auto &[state, value] : states)
			if (FAILED(device->SetRenderState(state, value)))
				return false;
		for (const auto &sampler : samplers)
		{
			if (FAILED(device->SetTexture(sampler.slot, textures[sampler.slot])))
				return false;
			for (const auto &[state, value] : sampler.states)
				if (FAILED(device->SetSamplerState(sampler.slot, state, value)))
					return false;
		}
		std::array<BOOL, 16> booleans{};
		std::copy(vertexRegs.booleans.begin(), vertexRegs.booleans.end(), booleans.begin());
		return SUCCEEDED(device->SetVertexDeclaration(declaration.get())) &&
			SUCCEEDED(device->SetVertexShader(vertex.get())) &&
			SUCCEEDED(device->SetPixelShader(pixel.get())) &&
			SUCCEEDED(device->SetVertexShaderConstantF(0, vertexRegs.floats[0].data(), 256)) &&
			SUCCEEDED(device->SetVertexShaderConstantB(0, booleans.data(), 16)) &&
			SUCCEEDED(device->SetPixelShaderConstantF(0, pixelRegs.floats[0].data(), 32));
	}();
	const bool drawn = ready && issueDraw();
	const bool restored = SUCCEEDED(saved->Apply());
	return drawn && restored;
}

bool NativePass::drawNormalMapped(void *handle, std::span<const NormalMappedVertex> vertices, std::span<const uint32_t> indices,
	const SetBinding &setBinding, const std::function<void *(std::string_view)> &getTexture,
	bool alphaTest, bool depthWrite, bool twoSided)
{
	auto *device = static_cast<IDirect3DDevice9 *>(handle);
	if (vertices.empty() || indices.empty())
		return false;
	return m_data->draw(device, setBinding, getTexture, [&] {
		const bool categoryState = m_data->useCategoryDepthAndCull;
		if (FAILED(device->SetRenderState(D3DRS_ALPHATESTENABLE, alphaTest ? TRUE : FALSE)) ||
			FAILED(device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE)) ||
			FAILED(device->SetRenderState(D3DRS_FOGENABLE, FALSE)) ||
			(categoryState && FAILED(device->SetRenderState(D3DRS_ZWRITEENABLE, depthWrite ? TRUE : FALSE))) ||
			((categoryState || twoSided) && FAILED(device->SetRenderState(D3DRS_CULLMODE, twoSided ? D3DCULL_NONE : D3DCULL_CW))))
			return false;
		return SUCCEEDED(device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, static_cast<UINT>(vertices.size()),
			static_cast<UINT>(indices.size() / 3), indices.data(), D3DFMT_INDEX32, vertices.data(), sizeof(NormalMappedVertex)));
	}, true);
}

#else

struct NativePass::Data {};

std::unique_ptr<NativePass> NativePass::makeWater(void *, const Effect &, CreateShader) { return nullptr; }
std::unique_ptr<NativePass> NativePass::makeFullWater(void *, const Effect &, CreateShader) { return nullptr; }
std::unique_ptr<NativePass> NativePass::makeTerrain(void *, const Effect &, CreateShader) { return nullptr; }
std::unique_ptr<NativePass> NativePass::makeTerrainWithShadows(void *, const Effect &, CreateShader) { return nullptr; }
std::unique_ptr<NativePass> NativePass::makeNormalMapped(void *, const Effect &, CreateShader) { return nullptr; }
std::unique_ptr<NativePass> NativePass::makeDefaultMesh(void *, const Effect &, CreateShader) { return nullptr; }
std::unique_ptr<NativePass> NativePass::makeVegetation(void *, const Effect &, CreateShader) { return nullptr; }

bool NativePass::drawVegetation(void *, std::span<const VegetationVertex>, std::span<const uint16_t>,
	const SetBinding &, const std::function<void *(std::string_view)> &) { return false; }

bool NativePass::drawWater(void *, std::span<const WaterVertex>, std::span<const uint16_t>,
	const SetBinding &, const std::function<void *(std::string_view)> &, bool) { return false; }

bool NativePass::drawTerrain(void *, std::span<const TerrainVertex>, std::span<const uint32_t>,
	std::span<const TerrainVertex>, std::span<const uint32_t>, const SetBinding &,
	const std::function<void *(std::string_view)> &, std::span<const TerrainVertex>,
	std::span<const uint32_t>, void *) { return false; }

bool NativePass::drawNormalMapped(void *, std::span<const NormalMappedVertex>, std::span<const uint32_t>,
	const SetBinding &, const std::function<void *(std::string_view)> &, bool, bool, bool) { return false; }

#endif

NativePass::NativePass(std::unique_ptr<Data> data) : m_data(std::move(data)) {}
NativePass::~NativePass() = default;

}
