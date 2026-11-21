#pragma once

#include "Common/File.h"
#include "Common/FileSystem.h"
#include "W3DDevice/GameClient/RetailEffectDraw.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/texture.h"
#include "d3dx8math.h"

#include <algorithm>
#include <bit>
#include <cstdio>

namespace RetailFX
{

inline std::optional<Effect> loadEffectFile(const char *name)
{
	auto file = std::unique_ptr<File, void (*)(File *)>(TheFileSystem->openFile(name, File::READ | File::BINARY),
		[](File *value) { value->close(); });
	if (!file)
		return {};
	const Int size = file->size();
	if (size <= 0 || size > 16 * 1024 * 1024)
		return {};
	std::vector<uint8_t> bytes(static_cast<size_t>(size));
	if (file->read(bytes.data(), size) != size)
		return {};
	std::string error;
	auto effect = parse(bytes, error);
	if (!effect)
		std::fprintf(stderr, "[RetailFX] Invalid %s: %s\n", name, error.c_str());
	return effect;
}

inline void *nativeTexture(TextureClass *texture)
{
	if (!texture)
		return nullptr;
	if (!texture->Is_Initialized())
		texture->Init();
	if (texture->Is_Missing_Texture())
		return nullptr;
	return DX8Wrapper::Get_Effect_Texture9(texture->Peek_D3D_Base_Texture());
}

inline void writeMatrix(std::span<Register> values, const D3DXMATRIX &matrix)
{
	for (unsigned i = 0; i < values.size() && i < 4; ++i)
		values[i] = {matrix.m[0][i], matrix.m[1][i], matrix.m[2][i], matrix.m[3][i]};
}

inline std::array<Register, 4> fogBindings(IDirect3DDevice8 *device)
{
	DWORD fogEnabled = 0, fogColor = 0, fogStart = 0, fogEnd = 0;
	device->GetRenderState(D3DRS_FOGENABLE, &fogEnabled);
	device->GetRenderState(D3DRS_FOGCOLOR, &fogColor);
	device->GetRenderState(D3DRS_FOGSTART, &fogStart);
	device->GetRenderState(D3DRS_FOGEND, &fogEnd);
	const float start = std::bit_cast<float>(static_cast<uint32_t>(fogStart));
	const float end = std::max(std::bit_cast<float>(static_cast<uint32_t>(fogEnd)), start + 1.0f);
	const auto channel = [&](unsigned shift) { return static_cast<float>((fogColor >> shift) & 255) / 255.0f; };
	return {{
		{fogEnabled ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f},
		{channel(16), channel(8), channel(0), 1.0f},
		{start, 0.0f, 0.0f, 0.0f},
		{end, 0.0f, 0.0f, 0.0f}
	}};
}

}
