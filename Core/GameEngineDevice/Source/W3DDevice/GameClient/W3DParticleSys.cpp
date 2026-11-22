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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// W3DParticleSys.cpp
// W3D Particle System implementation
// Author: Michael S. Booth, November 2001

#include "Common/GlobalData.h"
#include "Common/RetailFileLayers.h"
#include "GameClient/Color.h"
#include "GameClient/GameClient.h"
#include "W3DDevice/GameClient/RetailEffectInput.h"
#include "W3DDevice/GameClient/W3DParticleSys.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/W3DRetailGpuParticles.h"
#include "W3DDevice/GameClient/W3DSmudge.h"
#include "W3DDevice/GameClient/W3DSnow.h"
#include "W3DDevice/GameClient/W3DTerrainParticles.h"
#include "WW3D2/camera.h"
#include "WW3D2/dx8renderer.h"
#include "WW3D2/rendobj.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
void setModelParticleTint(RenderObjClass& model, unsigned tint, bool additive)
{
	model.Set_Particle_Tint(tint, additive);
	for (int i = 0; i < model.Get_Num_Sub_Objects(); ++i)
	{
		auto* child = model.Get_Sub_Object(i);
		if (child)
		{
			setModelParticleTint(*child, tint, additive);
			child->Release_Ref();
		}
	}
}

void renderModelParticles(RenderInfoClass& info, ParticleSystemManager::ParticleSystemList& systems, const AABoxClass& bounds)
{
	if (!W3DDisplay::m_assetManager)
		return;
	std::vector<RenderObjClass*> drawn;
	for (ParticleSystem* system : systems)
	{
		if (!system || !system->isUsingDrawables() || system->getParticleTypeName().isEmpty())
			continue;
		const bool opacityOnly = system->getTemplate() && system->getTemplate()->usesModelOpacityOnly();
		std::string modelName(system->getParticleTypeName().str());
		if (modelName.size() > 4 && stricmp(modelName.c_str() + modelName.size() - 4, ".w3d") == 0)
			modelName.resize(modelName.size() - 4);
		for (Particle* particle = system->getFirstParticle(); particle; particle = particle->m_systemNext)
		{
			const auto& size = particle->getModelSize();
			const Real opacity = particle->getAlpha();
			if (!(size.x > 0.0f && size.y > 0.0f && size.z > 0.0f && opacity > 0.0f))
				continue;
			const auto* position = particle->getPosition();
			const Real margin = std::max({size.x, size.y, size.z}) * 50.0f;
			if (WWMath::Fabs(position->x - bounds.Center.X) > bounds.Extent.X + margin ||
				WWMath::Fabs(position->y - bounds.Center.Y) > bounds.Extent.Y + margin ||
				WWMath::Fabs(position->z - bounds.Center.Z) > bounds.Extent.Z + margin)
				continue;
			auto* model = W3DDisplay::m_assetManager->Create_Render_Obj(modelName.c_str(), 1.0f, 0);
			if (!model)
				break;
			Matrix3D transform(true);
			if (const Matrix3D* target = system->getRetailTargetMatrix())
				transform = *target;
			transform.Rotate_Z(particle->getAngle());
			transform.Scale(size.x, size.y, size.z);
			transform.Set_Translation(Vector3(position->x, position->y, position->z));
			model->Set_Transform(transform);
			model->Set_Animation_Opacity(opacity);
			unsigned tint = 0xFFFFFFFF;
			if (!opacityOnly)
			{
				const auto* color = particle->getColor();
				const auto channel = [](Real value) { return static_cast<unsigned>(std::clamp(value, 0.0f, 1.0f) * 255.0f); };
				tint = 0xFF000000 | channel(color->red) << 16 | channel(color->green) << 8 | channel(color->blue);
			}
			setModelParticleTint(*model, tint, system->getShaderType() == ParticleSystemInfo::ADDITIVE);
			model->Render(info);
			drawn.push_back(model);
		}
	}
	if (!drawn.empty())
		TheDX8MeshRenderer.Flush();
	for (auto* model : drawn)
		model->Release_Ref();
}
}


