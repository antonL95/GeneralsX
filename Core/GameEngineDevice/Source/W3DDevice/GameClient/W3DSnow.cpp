/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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

// FILE: W3DSnow.h /////////////////////////////////////////////////////////

#include "W3DDevice/GameClient/W3DSnow.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
#include "GameClient/View.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WW3D2/assetmgr.h"
#if !RTS_GENERALS
#include "GameLogic/GameLogic.h"
#include <cstdio>
#include <cstdlib>
#endif
#include <algorithm>
#include <cmath>
#include <limits>



#define D3DFVF_POINTVERTEX (D3DFVF_XYZ)
#define SNOW_BUFFER_SIZE 4096	//size of vertex buffer holding particles.
#define SNOW_BATCH_SIZE	2048	//we render at most this many particles per drawprimitive call.  This number * 6 must be less than 65536 to fit into index buffer.

static constexpr Int RETAIL_CULL_BLOCK_SIZE = 128;

static const Vector3 QUAD_CORNERS[4] = {
	Vector3(-0.5f, 0.5f, 0.0f),
	Vector3(-0.5f, -0.5f, 0.0f),
	Vector3(0.5f, -0.5f, 0.0f),
	Vector3(0.5f, 0.5f, 0.0f)
};

static const Vector2 QUAD_UVS[4] = {
	Vector2(0.0f, 0.0f),
	Vector2(0.0f, 1.0f),
	Vector2(1.0f, 1.0f),
	Vector2(1.0f, 0.0f)
};

static void writeSnowQuad(VertexFormatXYZNDUV2 *&verts, const Vector3 &center, Real size)
{
	for (Int i = 0; i < 4; i++)
	{
		*(Vector3 *)verts = center + QUAD_CORNERS[i] * size;
		verts->nx = 0;
		verts->ny = 0;
		verts->nz = 0;
		verts->diffuse = 0xffffffff;
		verts->u1 = QUAD_UVS[i].X;
		verts->v1 = QUAD_UVS[i].Y;
		verts->u2 = 0;
		verts->v2 = 0;
		verts++;
	}
}

struct POINTVERTEX
{
    Vector3 v;	//center of particle.
};

W3DSnowManager::W3DSnowManager()
{
	m_indexBuffer=nullptr;
	m_snowTexture=nullptr;
	m_VertexBufferD3D=nullptr;
}

W3DSnowManager::~W3DSnowManager()
{
	ReleaseResources();
}

void W3DSnowManager::init()
{
	SnowManager::init();
	ReAcquireResources();
}

/** Releases all W3D/D3D assets before a reset.. */
void W3DSnowManager::ReleaseResources()
{
	REF_PTR_RELEASE(m_snowTexture);

	if (m_VertexBufferD3D)
		m_VertexBufferD3D->Release();

	m_VertexBufferD3D=nullptr;

	REF_PTR_RELEASE(m_indexBuffer);
}

/** (Re)allocates all W3D/D3D assets after a reset.. */
Bool W3DSnowManager::ReAcquireResources()
{
	ReleaseResources();

	if (!isPrecipitationEnabled())
		return TRUE;

	if (TheWeatherSetting->m_usePointSprites && DX8Wrapper::Get_Current_Caps()->Support_PointSprites())
	{
		LPDIRECT3DDEVICE8 m_pDev=DX8Wrapper::_Get_D3D_Device8();

		DEBUG_ASSERTCRASH(m_pDev, ("Trying to ReAcquireResources on W3DSnowManager without device"));

		if (m_VertexBufferD3D == nullptr)
		{
			// Create vertex buffer

			if (FAILED(m_pDev->CreateVertexBuffer
			(
				SNOW_BUFFER_SIZE*sizeof(POINTVERTEX),
				D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC|D3DUSAGE_POINTS,
				D3DFVF_POINTVERTEX,
				D3DPOOL_DEFAULT,
				&m_VertexBufferD3D
			)))
				return FALSE;
		}
	}
	else
	{
		m_indexBuffer=NEW_REF(DX8IndexBufferClass,(SNOW_BATCH_SIZE *6));	//allocate 2 triangles per flake, each with 3 indices.

		// Fill up the IB with static vertex indices that will be used for all smudges.
		{
			DX8IndexBufferClass::WriteLockClass lockIdxBuffer(m_indexBuffer);
			UnsignedShort *ib=lockIdxBuffer.Get_Index_Array();
			//quad of 4 triangles:
			//	0-----3
			//  |\   /|
			//  |  X  |
			//	|/   \|
			//  1-----2
			Int vbCount=0;
			for (Int i=0; i<SNOW_BATCH_SIZE; i++)
			{
				//Top
				ib[0]=vbCount+3;
				ib[1]=vbCount;
				ib[2]=vbCount+2;
				//Bottom
				ib[3]=vbCount+2;
				ib[4]=vbCount;
				ib[5]=vbCount+1;

				vbCount += 4;
				ib+=6;
			}
		}
	}

	m_snowTexture = WW3DAssetManager::Get_Instance()->Get_Texture(TheWeatherSetting->m_snowTexture.str());

	m_dwBase = SNOW_BUFFER_SIZE;
	m_dwDiscard = SNOW_BUFFER_SIZE;
	m_dwFlush = SNOW_BATCH_SIZE;

	return TRUE;
}

