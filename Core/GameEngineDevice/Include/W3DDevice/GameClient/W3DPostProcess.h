/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// FILE: W3DPostProcess.h /////////////////////////////////////////////////////
// GeneralsX @feature fbraz3 08/10/2026 Post-processing pipeline manager for GeneralsX:
//              Offscreen scene color target management, bloom bright-pass
//              extraction & separable Gaussian blur, HDR tone mapping, and
//              FXAA edge antialiasing before UI rendering.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/SubsystemInterface.h"
#include "Common/AsciiString.h"
#include "WW3D2/dx8wrapper.h"

#include <memory>
#include <vector>

class CameraClass;
class RTS3DScene;

namespace RetailFX
{
class LookupPass;
}

class W3DPostProcess : public SubsystemInterface, public DX8_CleanupHook
{
public:
	W3DPostProcess();
	virtual ~W3DPostProcess();

	virtual void init() override;
	virtual void reset() override;
	virtual void update() override;

	// DX8_CleanupHook interface
	virtual void ReleaseResources() override;
	virtual void ReAcquireResources() override {}

	void releaseResources();
	bool reacquireResources();

	// Called in W3DDisplay::draw() right before 3D world rendering begins
	bool beginScene();

	// Called in W3DDisplay::draw() right after drawViews(), before TheInGameUI->DRAW()
	bool endSceneAndApply();
	void applyRetailGlow(RTS3DScene &scene, CameraClass &camera);

	bool isPostProcessActive() const { return m_isSceneActive; }

private:
	struct RetailGlowConfig
	{
		Bool enabled = FALSE;
		Int diameter = 17;
		Real intensity = 1.0f;
		Int textureWidth = 256;
		Real radiusScale1 = 0.0f;
		Real amplitude1 = 0.0f;
		Real radiusScale2 = 0.0f;
		Real amplitude2 = 0.0f;
	};

	struct RetailGlowTap
	{
		Real offset = 0.0f;
		Real weight = 0.0f;
	};

	bool initShaders();
	bool prepareRetailLookup();
	void releaseRetailLookup();
	void loadRetailGlowConfig(const D3DCAPS8 &caps);
	void buildRetailGlowKernel();
	bool ensureRetailGlowResources(D3DFORMAT depthFormat);
	void releaseRetailGlowResources();
	bool renderGlowBlurPass(IDirect3DTexture8 *srcTex, IDirect3DSurface8 *dstSurf, bool vertical);
	void renderQuad(IDirect3DTexture8 *tex0, DWORD pixelShader, int width, int height,
	                IDirect3DTexture8 *tex1 = nullptr, const D3DVIEWPORT8 *glowViewport = nullptr);
	void renderQuad4Tap(IDirect3DTexture8 *tex, DWORD pixelShader, int width, int height,
	                    float offsetU, float offsetV);

	bool m_initialized;
	bool m_isSceneActive;

	int m_sceneWidth;
	int m_sceneHeight;
	int m_bloomWidth;
	int m_bloomHeight;

	IDirect3DTexture8 *m_sceneTexture;
	IDirect3DSurface8 *m_sceneSurface;
	IDirect3DSurface8 *m_sceneDepthSurface;

	IDirect3DTexture8 *m_bloomTexture[2];
	IDirect3DSurface8 *m_bloomSurface[2];

	IDirect3DTexture8 *m_compositeTexture;
	IDirect3DSurface8 *m_compositeSurface;

	IDirect3DSurface8 *m_savedRenderTarget;
	IDirect3DSurface8 *m_savedDepthBuffer;
	D3DVIEWPORT8 m_savedViewport;

	DWORD m_psCopy;
	DWORD m_psBloomExtract;
	DWORD m_psBloomBlur4Tap;
	DWORD m_psBloomComposite;
	DWORD m_psBloomCompositeHDR;
	DWORD m_psToneMapOnly;
	DWORD m_psFXAA;
	DWORD m_psRetailLookup = 0;
	IDirect3DTexture8 *m_retailLookup = nullptr;
	std::unique_ptr<RetailFX::LookupPass> m_nativeRetailLookup;
	AsciiString m_retailLookupName;
	Real m_retailLookupBlend = 0.0f;
	bool m_tracedRetailLookup = false;
	bool m_retailGlowLoaded = false;
	bool m_tracedRetailGlow = false;
	RetailGlowConfig m_retailGlow;
	std::vector<RetailGlowTap> m_retailGlowTaps;
	IDirect3DTexture8 *m_glowTexture[2] = { nullptr, nullptr };
	IDirect3DSurface8 *m_glowSurface[2] = { nullptr, nullptr };
	IDirect3DSurface8 *m_glowDepthSurface = nullptr;
	DWORD m_psGlowBlur4Tap = 0;
};

extern W3DPostProcess *TheW3DPostProcess;
