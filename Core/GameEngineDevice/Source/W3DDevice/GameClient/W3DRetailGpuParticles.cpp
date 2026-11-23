#include "W3DDevice/GameClient/W3DRetailGpuParticles.h"

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

using Sampler = std::vector<std::pair<D3DSAMPLERSTATETYPE, DWORD>>;

std::optional<Sampler> readSampler(const Effect &effect, const char *name)
{
	const auto parameter = std::find_if(effect.parameters.begin(), effect.parameters.end(),
		[=](const Parameter &item) { return item.value.name == name; });
	if (parameter == effect.parameters.end() || parameter->value.type != ValueType::Sampler2D)
		return {};
	Sampler states;
	bool atlas = false;
	for (const auto &state : parameter->value.samplerStates)
	{
		if (state.operation == 164 && !atlas && state.resourceUsage == ResourceUsage::Parameter &&
			state.parameterReference == "ParticleTexture")
			atlas = true;
		else if (state.operation >= 165 && state.operation <= 171 && !state.resourceUsage &&
			state.value.words.size() == 1)
			states.emplace_back(static_cast<D3DSAMPLERSTATETYPE>(state.operation - 164), state.value.words[0]);
		else
			return {};
	}
	return atlas ? std::optional(std::move(states)) : std::nullopt;
}

std::optional<DWORD> readSelection(const Program &program, const SetBinding &setBinding, unsigned lower, unsigned upper)
{
	Registers values;
	if (!fillRegisters(program, setBinding, values))
		return {};
	const float choice = values.floats[0][0];
	if (choice < static_cast<float>(lower) || choice > static_cast<float>(upper) || std::trunc(choice) != choice)
		return {};
	return static_cast<DWORD>(choice);
}

struct ScopedState
{
	IDirect3DStateBlock9 *block;
	~ScopedState() { block->Apply(); }
};

}

struct GpuParticlePass::Data
{
	Program vertexProgram;
	std::array<Program, 3> pixelPrograms;
	Program pixelSelector;
	Program srcBlend;
	Program destBlend;
	std::array<Sampler, 2> samplers;
	Com<IDirect3DVertexShader9> vertex;
	std::array<Com<IDirect3DPixelShader9>, 3> pixels;
	Com<IDirect3DVertexDeclaration9> declaration;
	Com<IDirect3DStateBlock9> state;
};

std::unique_ptr<GpuParticlePass> GpuParticlePass::make(void *handle, const Effect &effect, NativePass::CreateShader createShader)
{
	auto *device = static_cast<IDirect3DDevice9 *>(handle);
	const auto technique = std::find_if(effect.techniques.begin(), effect.techniques.end(),
		[](const Technique &item) { return item.name == "Default"; });
	const auto pixelArray = std::find_if(effect.parameters.begin(), effect.parameters.end(),
		[](const Parameter &item) { return item.value.name == "PS_Array"; });
	if (!device || !createShader || technique == effect.techniques.end() || technique->passes.size() != 1 ||
		pixelArray == effect.parameters.end() || pixelArray->value.elements.size() != 3)
		return {};
	auto data = std::make_unique<Data>();
	for (const auto &state : technique->passes[0].states)
	{
		if (state.index)
			return {};
		if (state.operation == 146 && state.resourceUsage == ResourceUsage::Program && state.value.type == ValueType::VertexShader)
		{
			auto program = readProgram(state.resource, true);
			if (!program || program->tokens.front() != 0xfffe0200u || !data->vertexProgram.tokens.empty())
				return {};
			data->vertexProgram = std::move(*program);
		}
		else if (state.operation == 147 && state.resourceUsage == ResourceUsage::ArraySelector &&
			state.value.type == ValueType::PixelShader && state.parameterReference == "PS_Array")
		{
			constexpr size_t headerSize = 16;
			if (state.resource.size() <= headerSize || state.resource[0] != 12 || state.resource[1] ||
				state.resource[2] || state.resource[3] || !data->pixelSelector.instructions.empty())
				return {};
			auto selector = readStateProgram(std::span(state.resource).subspan(headerSize));
			if (!selector)
				return {};
			data->pixelSelector = std::move(*selector);
		}
		else if ((state.operation == 6 || state.operation == 7) && state.resourceUsage == ResourceUsage::Program)
		{
			auto &target = state.operation == 6 ? data->srcBlend : data->destBlend;
			auto program = readStateProgram(state.resource);
			if (!program || !target.instructions.empty())
				return {};
			target = std::move(*program);
		}
		else
		{
			const auto expected = [=]() -> std::optional<uint32_t> {
				switch (state.operation)
				{
					case 0: case 13: return 1;
					case 3: case 14: return 0;
					case 8: return D3DCULL_CW;
					case 9: return D3DCMP_LESSEQUAL;
					default: return {};
				}
			}();
			if (!expected || state.resourceUsage || state.value.words.size() != 1 || state.value.words[0] != *expected)
				return {};
		}
	}
	if (data->vertexProgram.tokens.empty() || data->pixelSelector.instructions.empty() ||
		data->srcBlend.instructions.empty() || data->destBlend.instructions.empty())
		return {};

	for (size_t i = 0; i < data->pixels.size(); ++i)
	{
		const auto &item = pixelArray->value.elements[i];
		if (item.type != ValueType::PixelShader || !item.object || *item.object >= effect.objects.size())
			return {};
		auto program = readProgram(effect.objects[*item.object], false);
		if (!program || program->tokens.front() != 0xffff0200u ||
			std::count_if(program->bindings.begin(), program->bindings.end(),
				[](const auto &binding) { return binding.bank == 3; }) != data->samplers.size())
			return {};
		data->pixelPrograms[i] = std::move(*program);
		data->pixels[i].reset(static_cast<IDirect3DPixelShader9 *>(createShader(false, data->pixelPrograms[i].tokens.data())));
		if (!data->pixels[i])
			return {};
	}
	const char *samplers[] = {"ParticleTextureSampler", "NextFrameTextureSampler"};
	for (size_t i = 0; i < data->samplers.size(); ++i)
	{
		auto sampler = readSampler(effect, samplers[i]);
		const bool bindingsMatch = std::all_of(data->pixelPrograms.begin(), data->pixelPrograms.end(), [&](const auto &program) {
			return std::any_of(program.bindings.begin(), program.bindings.end(), [&](const auto &binding) {
				return binding.bank == 3 && binding.name == samplers[i] && binding.first == i && binding.count == 1;
			});
		});
		if (!sampler || !bindingsMatch)
			return {};
		data->samplers[i] = std::move(*sampler);
	}
	data->vertex.reset(static_cast<IDirect3DVertexShader9 *>(createShader(true, data->vertexProgram.tokens.data())));
	if (!data->vertex)
		return {};
	const D3DVERTEXELEMENT9 elements[] = {
		{0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 16, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		{0, 32, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
		D3DDECL_END()
	};
	static_assert(sizeof(GpuParticleVertex) == 40);
	IDirect3DVertexDeclaration9 *declaration = nullptr;
	if (FAILED(device->CreateVertexDeclaration(elements, &declaration)))
		return {};
	data->declaration.reset(declaration);
	IDirect3DStateBlock9 *block = nullptr;
	if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &block)))
		return {};
	data->state.reset(block);
	return std::unique_ptr<GpuParticlePass>(new GpuParticlePass(std::move(data)));
}