//------------------------------------------------------------------------------ Performance Timers
//#include "Common/PerfMetrics.h"
//#include "Common/PerfTimer.h"

//-------------------------------------------------------------------------------------------------


W3DParticleSystemManager::W3DParticleSystemManager()
{
	m_batchParticleAlignment = ParticleSystemInfo::PARTICLE_ALIGNMENT_BILLBOARD;
	m_batchShaderType = ParticleSystemInfo::INVALID_SHADER;

	m_pointGroup = nullptr;
	m_streakLine = nullptr;
	m_terrainParticles = nullptr;
	m_posBuffer = nullptr;
	m_RGBABuffer = nullptr;
	m_sizeBuffer = nullptr;
	m_angleBuffer = nullptr;
	m_readyToRender = false;

	m_onScreenParticleCount = 0;

	m_pointGroup = NEW PointGroupClass();
	//m_streakLine = nullptr;
	m_streakLine = NEW StreakLineClass();
	m_terrainParticles = NEW W3DTerrainParticles();

	m_posBuffer = NEW_REF( ShareBufferClass<Vector3>, (MAX_POINTS_PER_GROUP, "W3DParticleSystemManager::m_posBuffer") );
	m_RGBABuffer = NEW_REF( ShareBufferClass<Vector4>, (MAX_POINTS_PER_GROUP, "W3DParticleSystemManager::m_RGBABuffer") );
	m_sizeBuffer = NEW_REF( ShareBufferClass<float>, (MAX_POINTS_PER_GROUP, "W3DParticleSystemManager::m_sizeBuffer") );
	m_angleBuffer = NEW_REF( ShareBufferClass<uint8>, (MAX_POINTS_PER_GROUP, "W3DParticleSystemManager::m_angleBuffer") );
}

W3DParticleSystemManager::~W3DParticleSystemManager()
{
	delete m_pointGroup;

//	W3DDisplay::m_3DScene->Remove_Render_Object( m_streakLine );

	if (m_streakLine)
	{
		REF_PTR_RELEASE(m_streakLine);
	}

	delete m_terrainParticles;
	m_terrainParticles = nullptr;

	REF_PTR_RELEASE(m_posBuffer);
	REF_PTR_RELEASE(m_RGBABuffer);
	REF_PTR_RELEASE(m_sizeBuffer);
	REF_PTR_RELEASE(m_angleBuffer);
}

/**
 * Hack because DoParticles is called from Flush(), which is called
 * multiple times per frame.  We only want to render once.
 * @todo Clean up the flag/Flush hack.
 */
void W3DParticleSystemManager::queueParticleRender()
{
	m_readyToRender = true;
}

