#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
const PhysicsBehaviorModuleData* findPhysicsData(const Object& obj, const PhysicsBehavior& physics)
{
	const auto& modules = obj.getTemplate()->getBehaviorModuleInfo();
	for (Int i = 0; i < modules.getCount(); ++i)
	{
		const auto* data = modules.getNthData(i);
		if (data && data->getModuleTagNameKey() == physics.getModuleTagNameKey())
			return static_cast<const PhysicsBehaviorModuleData*>(data);
	}
	return nullptr;
}

void logPhysicsObject(const char* label, const Object* obj, UnsignedInt frame)
{
	const Coord3D* pos = obj->getPosition();
	const Real groundZ = TheTerrainLogic->getLayerHeight(pos->x, pos->y, obj->getLayer());
	const PhysicsBehavior* physics = obj->getPhysics();
	const auto* data = physics ? findPhysicsData(*obj, *physics) : nullptr;
	const Coord3D* vel = physics ? physics->getVelocity() : nullptr;
	const ModelConditionFlags& mc = obj->getLogicModelConditions();
	std::fprintf(stderr,
		"TEST pkg57 frame=%u label=%s object=%u dead=%d z=%.3f agl=%.3f vz=%.3f airborne=%d stunned=%d flailing=%d prone=%d standing=%d impact=%d gravity=%.1f low=%u high=%u up=%u\n",
		frame,
		label,
		static_cast<unsigned>(obj->getID()),
		obj->isEffectivelyDead() ? 1 : 0,
		pos->z,
		pos->z - groundZ,
		vel ? vel->z : 0.0f,
		obj->isAboveTerrain() ? 1 : 0,
		physics && physics->getIsStunned() ? 1 : 0,
		mc.test(MODELCONDITION_STUNNED_FLAILING) ? 1 : 0,
		mc.test(MODELCONDITION_STUNNED) ? 1 : 0,
		mc.test(MODELCONDITION_STUNNED_STANDING_UP) ? 1 : 0,
		physics && physics->isInRetailImpact() ? 1 : 0,
		data ? data->m_gravityMult : 0.0f,
		data ? data->m_shockStunnedTimeLow : 0,
		data ? data->m_shockStunnedTimeHigh : 0,
		data ? data->m_shockStandingTime : 0);
	std::fflush(stderr);
}

void handlePackage57(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.findObject(false);
	if (!source)
	{
		if (ctx.action == "pkg57-probe")
			std::fprintf(stderr, "TEST pkg57 frame=%u label=%s missing=1\n", ctx.frame, ctx.argument);
		return;
	}

	if (ctx.action == "pkg57-probe")
	{
		const char* label = ctx.argument && ctx.argument[0] ? ctx.argument : source->getTemplate()->getName().str();
		logPhysicsObject(label, source, ctx.frame);
	}
	else if (ctx.action == "pkg57-horde-probe")
	{
		const char* label = ctx.argument && ctx.argument[0] ? ctx.argument : source->getTemplate()->getName().str();
		logPhysicsObject(label, source, ctx.frame);
		if (const HordeContain* horde = HordeContain::get(source))
		{
			if (const ContainedItemsList* items = horde->getContainedItemsList())
			{
				Int idx = 0;
				for (const Object* member : *items)
				{
					if (!member)
						continue;
					char memberLabel[128];
					std::snprintf(memberLabel, sizeof(memberLabel), "%s-m%d", label, idx);
					logPhysicsObject(memberLabel, member, ctx.frame);
					if (++idx >= 2)
						break;
				}
			}
		}
	}
	else if (ctx.action == "pkg57-drop")
	{
		Real height = 80.0f;
		if (ctx.argument && ctx.argument[0])
			std::sscanf(ctx.argument, "%f", &height);
		Coord3D pos = *source->getPosition();
		pos.z = TheTerrainLogic->getLayerHeight(pos.x, pos.y, source->getLayer()) + height;
		source->setPosition(&pos);
		if (PhysicsBehavior* physics = source->getPhysics())
		{
			physics->setAllowToFall(TRUE);
			const Coord3D nudge{0.0f, 0.0f, -0.01f};
			physics->applyForce(&nudge);
		}
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage57TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg57-", handlePackage57);
#endif
}
}
