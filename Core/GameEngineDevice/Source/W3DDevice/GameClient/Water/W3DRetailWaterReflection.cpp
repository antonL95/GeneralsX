#include "W3DRetailWaterReflection.h"

#if defined(__linux__) || defined(__APPLE__)
#include <d3d9.h>
#endif

#include <memory>

namespace RetailFX
{

#if defined(__linux__) || defined(__APPLE__)

namespace
{

struct Release
{
	template<class T>
	void operator()(T *object) const { object->Release(); }
};

template<class T>
using Com = std::unique_ptr<T, Release>;

struct Vertex
{
	float x, y, z, rhw;
	float u, v;
};

bool configureCopy(IDirect3DDevice9 &device, IDirect3DTexture9 *source)
{
	return SUCCEEDED(device.SetVertexShader(nullptr)) &&
		SUCCEEDED(device.SetPixelShader(nullptr)) &&
		SUCCEEDED(device.SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1)) &&
		SUCCEEDED(device.SetTexture(0, source)) &&
		SUCCEEDED(device.SetRenderState(D3DRS_ZENABLE, FALSE)) &&
		SUCCEEDED(device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE)) &&
		SUCCEEDED(device.SetRenderState(D3DRS_STENCILENABLE, FALSE)) &&
		SUCCEEDED(device.SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE)) &&
		SUCCEEDED(device.SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID)) &&
		SUCCEEDED(device.SetRenderState(D3DRS_LIGHTING, FALSE)) &&
		SUCCEEDED(device.SetRenderState(D3DRS_FOGENABLE, FALSE)) &&
		SUCCEEDED(device.SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE)) &&
		SUCCEEDED(device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE)) &&
		SUCCEEDED(device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE)) &&
		SUCCEEDED(device.SetRenderState(D3DRS_COLORWRITEENABLE, 15)) &&
		SUCCEEDED(device.SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE)) &&
		SUCCEEDED(device.SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0)) &&
		SUCCEEDED(device.SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE)) &&
		SUCCEEDED(device.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1)) &&
		SUCCEEDED(device.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE)) &&
		SUCCEEDED(device.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1)) &&
		SUCCEEDED(device.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE)) &&
		SUCCEEDED(device.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE)) &&
		SUCCEEDED(device.SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE)) &&
		SUCCEEDED(device.SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP)) &&
		SUCCEEDED(device.SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP)) &&
		SUCCEEDED(device.SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR)) &&
		SUCCEEDED(device.SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR)) &&
		SUCCEEDED(device.SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE));
}

}

WaterReflectionImage::~WaterReflectionImage()
{
	if (m_texture)
		static_cast<IDirect3DTexture9 *>(m_texture)->Release();
}

bool WaterReflectionImage::prepare(void *deviceHandle, void *sourceHandle, const std::array<float, 4> &clipBounds)
{
	auto *device = static_cast<IDirect3DDevice9 *>(deviceHandle);
	auto *source = static_cast<IDirect3DTexture9 *>(sourceHandle);
	if (!device || !source || !(clipBounds[0] < clipBounds[2]) || !(clipBounds[1] < clipBounds[3]))
		return false;

	D3DSURFACE_DESC description{};
	if (FAILED(source->GetLevelDesc(0, &description)))
		return false;
	auto *texture = static_cast<IDirect3DTexture9 *>(m_texture);
	if (!texture)
	{
		if (FAILED(device->CreateTexture(description.Width, description.Height, 1, D3DUSAGE_RENDERTARGET,
			description.Format, D3DPOOL_DEFAULT, &texture, nullptr)))
			return false;
		m_texture = texture;
	}

	IDirect3DSurface9 *surface = nullptr;
	if (FAILED(texture->GetSurfaceLevel(0, &surface)))
		return false;
	const Com<IDirect3DSurface9> destination(surface);
	if (FAILED(device->GetRenderTarget(0, &surface)))
		return false;
	const Com<IDirect3DSurface9> oldColor(surface);
	surface = nullptr;
	const HRESULT depth = device->GetDepthStencilSurface(&surface);
	if (FAILED(depth) && depth != D3DERR_NOTFOUND)
		return false;
	const Com<IDirect3DSurface9> oldDepth(surface);
	D3DVIEWPORT9 oldViewport{};
	if (FAILED(device->GetViewport(&oldViewport)))
		return false;
	IDirect3DStateBlock9 *states = nullptr;
	if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &states)))
		return false;
	const Com<IDirect3DStateBlock9> saved(states);

	const float width = static_cast<float>(description.Width);
	const float height = static_cast<float>(description.Height);
	const float left = (clipBounds[0] + 1.0f) * width * 0.5f - 0.5f;
	const float top = (clipBounds[1] + 1.0f) * height * 0.5f - 0.5f;
	const float right = (clipBounds[2] + 1.0f) * width * 0.5f - 0.5f;
	const float bottom = (clipBounds[3] + 1.0f) * height * 0.5f - 0.5f;
	const Vertex quad[] = {
		{left, top, 0.0f, 1.0f, 0.0f, 1.0f}, {right, top, 0.0f, 1.0f, 1.0f, 1.0f},
		{left, bottom, 0.0f, 1.0f, 0.0f, 0.0f}, {right, bottom, 0.0f, 1.0f, 1.0f, 0.0f}
	};
	const D3DVIEWPORT9 viewport{0, 0, description.Width, description.Height, 0.0f, 1.0f};
	const bool copied = SUCCEEDED(device->SetDepthStencilSurface(nullptr)) &&
		SUCCEEDED(device->SetRenderTarget(0, destination.get())) &&
		SUCCEEDED(device->SetViewport(&viewport)) &&
		configureCopy(*device, source) &&
		SUCCEEDED(device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xffffffffu, 1.0f, 0)) &&
		SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(Vertex)));

	const bool colorRestored = SUCCEEDED(device->SetRenderTarget(0, oldColor.get()));
	const bool depthRestored = SUCCEEDED(device->SetDepthStencilSurface(oldDepth.get()));
	const bool stateRestored = SUCCEEDED(saved->Apply());
	const bool viewportRestored = SUCCEEDED(device->SetViewport(&oldViewport));
	return copied && colorRestored && depthRestored && stateRestored && viewportRestored;
}

#else

WaterReflectionImage::~WaterReflectionImage() = default;
bool WaterReflectionImage::prepare(void *, void *, const std::array<float, 4> &) { return false; }

#endif

}