UnsignedInt W3DParticleSystemManager::drawGpuParticles(ParticleSystem &system)
{
	const auto *definition = system.getTemplate();
	const auto *first = system.getFirstParticle();
	if (!definition->canDrawGpuParticles() || !first || !W3DDisplay::m_assetManager)
		return 0;
	const auto& name = definition->getName();
	if (name.compareNoCase("BarrageExplosion") == 0 || name.compareNoCase("BarrageExplosionFire") == 0)
		return 0;
	auto *device = DX8Wrapper::Get_Effect_Device9();
	if (!device)
		return 0;
	if (device != m_gpuEffectDevice)
	{
		m_gpuEffectDevice = device;
		m_gpuParticlePass.reset();
		if (const auto effect = RetailFX::loadEffectFile("shaders/compiled/gpuparticle.fxo"))
			m_gpuParticlePass = RetailFX::GpuParticlePass::make(device, *effect, DX8Wrapper::Create_Effect_Shader9);
	}
	if (!m_gpuParticlePass)
		return 0;
	RefCountPtr<TextureClass> texture;
	texture.Assign_No_Add_Ref(W3DDisplay::m_assetManager->Get_Texture(system.getParticleTypeName().str()));
	auto *atlas = RetailFX::nativeTexture(texture.Peek());
	auto *device8 = DX8Wrapper::_Get_D3D_Device8();
	D3DXMATRIX world, view, projection, inverseView, viewProjection;
	if (!atlas || FAILED(device8->GetTransform(D3DTS_VIEW, &view)) ||
		FAILED(device8->GetTransform(D3DTS_PROJECTION, &projection)) || !D3DXMatrixInverse(&inverseView, nullptr, &view))
		return 0;
	D3DXMatrixIdentity(&world);
	D3DXMatrixMultiply(&viewProjection, &view, &projection);
	const auto fog = RetailFX::fogBindings(device8);
	const auto settings = definition->getGpuDrawSettings(*first, system.getSizeMultiplier() * TheGlobalData->m_particleScale);
	const auto frame = TheGameClient->getFrame();
	const RetailFX::SetBinding setBinding = [&](const auto &binding, auto values, auto) {
		const auto copy = [&](const auto &registers) {
			std::copy_n(registers.begin(), std::min(registers.size(), values.size()), values.begin());
		};
		if (binding.name == "Draw") copy(settings.draw);
		else if (binding.name == "Update") copy(settings.update);
		else if (binding.name == "Physics") copy(settings.physics);
		else if (binding.name == "Fog") copy(fog);
		else if (binding.name == "World") RetailFX::writeMatrix(values, world);
		else if (binding.name == "View") RetailFX::writeMatrix(values, view);
		else if (binding.name == "ViewI") RetailFX::writeMatrix(values, inverseView);
		else if (binding.name == "WorldViewProjection") RetailFX::writeMatrix(values, viewProjection);
		else if (binding.name == "Time" && !values.empty()) values[0][0] = static_cast<Real>(frame) / 30.0f;
	};
	std::vector<RetailFX::GpuParticleVertex> vertices;
	std::vector<uint16_t> indices;
	const size_t capacity = std::min<UnsignedInt>(system.getParticleCount(), MAX_POINTS_PER_GROUP);
	vertices.reserve(capacity * 4);
	indices.reserve(capacity * 6);
	UnsignedInt drawn = 0;
	const auto draw = [&] {
		if (!indices.empty() && m_gpuParticlePass->draw(device, atlas, vertices, indices, setBinding))
			drawn += static_cast<UnsignedInt>(vertices.size() / 4);
		vertices.clear();
		indices.clear();
	};
	for (const Particle *particle = first; particle; particle = particle->m_systemNext)
	{
		if (!particle->m_lifetime)
			continue;
		const auto &position = particle->getGpuSpawnPosition();
		const auto &velocity = particle->getGpuSpawnVelocity();
		const auto base = static_cast<uint16_t>(vertices.size());
		for (unsigned corner = 0; corner < 4; ++corner)
		{
			vertices.push_back({
				{position.x, position.y, position.z, static_cast<Real>(particle->m_lifetime)},
				{velocity.x, velocity.y, velocity.z, static_cast<Real>(particle->getCreateTimestamp())},
				{particle->getGpuSeed(), static_cast<Real>(corner)}
			});
		}
		for (const unsigned index : {0, 2, 1, 0, 3, 2})
			indices.push_back(static_cast<uint16_t>(base + index));
		if (vertices.size() == MAX_POINTS_PER_GROUP * 4)
			draw();
	}
	draw();
	if (drawn && std::getenv("GENERALSX_TRACE_RETAIL_EFFECTS") &&
		std::find(m_tracedGpuTemplates.begin(), m_tracedGpuTemplates.end(), definition->getName()) == m_tracedGpuTemplates.end())
	{
		m_tracedGpuTemplates.push_back(definition->getName());
		std::fprintf(stderr, "[RetailFX] Drew GPUParticle Default (%s, %u)\n", definition->getName().str(), drawn);
	}
	return drawn;
}