bool GpuParticlePass::draw(void *handle, void *atlas, std::span<const GpuParticleVertex> vertices,
	std::span<const uint16_t> indices, const SetBinding &setBinding)
{
	auto *device = static_cast<IDirect3DDevice9 *>(handle);
	auto *texture = static_cast<IDirect3DBaseTexture9 *>(atlas);
	if (!device || !texture || vertices.empty() || vertices.size() > 65536 || indices.empty() || indices.size() % 3)
		return false;
	const auto pixel = readSelection(m_data->pixelSelector, setBinding, 0, 2);
	const auto source = readSelection(m_data->srcBlend, setBinding, D3DBLEND_ONE, D3DBLEND_DESTCOLOR);
	const auto destination = readSelection(m_data->destBlend, setBinding, D3DBLEND_ZERO, D3DBLEND_INVSRCALPHA);
	Registers vertexValues, pixelValues;
	if (!pixel || !source || !destination ||
		!fillRegisters(m_data->vertexProgram, setBinding, vertexValues) ||
		!fillRegisters(m_data->pixelPrograms[*pixel], setBinding, pixelValues) || FAILED(m_data->state->Capture()))
		return false;
	const ScopedState restore{m_data->state.get()};
	static constexpr std::array<std::array<int, 4>, 16> vertexIntegers{};
	if (FAILED(device->SetVertexDeclaration(m_data->declaration.get())) ||
		FAILED(device->SetVertexShader(m_data->vertex.get())) ||
		FAILED(device->SetPixelShader(m_data->pixels[*pixel].get())) ||
		FAILED(device->SetVertexShaderConstantF(0, vertexValues.floats.front().data(), static_cast<UINT>(vertexValues.floats.size()))) ||
		FAILED(device->SetVertexShaderConstantB(0, vertexValues.booleans.data(), static_cast<UINT>(vertexValues.booleans.size()))) ||
		FAILED(device->SetVertexShaderConstantI(0, vertexIntegers.front().data(), static_cast<UINT>(vertexIntegers.size()))) ||
		FAILED(device->SetPixelShaderConstantF(0, pixelValues.floats.front().data(), 32)))
		return false;
	const std::pair<D3DRENDERSTATETYPE, DWORD> states[] = {
		{D3DRS_ZENABLE, TRUE}, {D3DRS_ZWRITEENABLE, FALSE}, {D3DRS_ZFUNC, D3DCMP_LESSEQUAL},
		{D3DRS_CULLMODE, D3DCULL_CW}, {D3DRS_ALPHABLENDENABLE, TRUE}, {D3DRS_BLENDOP, D3DBLENDOP_ADD},
		{D3DRS_SRCBLEND, *source}, {D3DRS_DESTBLEND, *destination}, {D3DRS_FOGENABLE, FALSE},
		{D3DRS_ALPHATESTENABLE, FALSE}, {D3DRS_SEPARATEALPHABLENDENABLE, FALSE}
	};
	for (const auto &[state, value] : states)
		if (FAILED(device->SetRenderState(state, value)))
			return false;
	for (DWORD i = 0; i < m_data->samplers.size(); ++i)
	{
		if (FAILED(device->SetTexture(i, texture)))
			return false;
		for (const auto &[state, value] : m_data->samplers[i])
			if (FAILED(device->SetSamplerState(i, state, value)))
				return false;
	}
	return SUCCEEDED(device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, static_cast<UINT>(vertices.size()),
		static_cast<UINT>(indices.size() / 3), indices.data(), D3DFMT_INDEX16, vertices.data(), sizeof(GpuParticleVertex)));
}

#else

struct GpuParticlePass::Data {};

std::unique_ptr<GpuParticlePass> GpuParticlePass::make(void *, const Effect &, NativePass::CreateShader) { return {}; }
bool GpuParticlePass::draw(void *, void *, std::span<const GpuParticleVertex>,
	std::span<const uint16_t>, const SetBinding &) { return false; }

#endif

GpuParticlePass::GpuParticlePass(std::unique_ptr<Data> data) : m_data(std::move(data)) {}
GpuParticlePass::~GpuParticlePass() = default;

}
