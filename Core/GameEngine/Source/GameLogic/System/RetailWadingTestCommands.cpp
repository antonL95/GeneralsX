#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/AI.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
Int countAttachedParticles(DrawableID drawableId)
{
	if (drawableId == INVALID_DRAWABLE_ID || !TheParticleSystemManager)
		return 0;
	Int count = 0;
	auto& systems = TheParticleSystemManager->getAllParticleSystems();
	for (ParticleSystem* sys : systems)
	{
		if (sys && sys->getAttachedDrawable() == drawableId)
			++count;
	}
	return count;
}

void sampleWading(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (!object)
		return;
	const Drawable* drawable = object->getDrawable();
	const Coord3D& pos = *object->getPosition();
	Real waterZ = 0.0f;
	Real groundZ = 0.0f;
	const Bool underwater = TheTerrainLogic && TheTerrainLogic->isUnderwater(pos.x, pos.y, &waterZ, &groundZ);
	const Real depth = underwater ? (waterZ - groundZ) : 0.0f;
	const Bool wade = drawable && drawable->getModelConditionFlags().test(MODELCONDITION_WADING);
	const Bool swim = drawable && drawable->getModelConditionFlags().test(MODELCONDITION_SWIMMING);
	Int particles = drawable ? countAttachedParticles(drawable->getID()) : 0;

	Int members = 0;
	Int membersWade = 0;
	Int membersSwim = 0;
	if (const ContainModuleInterface* contain = object->getContain())
	{
		if (const ContainedItemsList* items = contain->getContainedItemsList())
		{
			for (const Object* member : *items)
			{
				if (!member || member->isEffectivelyDead())
					continue;
				++members;
				if (const Drawable* memberDraw = member->getDrawable())
				{
					if (memberDraw->getModelConditionFlags().test(MODELCONDITION_WADING))
						++membersWade;
					if (memberDraw->getModelConditionFlags().test(MODELCONDITION_SWIMMING))
						++membersSwim;
					particles += countAttachedParticles(memberDraw->getID());
				}
			}
		}
	}

	std::fprintf(stderr,
		"TEST wading-sample %u %s pos=%.1f,%.1f,%.1f uw=%d depth=%.2f wade=%d swim=%d members=%d membersWade=%d membersSwim=%d particles=%d\n",
		ctx.frame, object->getTemplate()->getName().str(),
		pos.x, pos.y, pos.z, underwater ? 1 : 0, depth,
		wade ? 1 : 0, swim ? 1 : 0, members, membersWade, membersSwim, particles);
	std::fflush(stderr);
}

void scanWaterLine(const RetailTestCommands::Context& ctx)
{
	if (!TheTerrainLogic || !TheAI || !TheAI->getAiData())
		return;
	const Real wadeDepth = TheAI->getAiData()->m_wadeWaterDepth;
	Real x0 = 0.0f;
	Real y0 = 0.0f;
	Real x1 = 0.0f;
	Real y1 = 0.0f;
	if (std::sscanf(ctx.argument, "%f,%f,%f,%f", &x0, &y0, &x1, &y1) != 4)
		return;
	for (Int i = 0; i <= 20; ++i)
	{
		const Real t = static_cast<Real>(i) / 20.0f;
		const Real x = x0 + (x1 - x0) * t;
		const Real y = y0 + (y1 - y0) * t;
		Real waterZ = 0.0f;
		Real groundZ = 0.0f;
		const Bool uw = TheTerrainLogic->isUnderwater(x, y, &waterZ, &groundZ);
		const Real depth = uw ? (waterZ - groundZ) : 0.0f;
		std::fprintf(stderr, "TEST wading-scan %u pt=%.0f,%.0f uw=%d depth=%.2f wadeDepth=%.2f\n",
			ctx.frame, x, y, uw ? 1 : 0, depth, wadeDepth);
	}
	std::fflush(stderr);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailWadingTestCommands()
{
#if !RTS_GENERALS
	registerAction("wading-sample", sampleWading);
	registerAction("wading-scan", scanWaterLine);
#endif
}
}