UnsignedInt W3DParticleSystemManager::drawGpuFallback(RenderInfoClass &rinfo, ParticleSystem &system)
{
	const auto &definition = *system.getTemplate();
	const auto rows = definition.getGpuFramesPerRow();
	if (!system.getFirstParticle() || !W3DDisplay::m_assetManager || rows > 16 || !std::has_single_bit(rows))
		return 0;
	RefCountPtr<TextureClass> texture;
	texture.Assign_No_Add_Ref(W3DDisplay::m_assetManager->Get_Texture(system.getParticleTypeName().str()));
	if (!texture)
		return 0;
	if (!texture->Is_Initialized())
		texture->Init();
	if (texture->Is_Missing_Texture())
		return 0;
	if (!m_gpuFrameBuffer)
		m_gpuFrameBuffer.Assign_No_Add_Ref(NEW_REF(ShareBufferClass<uint8>, (MAX_POINTS_PER_GROUP, "GpuParticleFrames")));
	m_pointGroup->Set_Texture(texture.Peek());
	m_pointGroup->Set_Shader(system.getShaderType() == ParticleSystemInfo::ADDITIVE ?
		ShaderClass::_PresetAdditiveSpriteShader : ShaderClass::_PresetAlphaSpriteShader);
	m_pointGroup->Set_Flag(PointGroupClass::TRANSFORM, true);
	m_pointGroup->Set_Point_Mode(PointGroupClass::QUADS);
	m_pointGroup->Set_Billboard(true);
	m_pointGroup->Set_Frame_Row_Column_Count_Log2(static_cast<uint8>(std::countr_zero(rows)));
	const auto &bounds = m_currentBoundingBox;
	const auto speed = static_cast<double>(definition.getGpuSpeedMultiplier());
	const auto frames = static_cast<double>(definition.getGpuTotalFrames());
	UnsignedInt count = 0, drawn = 0;
	const auto flush = [&] {
		if (!count)
			return;
		m_pointGroup->Set_Arrays(m_posBuffer, m_RGBABuffer, nullptr, m_sizeBuffer, m_angleBuffer, m_gpuFrameBuffer.Peek(), count);
		m_pointGroup->Render(rinfo);
		drawn += count;
		count = 0;
	};
	for (Particle *particle = system.getFirstParticle(); particle; particle = particle->m_systemNext)
	{
		const auto &pos = *particle->getPosition();
		const Real size = particle->getSize();
		if (!particle->m_lifetime || !(size > 0.0f) || WWMath::Fabs(pos.x - bounds.Center.X) > bounds.Extent.X + size ||
			WWMath::Fabs(pos.y - bounds.Center.Y) > bounds.Extent.Y + size ||
			WWMath::Fabs(pos.z - bounds.Center.Z) > bounds.Extent.Z + size)
			continue;
		const auto &color = *particle->getColor();
		const auto alpha = definition.hasGpuAlphaKeys() ? particle->getAlpha() : 1.0f;
		m_posBuffer->Get_Array()[count] = Vector3(pos.x, pos.y, pos.z);
		m_sizeBuffer->Get_Array()[count] = size;
		m_RGBABuffer->Get_Array()[count] = Vector4(color.red, color.green, color.blue, alpha);
		const Real angle = std::isfinite(particle->getAngle()) ? std::fmod(particle->getAngle(), TWO_PI) : 0.0f;
		m_angleBuffer->Get_Array()[count] = static_cast<uint8>(static_cast<Int>(angle * 256.0f / TWO_PI));
		const auto age = static_cast<double>(particle->getElapsedFrames());
		m_gpuFrameBuffer->Get_Array()[count] = static_cast<uint8>(std::fmod(age * speed, frames));
		if (++count == MAX_POINTS_PER_GROUP)
			flush();
	}
	flush();
	m_pointGroup->Set_Frame_Row_Column_Count_Log2(0);
	return drawn;
}