void W3DSnowManager::updateIniSettings()
{
	//Call base class
	SnowManager::updateIniSettings();

	if (m_retailWeather)
	{
		m_heightTraveled = 0.0f;
		ReleaseResources();
		return;
	}

	if (m_snowTexture && stricmp(m_snowTexture->Get_Texture_Name(),TheWeatherSetting->m_snowTexture.str()) != 0)
	{
		REF_PTR_RELEASE(m_snowTexture);
		m_snowTexture = WW3DAssetManager::Get_Instance()->Get_Texture(TheWeatherSetting->m_snowTexture.str());
	}
}

void W3DSnowManager::reset()
{
	SnowManager::reset();
	if (m_retailWeather)
		m_heightTraveled = 0.0f;
}

void W3DSnowManager::update()
{
	if (m_retailWeather)
	{
		updateRetailSpellRain();
		if (isPrecipitationEnabled())
			m_heightTraveled = std::fmod(m_heightTraveled + WW3D::Get_Logic_Frame_Time_Seconds() * m_velocity, m_boxDimensions);
		return;
	}

	// TheSuperHackers @tweak The snow render update is now decoupled from the logic step.
	m_time += WW3D::Get_Logic_Frame_Time_Seconds();

	//find current time offset, adjusting for overflow
	m_time=fmod(m_time,m_fullTimePeriod);
}

#define MAXIMUM_CAMERA_DISTANCE 100000	//maximum distance of camera position from world origin.
#define ISPOW2(x)  (x && (x & (x-1)) == 0)	//is a number a power of 2?
#define MODPOW2(x,y) ((x) & (y-1))		//mod '%' operator for powers of 2.

