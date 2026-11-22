#include "W3DDevice/GameClient/W3DRetailPostEffect.h"

#include "WW3D2/retailfxprogram.h"

#if defined(__linux__) || defined(__APPLE__)
#include <d3d9.h>
#endif

#include <algorithm>
#include <cmath>
#include <utility>

namespace RetailFX
{

#if defined(__linux__) || defined(__APPLE__)
namespace
{

template<class T>
struct Release
{
	void operator()(T *value) const { value->Release(); }
};

template<class T>
using Com = std::unique_ptr<T, Release<T>>;

using SamplerStates = std::vector<std::pair<D3DSAMPLERSTATETYPE, DWORD>>;

struct LookupVertex
{
	float x, y, u, v;
};

std::optional<Program> lookupProgram(const Pass &pass, bool vertex)
{
	const unsigned operation = vertex ? 146 : 147;
	const auto state = std::find_if(pass.states.begin(), pass.states.end(),
		[=](const State &item) { return item.operation == operation; });
	if (state == pass.states.end() || state->index || state->resourceUsage != ResourceUsage::Program ||
		state->value.type != (vertex ? ValueType::VertexShader : ValueType::PixelShader))
		return {};
	auto program = readProgram(state->resource, vertex);
	if (!program || program->tokens.front() != (vertex ? 0xfffe0101u : 0xffff0200u) ||
		!program->instructions.empty())
		return {};
	return program;
}

bool readSampler(const Effect &effect, std::string_view name, std::string_view texture,
	ValueType type, SamplerStates &states)
{
	const auto parameter = std::find_if(effect.parameters.begin(), effect.parameters.end(),
		[=](const Parameter &item) { return item.value.name == name; });
	if (parameter == effect.parameters.end() || parameter->value.type != type)
		return false;
	bool textureFound = false;
	for (const auto &state : parameter->value.samplerStates)
	{
		if (state.operation == 164 && state.resourceUsage == ResourceUsage::Parameter &&
			!textureFound && state.parameterReference == texture)
			textureFound = true;
		else if (state.operation >= 165 && state.operation <= 175 && !state.resourceUsage && state.value.words.size() == 1)
			states.emplace_back(static_cast<D3DSAMPLERSTATETYPE>(state.operation - 164), state.value.words[0]);
		else
			return false;
	}
	return textureFound;
}

Com<IDirect3DVolumeTexture9> makeVolume(IDirect3DDevice9 *device, std::span<const uint8_t> pixels,
	unsigned pixelSize, bool topOrigin)
{
	constexpr auto size = LookupSize;
	if ((pixelSize != 3 && pixelSize != 4) || pixels.size() != size * size * size * pixelSize)
		return {};
	IDirect3DVolumeTexture9 *texture = nullptr;
	if (FAILED(device->CreateVolumeTexture(size, size, size, 1, 0, D3DFMT_X8R8G8B8,
		D3DPOOL_MANAGED, &texture, nullptr)))
		return {};
	Com<IDirect3DVolumeTexture9> volume(texture);
	D3DLOCKED_BOX box{};
	if (FAILED(volume->LockBox(0, &box, nullptr, 0)))
		return {};
	for (unsigned blue = 0; blue < size; ++blue)
	{
		for (unsigned green = 0; green < size; ++green)
		{
			const unsigned row = topOrigin ? green : size - 1 - green;
			const auto *source = pixels.data() + (row * size * size + blue * size) * pixelSize;
			auto *destination = static_cast<uint8_t *>(box.pBits) + blue * box.SlicePitch + green * box.RowPitch;
			for (unsigned red = 0; red < size; ++red)
			{
				std::copy_n(source, 3, destination);
				destination[3] = 255;
				source += pixelSize;
				destination += 4;
			}
		}
	}
	if (FAILED(volume->UnlockBox(0)))
		return {};
	return volume;
}

}

struct LookupPass::Data
{
	Com<IDirect3DVertexShader9> vertex;
	Com<IDirect3DPixelShader9> pixel;
	Com<IDirect3DVertexDeclaration9> declaration;
	Com<IDirect3DVolumeTexture9> volume;
	std::array<SamplerStates, 2> samplers;
};

std::unique_ptr<LookupPass> LookupPass::make(void *handle, const Effect &effect, CreateShader createShader,
	std::span<const uint8_t> pixels, unsigned pixelSize, bool topOrigin)
{
	auto *device = static_cast<IDirect3DDevice9 *>(handle);
	const auto technique = std::find_if(effect.techniques.begin(), effect.techniques.end(),
		[](const Technique &item) { return item.name == "Default"; });
	if (!device || !createShader || technique == effect.techniques.end() || technique->passes.size() != 1)
		return {};

	const auto &pass = technique->passes.front();
	constexpr std::pair<unsigned, unsigned> expected[]{{59, 0}, {0, 0}, {3, 0}, {8, 1}, {13, 0}, {4, 0}, {14, 0}};
	if (pass.states.size() != std::size(expected) + 2)
		return {};
	for (const auto &[operation, value] : expected)
	{
		if (std::count_if(pass.states.begin(), pass.states.end(), [&](const State &state) {
			return state.operation == operation && !state.index && !state.resourceUsage &&
				state.value.words.size() == 1 && state.value.words[0] == value;
		}) != 1)
			return {};
	}

	const auto vertex = lookupProgram(pass, true);
	const auto pixel = lookupProgram(pass, false);
	if (!vertex || !pixel || !vertex->bindings.empty() || pixel->bindings.size() != 3)
		return {};
	const auto binds = [&](std::string_view name, unsigned bank, unsigned first) {
		return std::any_of(pixel->bindings.begin(), pixel->bindings.end(),
			[=](const Binding &binding) {
				return binding.name == name && binding.bank == bank && binding.first == first && binding.count == 1;
			});
	};
	if (!binds("BlendFactor", 2, 0) || !binds("FrameBufferSampler", 3, 0) || !binds("LookupTableSampler", 3, 1))
		return {};

	auto data = std::make_unique<Data>();
	if (!readSampler(effect, "FrameBufferSampler", "FrameBufferTexture", ValueType::Sampler2D, data->samplers[0]) ||
		!readSampler(effect, "LookupTableSampler", "LookupTexture", ValueType::Sampler3D, data->samplers[1]))
		return {};
	data->vertex.reset(static_cast<IDirect3DVertexShader9 *>(createShader(true, vertex->tokens.data())));
	data->pixel.reset(static_cast<IDirect3DPixelShader9 *>(createShader(false, pixel->tokens.data())));
	if (!data->vertex || !data->pixel)
		return {};

	const D3DVERTEXELEMENT9 elements[]{
		{0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, static_cast<WORD>(offsetof(LookupVertex, u)), D3DDECLTYPE_FLOAT2,
			D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		D3DDECL_END()
	};
	IDirect3DVertexDeclaration9 *declaration = nullptr;
	if (FAILED(device->CreateVertexDeclaration(elements, &declaration)))
		return {};
	data->declaration.reset(declaration);
	data->volume = makeVolume(device, pixels, pixelSize, topOrigin);
	if (!data->volume)
		return {};
	return std::unique_ptr<LookupPass>(new LookupPass(std::move(data)));
}

bool LookupPass::draw(void *handle, void *frameBuffer, int width, int height, float blend) const
{
	auto *device = static_cast<IDirect3DDevice9 *>(handle);
	if (!device || !frameBuffer || width <= 0 || height <= 0 || !std::isfinite(blend))
		return false;
	const float x = -1.0f / static_cast<float>(width);
	const float y = 1.0f / static_cast<float>(height);
	const LookupVertex quad[]{
		{-1.0f + x,  1.0f + y, 0.0f, 0.0f}, {1.0f + x,  1.0f + y, 1.0f, 0.0f},
		{-1.0f + x, -1.0f + y, 0.0f, 1.0f}, {1.0f + x, -1.0f + y, 1.0f, 1.0f}
	};
	const float factor[4]{blend, 0.0f, 0.0f, 0.0f};

	IDirect3DStateBlock9 *block = nullptr;
	if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &block)))
		return false;
	const Com<IDirect3DStateBlock9> saved(block);
	const auto draw = [&] {
		const std::pair<D3DRENDERSTATETYPE, DWORD> states[]{
			{D3DRS_ZENABLE, FALSE}, {D3DRS_ZWRITEENABLE, FALSE}, {D3DRS_CULLMODE, D3DCULL_NONE},
			{D3DRS_ALPHABLENDENABLE, FALSE}, {D3DRS_ALPHATESTENABLE, FALSE}, {D3DRS_FOGENABLE, FALSE},
			{D3DRS_LIGHTING, FALSE}, {D3DRS_STENCILENABLE, FALSE}, {D3DRS_SCISSORTESTENABLE, FALSE},
			{D3DRS_COLORWRITEENABLE, 0xf}
		};
		for (const auto &[state, value] : states)
			if (FAILED(device->SetRenderState(state, value)))
				return false;
		for (unsigned slot = 0; slot < m_data->samplers.size(); ++slot)
		{
			if (FAILED(device->SetSamplerState(slot, D3DSAMP_SRGBTEXTURE, FALSE)))
				return false;
			for (const auto &[state, value] : m_data->samplers[slot])
				if (FAILED(device->SetSamplerState(slot, state, value)))
					return false;
		}
		return SUCCEEDED(device->SetTexture(0, static_cast<IDirect3DBaseTexture9 *>(frameBuffer))) &&
			SUCCEEDED(device->SetTexture(1, m_data->volume.get())) &&
			SUCCEEDED(device->SetVertexDeclaration(m_data->declaration.get())) &&
			SUCCEEDED(device->SetVertexShader(m_data->vertex.get())) &&
			SUCCEEDED(device->SetPixelShader(m_data->pixel.get())) &&
			SUCCEEDED(device->SetPixelShaderConstantF(0, factor, 1)) &&
			SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(LookupVertex)));
	};
	const bool drawn = draw();
	const bool restored = SUCCEEDED(saved->Apply());
	return drawn && restored;
}

#else

struct LookupPass::Data {};

std::unique_ptr<LookupPass> LookupPass::make(void *, const Effect &, CreateShader, std::span<const uint8_t>, unsigned, bool)
{
	return {};
}

bool LookupPass::draw(void *, void *, int, int, float) const
{
	return false;
}

#endif

LookupPass::LookupPass(std::unique_ptr<Data> data) : m_data(std::move(data)) {}
LookupPass::~LookupPass() = default;

}