/**
 * Nasty hack to render particles last. Called directly by WW3D::Flush()
 */
void DoParticles( RenderInfoClass &rinfo )
{
	if (TheParticleSystemManager)
		TheParticleSystemManager->doParticles(rinfo);
}

void W3DParticleSystemManager::doParticles(RenderInfoClass &rinfo)
{

	if (m_readyToRender == false)
		return;

	// external mechanism must tell us when it's OK to render again...
	m_readyToRender = false;

	//reset each frame
	/// @todo lorenzen sez: this should be debug only:
	m_onScreenParticleCount = 0;

 	const FrustumClass & frustum = rinfo.Camera.Get_Frustum();
	AABoxClass bbox;

	//Get a bounding box around our visible universe.  Bounded by terrain and the sky
	//so much tighter fitting volume than what's actually visible.  This will cull
	//particles falling under the ground.

 	TheTerrainRenderObject->getMaximumVisibleBox(frustum, &bbox, TRUE);
	m_currentBoundingBox = bbox;

	//@todo lorenzen sez: put these in registers for sure
	Real bcX = bbox.Center.X;
	Real bcY = bbox.Center.Y;
	Real bcZ = bbox.Center.Z;
	Real beX = bbox.Extent.X;
	Real beY = bbox.Extent.Y;
	Real beZ = bbox.Extent.Z;

	unsigned int personalities[MAX_POINTS_PER_GROUP];


	m_fieldParticleCount = 0;

	const Bool drawSmudge = TheSmudgeManager && TheSmudgeManager->getHardwareSupport() && TheGlobalData->m_useHeatEffects;

	if (drawSmudge)
	{
		TheSmudgeManager->resetDraw();
	}

	// Number of particles/points being rendered.
	UnsignedInt pointCount = 0;

	ParticleSystemManager::ParticleSystemList &particleSysList = TheParticleSystemManager->getAllParticleSystems();
	if (RetailFileLayers::enabled())
		renderModelParticles(rinfo, particleSysList, bbox);
	for( ParticleSystemManager::ParticleSystemListIt it = particleSysList.begin(); it != particleSysList.end(); ++it)
	{
		ParticleSystem *sys = (*it);
		if (!sys) {
			continue;
		}
		if (RetailFileLayers::enabled() && !sys->isUsingSmudge() && sys->getTemplate() && sys->getTemplate()->usesGpuDraw())
		{
			flushParticleBatch(rinfo, pointCount);
			if (sys->getTemplate()->canDrawGpuParticles())
			{
				const auto drawn = drawGpuParticles(*sys);
				m_onScreenParticleCount += drawn ? drawn : drawGpuFallback(rinfo, *sys);
			}
			continue;
		}

		// only look at particle/point style systems
		if (sys->isUsingDrawables())
			continue;

		// TheSuperHackers @performance Mauller 16/08/2026 Skip processing particle system if no particles are in view.
		UnsignedInt particleCount = 0;
		for (Particle* vp = sys->getFirstParticle(); vp; vp = vp->m_systemNext)
		{
			const Coord3D* pos = vp->getPosition();
			const Real psize = vp->getSize();

			//Test if particle is at the screen or terrain edges.
			if (WWMath::Fabs(pos->x - bcX) > (beX + psize) ||
				WWMath::Fabs(pos->y - bcY) > (beY + psize) ||
				WWMath::Fabs(pos->z - bcZ) > (beZ + psize))
			{
				vp->setIsCulled(true);
				continue;
			}

			vp->setIsCulled(false);
			particleCount++;
		}

		// Particle system has no particles on screen
		if (particleCount == 0)
			continue;

		// Handle smudge type particles
		if (sys->isUsingSmudge())
		{
			if (!drawSmudge)
				continue;

			for (Particle *p = sys->getFirstParticle(); p; p = p->m_systemNext)
			{
				if (p->isCulled())
					continue;

				if (Smudge *smudge = TheSmudgeManager->findSmudge(p))
				{
					// The particle is in view. Draw the smudge!
					smudge->m_draw = true;
				}
			}
			continue;
		}

		// TheSuperHackers @performance Ronin/Mauller 09/08/2026 Implement batched rendering for similar particles.
		// Particles with the same properties will now be batched onto a single texture surface before being drawn.
		// If a different particle type appears before the batch is filled, the previous batch will be drawn first.
		RefCountPtr<TextureClass> texture;
		texture.Assign_No_Add_Ref(W3DDisplay::m_assetManager->Get_Texture(sys->getParticleTypeName().str()));

		const Bool canBatch = sys->isUsingParticles();
		const Bool batchDone = finishedBatch(*sys, texture);
		if (!canBatch || batchDone)
		{
			flushParticleBatch(rinfo, pointCount);
		}

		// setup a new particle batch texture if prior batch was flushed.
		if (canBatch && m_batchTexture == nullptr)
		{
			initializeBatch(*sys, texture);
		}

		UnsignedInt startCount = pointCount;

		// build W3D particle buffer
		Vector3 *posArray = m_posBuffer->Get_Array();
		Real *sizeArray = m_sizeBuffer->Get_Array();
		Vector4 *RGBAArray = m_RGBABuffer->Get_Array();
		uint8 *angleArray = m_angleBuffer->Get_Array();
		const Coord3D *pos;
		const RGBColor *color;
		Real psize;



		//set-up all the per-particle
		for (Particle *p = sys->getFirstParticle(); p; p = p->m_systemNext)
		{
			if (p->isCulled())
				continue;

			pos = p->getPosition();
			psize = p->getSize();

			m_fieldParticleCount += ( sys->getPriority() == AREA_EFFECT && sys->isFieldParticle() );

			//@todo lorenzen sez: use pointer arithmetic for these arrays
			personalities[pointCount] = p->getPersonality();

			posArray[pointCount].X = pos->x;
			posArray[pointCount].Y = pos->y;
			posArray[pointCount].Z = pos->z;

			sizeArray[pointCount] = psize;

			color = p->getColor();
			RGBAArray[pointCount].X = color->red;
			RGBAArray[pointCount].Y = color->green;
			RGBAArray[pointCount].Z = color->blue;
			RGBAArray[pointCount].W = p->getAlpha();

			angleArray[pointCount] = (uint8)(p->getAngle() * 255.0f / (2.0f * PI));

			if (++pointCount == MAX_POINTS_PER_GROUP)
			{
				if (!canBatch)
				{
					break;
				}

				// TheSuperHackers @info The Buffer is full mid-system so draw what we have and carry on with the SAME system.
				// This prevents particles being dropped. Bank the stats first as the flush resets count to 0.
				m_onScreenParticleCount += (pointCount - startCount);
				flushParticleBatch(rinfo, pointCount);
				initializeBatch(*sys, texture);
				startCount = 0;
			}
		}

		if (pointCount == startCount)
		{
			continue;	//this system has no particles to render
		}

		const UnsignedInt volumeParticleDepth = sys->getVolumeParticleDepth();

		// Handle drawing streak type particles.
		if ( sys->isUsingStreak() && (pointCount >= 2) )
		{
			m_streakLine->Reset_Line();

			m_streakLine->Set_Texture( texture.Peek() );
			switch( sys->getShaderType() )
			{
				case ParticleSystemInfo::ADDITIVE:
					m_streakLine->Set_Shader( ShaderClass::_PresetAdditiveSpriteShader );
					break;
				case ParticleSystemInfo::ALPHA:
					m_streakLine->Set_Shader( ShaderClass::_PresetAlphaSpriteShader );
					break;
				case ParticleSystemInfo::ALPHA_TEST:
					m_streakLine->Set_Shader( ShaderClass::_PresetATestSpriteShader );
					break;
				case ParticleSystemInfo::MULTIPLY:
					m_streakLine->Set_Shader( ShaderClass::_PresetMultiplicativeSpriteShader );
					break;
			}

			//UPDATE THE STREAK'S ARRAYS
			m_streakLine->Set_LocsWidthsColors(
				pointCount,
				m_posBuffer->Get_Array(),
				m_sizeBuffer->Get_Array(),
				m_RGBABuffer->Get_Array(),
				&personalities[0]
				);

			//WWASSERT( m_streakLine->Get_Num_Points() == pointCount );

			// This is the happy place for this!
			RGBAArray[0].X = 0;//eliminates the scissor edge on the trailing edge of the streak
			RGBAArray[0].Y = 0;
			RGBAArray[0].Z = 0;
			RGBAArray[0].W = 0;


			//RENDER STREAK!
			m_streakLine->Render( rinfo );
			m_onScreenParticleCount += (pointCount - startCount);
			pointCount = startCount;
		}
		// Handle lone streak type particles by drawing them as regular particles.
		else if (sys->isUsingStreak() && (pointCount == 1))
		{
			m_onScreenParticleCount += (pointCount - startCount);
			initializeBatch(*sys, texture);
			flushParticleBatch(rinfo, pointCount);
			startCount = 0;
		}
		// Handle volumetric type particle systems.
		else if( sys->isUsingVolumeParticles() && volumeParticleDepth > DEFAULT_VOLUME_PARTICLE_DEPTH )
		{
			m_pointGroup->Set_Texture( texture.Peek() );
			m_pointGroup->Set_Flag( PointGroupClass::TRANSFORM, true );	// transform to screen space

			switch( sys->getShaderType() )
			{
				case ParticleSystemInfo::ADDITIVE:
					m_pointGroup->Set_Shader( ShaderClass::_PresetAdditiveSpriteShader );
					break;
				case ParticleSystemInfo::ALPHA:
					m_pointGroup->Set_Shader( ShaderClass::_PresetAlphaSpriteShader );
					break;
				case ParticleSystemInfo::ALPHA_TEST:
					m_pointGroup->Set_Shader( ShaderClass::_PresetATestSpriteShader );
					break;
				case ParticleSystemInfo::MULTIPLY:
					m_pointGroup->Set_Shader( ShaderClass::_PresetMultiplicativeSpriteShader );
					break;
			}

			/// @todo Use both QUADS and TRIS for particles
			m_pointGroup->Set_Point_Mode( PointGroupClass::QUADS );
			m_pointGroup->Set_Arrays( m_posBuffer, m_RGBABuffer, nullptr, m_sizeBuffer, m_angleBuffer, nullptr, pointCount );
			m_pointGroup->Set_Billboard(sys->getParticleAlignment() == ParticleSystemInfo::PARTICLE_ALIGNMENT_BILLBOARD);

			/// @todo Support animated texture particles
			/// @todo lorenzen sez: unimplemented code wastes cpu cycles
			m_pointGroup->Set_Point_Frame( 0 );

			m_pointGroup->RenderVolumeParticle( rinfo, volumeParticleDepth);
			m_onScreenParticleCount += (pointCount - startCount);
			pointCount = startCount;
		}


		/// @todo lorenzen sez: this should be debug only:
		//add particle count to total
		m_onScreenParticleCount += (pointCount - startCount);

	/*
		// draw the wind vector for this particle system on the screen
		UnsignedInt width = TheDisplay->getWidth();
		UnsignedInt height = TheDisplay->getHeight();
		Coord3D worldStart, worldEnd;
		ICoord2D pixelStart, pixelEnd;
		sys->getPosition( &worldStart );
		worldEnd.x = Cos( sys->getWindAngle() ) * 50.0f + worldStart.x;
		worldEnd.y = Sin( sys->getWindAngle() ) * 50.0f + worldStart.y;
		worldEnd.z = worldStart.z;
		TheTacticalView->worldToScreen( &worldStart, &pixelStart );
		TheTacticalView->worldToScreen( &worldEnd, &pixelEnd );
		Color colorStart = GameMakeColor( 255, 255, 255, 255 );
		Color colorEnd = GameMakeColor( 255, 128, 128, 255 );
		TheDisplay->drawLine( pixelStart.x, pixelStart.y, pixelEnd.x, pixelEnd.y, 1.0f, colorStart, colorEnd );
	*/


	}

	// TheSuperHackers @info Flush the last batch if one is pending.
	flushParticleBatch(rinfo, pointCount);

		/// @todo lorenzen sez: this should be debug only:
	TheParticleSystemManager->setOnScreenParticleCount(m_onScreenParticleCount);

	//Draw any particles belonging to weather effects
	if (TheSnowManager)
		((W3DSnowManager *)TheSnowManager)->render(rinfo);

	//Now process screen smudges which are particles that distort the background behind them.
	if(TheSmudgeManager)
	{
		((W3DSmudgeManager *)TheSmudgeManager)->render(rinfo);
	}
}