/*Recursively subdivide the large snow box enclosing the camera until we reach some predefined leaf size.  This
method is used so that very few off-screen particles end up getting rendered.  Culling them individually would
be too expensive since we're dealing with 1000's for this effect.*/
void W3DSnowManager::renderSubBox(RenderInfoClass &rinfo, Int originX, Int originY, Int cubeDimX, Int cubeDimY )
{
	//check if this box is too large and needs subdivision
	Int boxDimX=cubeDimX - originX;
	Int boxDimY=cubeDimY - originY;
	Int halfX=REAL_TO_INT_CEIL(boxDimX*0.5f);
	Int halfY=REAL_TO_INT_CEIL(boxDimY*0.5f);

	CameraClass &camera=rinfo.Camera;
	MinMaxAABoxClass mmbox;

	if (boxDimX > m_leafDim)
	{
		//subdivide the box
		if (boxDimY > m_leafDim)
		{
			//subdivide in both directions
			//Upper left
			mmbox.MinCorner.Set(originX*m_emitterSpacing-m_cullOverscan, (originY + halfY)*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set((originX + halfX)*m_emitterSpacing+m_cullOverscan, cubeDimY*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX, originY + halfY, originX + halfX, cubeDimY);
			//Upper right
			mmbox.MinCorner.Set((originX + halfX)*m_emitterSpacing-m_cullOverscan, (originY + halfY)*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set(cubeDimX*m_emitterSpacing+m_cullOverscan, cubeDimY*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX + halfX, originY + halfY,cubeDimX, cubeDimY);
			//Lower left
			mmbox.MinCorner.Set(originX*m_emitterSpacing-m_cullOverscan, originY*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set((originX + halfX)*m_emitterSpacing+m_cullOverscan, (originY + halfY)*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX,originY,originX + halfX, originY + halfY);
			//Lower right
			mmbox.MinCorner.Set((originX + halfX)*m_emitterSpacing-m_cullOverscan, originY*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set(cubeDimX*m_emitterSpacing+m_cullOverscan, (originY + halfY)*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX + halfX, originY, cubeDimX, originY + halfY);
			return;
		}
		else
		{
			//only subdivide in x direction.
			//Left
			mmbox.MinCorner.Set(originX*m_emitterSpacing-m_cullOverscan, originY*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set((originX + halfX)*m_emitterSpacing+m_cullOverscan, cubeDimY*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX, originY, originX + halfX, cubeDimY);
			//Right
			mmbox.MinCorner.Set((originX + halfX)*m_emitterSpacing-m_cullOverscan, originY*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set(cubeDimX*m_emitterSpacing+m_cullOverscan, cubeDimY*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX + halfX, originY, cubeDimX, cubeDimY);
			return;
		}
	}
	else
	if (boxDimY > m_leafDim)
	{
		//only subdivide in y direction
		//Top
		mmbox.MinCorner.Set(originX*m_emitterSpacing-m_cullOverscan, (originY+halfY)*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
		mmbox.MaxCorner.Set(cubeDimX*m_emitterSpacing+m_cullOverscan, cubeDimY*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
		if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
			renderSubBox(rinfo, originX, originY+halfY,cubeDimX, cubeDimY);
		//Bottom
		mmbox.MinCorner.Set(originX*m_emitterSpacing-m_cullOverscan, originY*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
		mmbox.MaxCorner.Set(cubeDimX*m_emitterSpacing+m_cullOverscan, (originY + halfY)*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
		if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
			renderSubBox(rinfo, originX, originY, cubeDimX, originY + halfY);
		return;
	}

	//Box too small to subdivide so render it.

	//Find total number of particles that need rendering.
	Int totalPart=(cubeDimY-originY)*(cubeDimX-originX);

	if (!totalPart)
		return;	//nothing to render.

	Int y=originY;	//loop counter.
	Int cubeOriginXRemainder = originX;	//loop counter - adjusted when not all particles fit into render buffer.
	Vector3 snowCenter;

	m_totalRendered += totalPart;

	while (totalPart)
	{
		Int batchSize=totalPart;

		if (batchSize > m_dwFlush)
			batchSize = m_dwFlush;

		if((m_dwBase + batchSize) > m_dwDiscard)
			m_dwBase = 0;

		POINTVERTEX* verts;

		if(m_VertexBufferD3D->Lock(m_dwBase * sizeof(POINTVERTEX), batchSize * sizeof(POINTVERTEX),
			(unsigned char **) &verts, m_dwBase ? D3DLOCK_NOOVERWRITE : D3DLOCK_DISCARD) != D3D_OK )
			return;	//couldn't lock buffer.

		Int numberInBatch=0;

		for (;y<cubeDimY; y++)
		{
			for (Int x=cubeOriginXRemainder; x<cubeDimX; x++)
			{
				if (numberInBatch >= batchSize)
				{	cubeOriginXRemainder = x;
					goto flush_particles;
				}

				if (m_retailWeather)
				{
					*(Vector3 *)verts = retailParticleCenter(m_tileOrigin, x, y);
				}
				else
				{
					Int noiseOffset = MODPOW2(x + MAXIMUM_CAMERA_DISTANCE, SNOW_NOISE_X) + MODPOW2(y + MAXIMUM_CAMERA_DISTANCE, SNOW_NOISE_Y) * SNOW_NOISE_X;
					if (noiseOffset > SNOW_NOISE_X * SNOW_NOISE_Y)
						noiseOffset = 0;
					const Real h0 = m_snowCeiling - fmod(m_heightTraveled + m_startingHeights[noiseOffset], m_boxDimensions);
					snowCenter.Set(x * m_emitterSpacing, y * m_emitterSpacing, h0);
					snowCenter.X += m_amplitude * WWMath::Fast_Sin(h0 * m_frequencyScaleX + (Real)x);
					snowCenter.Y += m_amplitude * WWMath::Fast_Sin(h0 * m_frequencyScaleY + (Real)y);
					*(Vector3 *)verts = snowCenter;
				}
				verts++;

				numberInBatch++;
			}
			//getting here means we did not overflow the render buffer, so reset x origin to normal.
			cubeOriginXRemainder = originX;	//reset to normal amount
		}

flush_particles:
		m_VertexBufferD3D->Unlock();
		//Render any particles that may be queued up.
		if (numberInBatch)
		{
			Debug_Statistics::Record_DX8_Polys_And_Vertices(numberInBatch*2,numberInBatch*4,ShaderClass::_PresetOpaqueShader);
			DX8Wrapper::_Get_D3D_Device8()->DrawPrimitive( D3DPT_POINTLIST, m_dwBase, numberInBatch);
			totalPart -= numberInBatch;
			m_dwBase += numberInBatch;
		}
	}
}

void W3DSnowManager::render(RenderInfoClass &rinfo)
{
	m_totalRendered = 0;
	renderPrecipitation(rinfo);
#if !RTS_GENERALS
	static const Bool traceWeather = std::getenv("GENERALSX_TEST_WEATHER_DRAW") != nullptr;
	if (traceWeather && m_retailWeather && TheGameLogic)
	{
		std::fprintf(stderr, "TEST weather-draw frame=%u state=%d end=%u enabled=%d visible=%d map=%d spell=%d system=snow-manager count=%d texture=%s authored=%s spacing=%.1f speed=%.1f\n",
			TheGameLogic->getFrame(), TheGameLogic->getRetailWeather(), TheGameLogic->getRetailWeatherEndFrame(),
			isPrecipitationEnabled() && m_isVisible, m_isVisible, TheWeatherSetting->m_isSnowing, TheWeatherSetting->m_spellEnabled,
			m_totalRendered, m_snowTexture ? m_snowTexture->Get_Texture_Name().str() : "-",
			TheWeatherSetting->m_snowTexture.str(), m_emitterSpacing, m_velocity);
		std::fflush(stderr);
	}
#endif
}

void W3DSnowManager::renderPrecipitation(RenderInfoClass &rinfo)
{
	if (!isPrecipitationEnabled() || !m_isVisible)
		return;

	Int usePointSprites = DX8Wrapper::Get_Current_Caps()->Support_PointSprites() && TheWeatherSetting->m_usePointSprites;

	//make sure the noise table is powers of 2 in dimensions.
	WWASSERT(ISPOW2(SNOW_NOISE_X) && ISPOW2(SNOW_NOISE_Y));

	const FrustumClass &frustum = rinfo.Camera.Get_Frustum();
	AABoxClass bbox;

	//Get a bounding box around our visible universe.  Bounded by terrain and the sky
	//so much tighter fitting volume than what's actually visible.  This will cull
	//particles falling under the ground.

 	TheTerrainRenderObject->getMaximumVisibleBox(frustum, &bbox, TRUE);

	Int cubeOriginX = 0;
	Int cubeOriginY = 0;
	Int cubeDimX = 0;
	Int cubeDimY = 0;
	if (!m_retailWeather)
	{
		const Coord3D &cPos = TheTacticalView->get3DCameraPosition();
		const Int halfEmitters = (Int)floor(m_boxDimensions / m_emitterSpacing * 0.5f);
		const Int cubeCenterX = (Int)floor(cPos.x / m_emitterSpacing);
		const Int cubeCenterY = (Int)floor(cPos.y / m_emitterSpacing);
		cubeOriginX = cubeCenterX - halfEmitters;
		cubeOriginY = cubeCenterY - halfEmitters;
		cubeDimX = cubeCenterX + halfEmitters;
		cubeDimY = cubeCenterY + halfEmitters;

		bbox.Extent.X += m_amplitude + m_quadSize;
		bbox.Extent.Y += m_amplitude + m_quadSize;
		if (cubeOriginX * m_emitterSpacing < bbox.Center.X - bbox.Extent.X)
			cubeOriginX = (Int)floor((bbox.Center.X - bbox.Extent.X) / m_emitterSpacing);
		if (cubeOriginY * m_emitterSpacing < bbox.Center.Y - bbox.Extent.Y)
			cubeOriginY = (Int)floor((bbox.Center.Y - bbox.Extent.Y) / m_emitterSpacing);
		if (cubeDimX * m_emitterSpacing > bbox.Center.X + bbox.Extent.X)
			cubeDimX = (Int)floor((bbox.Center.X + bbox.Extent.X) / m_emitterSpacing);
		if (cubeDimY * m_emitterSpacing > bbox.Center.Y + bbox.Extent.Y)
			cubeDimY = (Int)floor((bbox.Center.Y + bbox.Extent.Y) / m_emitterSpacing);

		if (cubeDimX <= cubeOriginX || cubeDimY <= cubeOriginY)
			return;

		m_snowCeiling = cPos.z + m_boxDimensions / 2.0f;
		m_heightTraveled = m_time * m_velocity + fmod(cPos.z, m_boxDimensions);
	}
	else
	{
		m_snowCeiling = m_boxDimensions;
	}

	Matrix4x4 identity(true);
	DX8Wrapper::Set_Transform(D3DTS_WORLD,identity);

	DX8Wrapper::Set_Shader(ShaderClass::_PresetAlphaShader);

	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vmat);
	REF_PTR_RELEASE(vmat);

	//make sure we have all the resources we need
	if ((usePointSprites ? !m_VertexBufferD3D : !m_indexBuffer) && !ReAcquireResources())
		return;

	DX8Wrapper::Set_Texture(0,m_snowTexture);

	if (!usePointSprites)
	{
		if (m_retailWeather)
			renderRetailTiles(rinfo, bbox, FALSE);
		else
			renderAsQuads(rinfo, cubeOriginX, cubeOriginY, cubeDimX, cubeDimY);
		return;
	}

	Vector3 snowCenter;

	DX8Wrapper::Apply_Render_State_Changes();

    // Set the render states for using point sprites
	DX8Wrapper::Set_DX8_Render_State( D3DRS_POINTSPRITEENABLE, TRUE );
    DX8Wrapper::Set_DX8_Render_State( D3DRS_POINTSCALEENABLE,  TRUE );
    DX8Wrapper::Set_DX8_Render_State( D3DRS_POINTSIZE,     F2DW(m_pointSize) );
    DX8Wrapper::Set_DX8_Render_State( D3DRS_POINTSIZE_MIN, F2DW(m_minPointSize) );
    DX8Wrapper::Set_DX8_Render_State( D3DRS_POINTSIZE_MAX, F2DW(m_maxPointSize) );
    DX8Wrapper::Set_DX8_Render_State( D3DRS_POINTSCALE_A,  F2DW(0.00f) );
    DX8Wrapper::Set_DX8_Render_State( D3DRS_POINTSCALE_B,  F2DW(0.00f) );
    DX8Wrapper::Set_DX8_Render_State( D3DRS_POINTSCALE_C,  F2DW(1.00f) );

	DX8Wrapper::_Get_D3D_Device8()->SetStreamSource( 0, m_VertexBufferD3D, sizeof(POINTVERTEX) );
    DX8Wrapper::_Get_D3D_Device8()->SetVertexShader( D3DFVF_POINTVERTEX );
	m_dwBase = SNOW_BUFFER_SIZE;	//start with a new vertex buffer each frame.

	m_leafDim = m_retailWeather ? std::numeric_limits<Int>::max() : 45;

	//Particle centers can deviate from center by by amplitude of sine offset.  They also have radius m_quadSize.
	//Enlarge culling bounds to compensate.
	m_cullOverscan = m_amplitude+m_quadSize;
	if (m_retailWeather)
		renderRetailTiles(rinfo, bbox, TRUE);
	else
		renderSubBox(rinfo, cubeOriginX, cubeOriginY, cubeDimX, cubeDimY);

	// Reset render states
    DX8Wrapper::Set_DX8_Render_State( D3DRS_POINTSPRITEENABLE, FALSE );
    DX8Wrapper::Set_DX8_Render_State( D3DRS_POINTSCALEENABLE,  FALSE );

}

void W3DSnowManager::renderRetailTiles(RenderInfoClass &rinfo, const AABoxClass &bbox, Bool usePointSprites)
{
	WorldHeightMap *map = TheTerrainRenderObject->getMap();
	if (!map)
		return;

	const Int border = map->getBorderSize();
	const Int width = (map->getXExtent() - 2 * border) * MAP_XY_FACTOR;
	const Int height = (map->getYExtent() - 2 * border) * MAP_XY_FACTOR;
	const Int tiles = usePointSprites ? 1 : std::max(1, TheWeatherSetting->m_numberTiles);
	const Int tileWidth = width / tiles;
	const Int tileHeight = height / tiles;
	const Int spacing = static_cast<Int>(m_emitterSpacing);
	const Int columns = tileWidth / spacing;
	const Int rows = tileHeight / spacing;
	if (columns <= 0 || rows <= 0)
		return;

	const Coord2D &wind = TheWeatherSetting->m_snowXYSpeed;
	const Real margin = std::abs(m_amplitude) + m_quadSize;
	const Vector2 drift(margin + std::abs(wind.x * m_fullTimePeriod), margin + std::abs(wind.y * m_fullTimePeriod));
	const Real overscanX = bbox.Extent.X + drift.X;
	const Real overscanY = bbox.Extent.Y + drift.Y;
	const auto cell = [spacing](Real offset, Int count, Int padding) {
		const Real position = std::floor(offset / spacing) + padding;
		return static_cast<Int>(std::clamp(position, 0.0f, static_cast<Real>(count)));
	};

	m_retailBlocks.clear();
	for (Int ty = 0; ty < tiles; ++ty)
	{
		const Int originY = ty * tileHeight;
		const Int firstY = cell(bbox.Center.Y - overscanY - originY, rows, 0);
		const Int lastY = cell(bbox.Center.Y + overscanY - originY, rows, 1);
		if (lastY <= firstY)
			continue;

		for (Int tx = 0; tx < tiles; ++tx)
		{
			const Int originX = tx * tileWidth;
			const Int firstX = cell(bbox.Center.X - overscanX - originX, columns, 0);
			const Int lastX = cell(bbox.Center.X + overscanX - originX, columns, 1);
			if (lastX <= firstX)
				continue;

			m_tileOrigin.Set(static_cast<Real>(originX), static_cast<Real>(originY), 0.0f);
			if (usePointSprites)
				renderSubBox(rinfo, firstX, firstY, lastX, lastY);
			else
				collectRetailBlocks(rinfo, m_tileOrigin, firstX, firstY, lastX, lastY, drift);
		}
	}

	if (!usePointSprites)
		renderRetailQuads(rinfo);
}

void W3DSnowManager::collectRetailBlocks(RenderInfoClass &rinfo, const Vector3 &origin, Int firstX, Int firstY, Int lastX, Int lastY, const Vector2 &margin)
{
	const FrustumClass &frustum = rinfo.Camera.Get_Frustum();
	const Int spacing = static_cast<Int>(m_emitterSpacing);
	const Int step = (RETAIL_CULL_BLOCK_SIZE + spacing - 1) / spacing;
	const Real bottom = m_snowCeiling - m_boxDimensions - m_quadSize;
	const Real top = m_snowCeiling + m_quadSize;

	for (Int y = firstY; y < lastY; y += step)
	{
		const Int endY = std::min(y + step, lastY);
		for (Int x = firstX; x < lastX; x += step)
		{
			const Int endX = std::min(x + step, lastX);
			const Vector3 low(origin.X + x * spacing - margin.X, origin.Y + y * spacing - margin.Y, bottom);
			const Vector3 high(origin.X + (endX - 1) * spacing + margin.X, origin.Y + (endY - 1) * spacing + margin.Y, top);
			const AABoxClass box((low + high) * 0.5f, (high - low) * 0.5f);
			if (CollisionMath::Overlap_Test(frustum, box) != CollisionMath::OUTSIDE)
				m_retailBlocks.push_back(RetailBlock{origin, x, y, endX, endY});
		}
	}
}

Vector3 W3DSnowManager::retailParticleCenter(const Vector3 &origin, Int x, Int y) const
{
	const Int worldX = x * static_cast<Int>(m_emitterSpacing);
	const Int worldY = y * static_cast<Int>(m_emitterSpacing);
	const Int noiseOffset = MODPOW2(worldX + SNOW_NOISE_X / 2, SNOW_NOISE_X)
		+ MODPOW2(worldY + SNOW_NOISE_Y / 2, SNOW_NOISE_Y) * SNOW_NOISE_X;
	const Real initialHeight = m_startingHeights[noiseOffset];
	const Real fall = initialHeight + m_heightTraveled;
	const Real phase = fall - static_cast<Real>(floor(fall / m_boxDimensions)) * m_boxDimensions;
	const Real z = m_snowCeiling - phase;
	const Real driftTime = m_velocity != 0.0f ? (phase - initialHeight) / m_velocity : 0.0f;
	const Coord2D &wind = TheWeatherSetting->m_snowXYSpeed;
	return Vector3(
		origin.X + worldX + wind.x * driftTime + m_amplitude * WWMath::Fast_Sin(z * m_frequencyScaleX + worldX),
		origin.Y + worldY + wind.y * driftTime + m_amplitude * WWMath::Fast_Sin(z * m_frequencyScaleY + worldY),
		z);
}

/**For hardware that doesn't support point sprites*/
void W3DSnowManager::renderAsQuads(RenderInfoClass &rinfo, Int cubeOriginX, Int cubeOriginY, Int cubeDimX, Int cubeDimY)
{

	Matrix4x4 proj;
	Matrix3D view;
	Vector3 snowCenter;
	Vector3 snowCenterVS;

	CameraClass &camera=rinfo.Camera;

	camera.Get_View_Matrix(&view);
	camera.Get_Projection_Matrix(&proj);

	Vector3 vertex_offsets[4] = {
		Vector3(-0.5f, 0.5f, 0.0f),
		Vector3(-0.5f, -0.5f, 0.0f),
		Vector3(0.5f, -0.5f, 0.0f),
		Vector3(0.5f, 0.5f, 0.0f)
	};

	Vector2 quad_uvs[4] = {
		Vector2(0.0f, 0.0f),
		Vector2(0.0f, 1.0f),
		Vector2(1.0f, 1.0f),
		Vector2(1.0f, 0.0f)
	};


	//pre-multiple the offsets by particle size
	for (Int i=0; i<4; i++)
	{
		vertex_offsets[i] *= m_quadSize;
	}

	Matrix4x4 identity(true);
	DX8Wrapper::Set_Transform(D3DTS_VIEW,identity);

	DX8Wrapper::Set_Index_Buffer(m_indexBuffer,0);

	Int y=cubeOriginY;	//loop counter.
	Int cubeOriginXRemainder = cubeOriginX;	//loop counter - adjusted when not all particles fit into render buffer.

	//Find total number of particles that need rendering.
	Int totalPart=(cubeDimY-cubeOriginY)*(cubeDimX-cubeOriginX);

	m_totalRendered += totalPart;

	while (totalPart)
	{
		Int batchSize=totalPart;

		if (batchSize > SNOW_BATCH_SIZE)
			batchSize = SNOW_BATCH_SIZE;

		Int numberInBatch=0;

		DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC_DX8,dynamic_fvf_type,batchSize*4);	//allocate 4 verts per flake
		{
			DynamicVBAccessClass::WriteLockClass lock(&vb_access);
			VertexFormatXYZNDUV2* verts=lock.Get_Formatted_Vertex_Array();

			for (;y<cubeDimY; y++)
			{
				for (Int x=cubeOriginXRemainder; x<cubeDimX; x++)
				{
					if (numberInBatch >= batchSize)
					{	cubeOriginXRemainder = x;
						goto flush_particles;
					}

					Int noiseOffset = MODPOW2(x + MAXIMUM_CAMERA_DISTANCE, SNOW_NOISE_X) + MODPOW2(y + MAXIMUM_CAMERA_DISTANCE, SNOW_NOISE_Y) * SNOW_NOISE_X;
					if (noiseOffset > SNOW_NOISE_X * SNOW_NOISE_Y)
						noiseOffset = 0;
					const Real h0 = m_snowCeiling - fmod(m_heightTraveled + m_startingHeights[noiseOffset], m_boxDimensions);
					snowCenter.Set(x * m_emitterSpacing, y * m_emitterSpacing, h0);
					Matrix3D::Transform_Vector(view, snowCenter, &snowCenterVS);
					snowCenterVS.X += m_amplitude * WWMath::Fast_Sin(h0 * m_frequencyScaleX + (Real)x);
					snowCenterVS.Y += m_amplitude * WWMath::Fast_Sin(h0 * m_frequencyScaleY + (Real)y);

					for (Int i=0; i<4; i++)
					{
						*(Vector3 *)verts=snowCenterVS + vertex_offsets[i];
						verts->nx=0;	//keep AGP write-combining active
						verts->ny=0;
						verts->nz=0;
						verts->diffuse=0xffffffff;	//set to opaque
						verts->u1=quad_uvs[i].X;
						verts->v1=quad_uvs[i].Y;
						verts->u2=0;	//keep AGP write-combining active
						verts->v2=0;
						verts++;
					}

					numberInBatch++;
				}
				//getting here means we did not overflow the render buffer, so reset x origin to normal.
				cubeOriginXRemainder = cubeOriginX;	//reset to normal amount
			}
flush_particles:
			numberInBatch;	//need something at goto destination - stupid c compiler.
		}

		//Render any particles that may be queued up.
		if (numberInBatch)
		{
			DX8Wrapper::Set_Vertex_Buffer(vb_access);
			DX8Wrapper::Draw_Triangles(	0,numberInBatch*2, 0, numberInBatch*4);
			totalPart -= numberInBatch;
		}
	}
}

void W3DSnowManager::renderRetailQuads(RenderInfoClass &rinfo)
{
	Int remaining = 0;
	for (const RetailBlock &block : m_retailBlocks)
		remaining += (block.lastX - block.firstX) * (block.lastY - block.firstY);

	m_totalRendered += remaining;
	if (remaining == 0)
		return;

	Matrix3D view;
	rinfo.Camera.Get_View_Matrix(&view);

	Matrix4x4 identity(true);
	DX8Wrapper::Set_Transform(D3DTS_VIEW, identity);
	DX8Wrapper::Set_Index_Buffer(m_indexBuffer, 0);

	size_t blockIndex = 0;
	Int x = m_retailBlocks[0].firstX;
	Int y = m_retailBlocks[0].firstY;

	while (remaining > 0)
	{
		const Int batchSize = std::min(remaining, SNOW_BATCH_SIZE);
		DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC_DX8, dynamic_fvf_type, batchSize * 4);
		{
			DynamicVBAccessClass::WriteLockClass lock(&vb_access);
			VertexFormatXYZNDUV2 *verts = lock.Get_Formatted_Vertex_Array();

			for (Int written = 0; written < batchSize; ++written)
			{
				const RetailBlock &block = m_retailBlocks[blockIndex];
				Vector3 centerVS;
				Matrix3D::Transform_Vector(view, retailParticleCenter(block.origin, x, y), &centerVS);
				writeSnowQuad(verts, centerVS, m_quadSize);

				if (++x < block.lastX)
					continue;
				x = block.firstX;
				if (++y < block.lastY)
					continue;
				if (++blockIndex < m_retailBlocks.size())
				{
					x = m_retailBlocks[blockIndex].firstX;
					y = m_retailBlocks[blockIndex].firstY;
				}
			}
		}

		DX8Wrapper::Set_Vertex_Buffer(vb_access);
		DX8Wrapper::Draw_Triangles(0, batchSize * 2, 0, batchSize * 4);
		remaining -= batchSize;
	}
}
