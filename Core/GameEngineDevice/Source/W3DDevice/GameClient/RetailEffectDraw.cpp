#include "W3DDevice/GameClient/RetailEffectDraw.h"

#if defined(__linux__) || defined(__APPLE__)
#include <d3d9.h>
#endif

#include <algorithm>
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
	std::vector<std::pair<D3DRENDERSTATETYPE, DWORD>> states;
	std::vector<Sampler> samplers;
	Program vs;
	Program ps;
};

std::unique_ptr<NativePass> NativePass::makeWater(void *handle, const Effect &effect, CreateShader createShader)
{
	auto *device = static_cast<IDirect3DDevice9 *>(handle);
	const auto technique = std::find_if(effect.techniques.begin(), effect.techniques.end(),
		[](const auto &item) { return item.name == "BumpSpecular_M"; });
	if (!device || technique == effect.techniques.end() || technique->passes.size() != 1)
		return nullptr;
	auto data = std::make_unique<Data>();
	for (const auto &state : technique->passes[0].states)
	{
		if (state.operation == 146 || state.operation == 147)
		{
			const bool vertex = state.operation == 146;
			const auto type = vertex ? ValueType::VertexShader : ValueType::PixelShader;
			if (state.value.type != type || state.resourceUsage != ResourceUsage::Program)
				return nullptr;
			auto shader = readProgram(state.resource, vertex);
			if (!shader)
			{
				std::fprintf(stderr, "[RetailFX] Unsupported WaterShader %s bytecode\n", vertex ? "vertex" : "pixel");
				return nullptr;
			}
			(vertex ? data->vs : data->ps) = std::move(*shader);
		}
		else
		{
			const auto native = renderState(state.operation);
			if (!native || state.index || state.resourceUsage || state.value.words.size() != 1)
			{
				std::fprintf(stderr, "[RetailFX] Unsupported WaterShader state %u\n", state.operation);
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
			std::fprintf(stderr, "[RetailFX] Unsupported WaterShader sampler %s\n", binding.name.c_str());
			return nullptr;
		}
		data->samplers.push_back(std::move(*sampler));
	}

	const D3DVERTEXELEMENT9 declaration[] = {
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, static_cast<WORD>(offsetof(WaterVertex, terrain)), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
		D3DDECL_END()
	};
	IDirect3DVertexDeclaration9 *decl = nullptr;
	if (FAILED(device->CreateVertexDeclaration(declaration, &decl)))
		return nullptr;
	data->declaration.reset(decl);
	data->vertex.reset(static_cast<IDirect3DVertexShader9 *>(createShader(true, data->vs.tokens.data())));
	data->pixel.reset(static_cast<IDirect3DPixelShader9 *>(createShader(false, data->ps.tokens.data())));
	if (!data->vertex || !data->pixel)
	{
		std::fprintf(stderr, "[RetailFX] D3D9 rejected WaterShader shader creation\n");
		return nullptr;
	}
	return std::unique_ptr<NativePass>(new NativePass(std::move(data)));
}

bool NativePass::drawWater(void *handle, std::span<const WaterVertex> vertices, std::span<const uint16_t> indices,
	const SetBinding &setBinding, const std::function<void *(std::string_view)> &getTexture, bool additive)
{
	auto *device = static_cast<IDirect3DDevice9 *>(handle);
	Registers vs, ps;
	if (!device || vertices.empty() || indices.empty() || !fillRegisters(m_data->vs, setBinding, vs) ||
		!fillRegisters(m_data->ps, setBinding, ps))
		return false;

	std::array<IDirect3DBaseTexture9 *, 16> textures{};
	for (const auto &sampler : m_data->samplers)
	{
		auto *texture = static_cast<IDirect3DBaseTexture9 *>(getTexture(sampler.texture));
		if (!texture)
			return false;
		textures[sampler.slot] = texture;
	}

	IDirect3DStateBlock9 *block = nullptr;
	if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &block)))
		return false;
	const Com<IDirect3DStateBlock9> saved(block);
	const bool ready = [&] {
		for (const auto &[state, value] : m_data->states)
			if (FAILED(device->SetRenderState(state, value)))
				return false;
		if (FAILED(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE)))
			return false;
		if (additive && FAILED(device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE)))
			return false;
		for (const auto &sampler : m_data->samplers)
		{
			if (FAILED(device->SetTexture(sampler.slot, textures[sampler.slot])))
				return false;
			for (const auto &[state, value] : sampler.states)
				if (FAILED(device->SetSamplerState(sampler.slot, state, value)))
					return false;
		}
		std::array<BOOL, 16> booleans{};
		std::copy(vs.booleans.begin(), vs.booleans.end(), booleans.begin());
		return SUCCEEDED(device->SetVertexDeclaration(m_data->declaration.get())) &&
			SUCCEEDED(device->SetVertexShader(m_data->vertex.get())) &&
			SUCCEEDED(device->SetPixelShader(m_data->pixel.get())) &&
			SUCCEEDED(device->SetVertexShaderConstantF(0, vs.floats[0].data(), 256)) &&
			SUCCEEDED(device->SetVertexShaderConstantB(0, booleans.data(), 16)) &&
			SUCCEEDED(device->SetPixelShaderConstantF(0, ps.floats[0].data(), 32));
	}();
	bool drawn = false;
	if (ready)
		drawn = SUCCEEDED(device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, static_cast<UINT>(vertices.size()),
			static_cast<UINT>(indices.size() / 3), indices.data(), D3DFMT_INDEX16, vertices.data(), sizeof(WaterVertex)));
	const bool restored = SUCCEEDED(saved->Apply());
	return drawn && restored;
}

#else

struct NativePass::Data {};

std::unique_ptr<NativePass> NativePass::makeWater(void *, const Effect &, CreateShader) { return nullptr; }

bool NativePass::drawWater(void *, std::span<const WaterVertex>, std::span<const uint16_t>,
	const SetBinding &, const std::function<void *(std::string_view)> &, bool) { return false; }

#endif

NativePass::NativePass(std::unique_ptr<Data> data) : m_data(std::move(data)) {}
NativePass::~NativePass() = default;

}