Bool W3DParticleSystemManager::finishedBatch(const ParticleSystem& system, const RefCountPtr<TextureClass>& texture)
{
	return texture.Peek() != m_batchTexture.Peek() ||
		system.getShaderType() != m_batchShaderType ||
		system.getParticleAlignment() != m_batchParticleAlignment;
}

void W3DParticleSystemManager::initializeBatch(const ParticleSystem& system, const RefCountPtr<TextureClass>& texture)
{
	m_batchTexture = texture;
	m_batchShaderType = system.getShaderType();
	m_batchParticleAlignment = system.getParticleAlignment();
}

void W3DParticleSystemManager::flushParticleBatch(RenderInfoClass& rinfo, UnsignedInt& pointCount)
{
	if (pointCount > 0)
	{
		ShaderClass shader;
		switch (m_batchShaderType)
		{
			case ParticleSystemInfo::ADDITIVE:
				shader = ShaderClass::_PresetAdditiveSpriteShader;
				break;
			case ParticleSystemInfo::ALPHA:
				shader = ShaderClass::_PresetAlphaSpriteShader;
				break;
			case ParticleSystemInfo::ALPHA_TEST:
				shader = ShaderClass::_PresetATestSpriteShader;
				break;
			case ParticleSystemInfo::MULTIPLY:
				shader = ShaderClass::_PresetMultiplicativeSpriteShader;
				break;
		}

		if (m_batchParticleAlignment == ParticleSystemInfo::PARTICLE_ALIGNMENT_CONFORMING)
		{
			m_terrainParticles->setTexture(m_batchTexture.Peek());
			m_terrainParticles->setShader( shader );
			m_terrainParticles->setArrays( m_posBuffer, m_RGBABuffer, m_sizeBuffer, m_angleBuffer, pointCount );
			m_terrainParticles->setBoundingBox( m_currentBoundingBox );
			m_terrainParticles->render();
		}
		else // draw regular point group
		{
			m_pointGroup->Set_Texture(m_batchTexture.Peek());
			m_pointGroup->Set_Shader(shader);
			m_pointGroup->Set_Flag(PointGroupClass::TRANSFORM, true);
			m_pointGroup->Set_Point_Mode(PointGroupClass::QUADS);
			m_pointGroup->Set_Arrays(m_posBuffer, m_RGBABuffer, nullptr, m_sizeBuffer, m_angleBuffer, nullptr, pointCount);
			m_pointGroup->Set_Billboard(m_batchParticleAlignment == ParticleSystemInfo::PARTICLE_ALIGNMENT_BILLBOARD);
			m_pointGroup->Set_Point_Frame(0);
			m_pointGroup->Render(rinfo);
		}

		pointCount = 0;
	}

	m_batchTexture.Clear();
	m_batchParticleAlignment = ParticleSystemInfo::PARTICLE_ALIGNMENT_BILLBOARD;
	m_batchShaderType = ParticleSystemInfo::INVALID_SHADER;
}
