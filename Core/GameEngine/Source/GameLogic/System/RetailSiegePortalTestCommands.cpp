#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "Common/ThingTemplate.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/RetailGate.h"
#include "GameLogic/Module/RetailSiegeContain.h"
#include "GameLogic/Module/RetailSiegeDocking.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"

namespace
{
void probeSiegePortal(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (!object)
		return;
	const auto* body = object->getBodyModule();
	const Real hp = body ? body->getHealth() : 0.0f;
	const Real maxHp = body ? body->getMaxHealth() : 0.0f;
	if (const auto* portal = DynamicPortalBehaviour::find(object))
	{
		const auto* siege = SiegeAIUpdate::find(object);
		std::vector<Coord3D> waypoints;
		portal->getClimbWaypoints(waypoints);
		Coord3D topPos{};
		const Bool hasTop = portal->getTopAttackPosition(topPos);
		const Real topRad = portal->getTopAttackRadius();
		std::fprintf(stderr,
			"TEST siege-portal frame=%u object=%s active=%d docked=%d hp=%.1f/%.1f waypoints=%zu "
			"topPos=%d(%.2f,%.2f,%.2f) topRad=%.1f\n",
			ctx.frame, object->getTemplate()->getName().str(),
			portal->isActive() ? 1 : 0,
			siege && siege->isDocked() ? 1 : 0,
			hp, maxHp, waypoints.size(),
			hasTop ? 1 : 0, topPos.x, topPos.y, topPos.z, topRad);
		return;
	}
	const Coord3D& pos = *object->getPosition();
	const Object* container = object->getContainedBy();
	Bool climbing = FALSE;
	Real climbDist = 0.0f;
	Int climbSeg = -1;
	if (const auto* contain = SiegeEngineContain::find(container))
	{
		climbing = contain->getClimbProgress(object->getID(), climbDist, climbSeg);
	}
	Bool inRange = FALSE;
	Bool topRange = FALSE;
	if (ctx.target)
	{
		if (const auto* portal = DynamicPortalBehaviour::find(ctx.target))
			topRange = portal->isWithinTopAttackRange(*object);
		if (const Weapon* weapon = const_cast<Object*>(object)->getCurrentWeapon())
			inRange = weapon->isWithinAttackRange(object, ctx.target);
	}
	std::fprintf(stderr,
		"TEST siege-portal frame=%u object=%s ord=%d layer=%d wall=%d pos=%.2f,%.2f,%.2f "
		"contained=%u climbing=%d climbDist=%.2f climbSeg=%d inRange=%d topRange=%d hp=%.1f\n",
		ctx.frame, object->getTemplate()->getName().str(), ctx.ordinal,
		object->getLayer(), object->getLayer() == LAYER_WALL ? 1 : 0,
		pos.x, pos.y, pos.z,
		container ? static_cast<unsigned>(container->getID()) : 0u,
		climbing ? 1 : 0, climbDist, climbSeg,
		inRange ? 1 : 0, topRange ? 1 : 0, hp);
}

void placeOnWallNearPortal(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit)
		return;
	Coord3D pos{};
	if (ctx.hasPosition)
	{
		pos = ctx.position;
	}
	else if (ctx.argument && std::sscanf(ctx.argument, "%f,%f", &pos.x, &pos.y) == 2)
	{
		pos.z = 0.0f;
	}
	else if (ctx.target)
	{
		const auto* siege = SiegeAIUpdate::find(ctx.target);
		const auto* portal = DynamicPortalBehaviour::find(ctx.target);
		if (siege && siege->getReleasePosition(pos))
		{
		}
		else if (portal && portal->getTopAttackPosition(pos))
		{
		}
		else
		{
			return;
		}
	}
	else
	{
		return;
	}
	pos.z = TheTerrainLogic->getLayerHeight(pos.x, pos.y, LAYER_WALL);
	if (auto* ai = unit->getAI())
	{
		ai->destroyPath();
		ai->setLocomotorGoalNone();
		ai->aiIdle(CMD_FROM_AI);
	}
	if (auto* physics = unit->getPhysics())
		physics->resetDynamicPhysics();
	TheAI->pathfinder()->removeObjectFromPathfindMap(unit);
	unit->setLayer(LAYER_WALL);
	unit->setDestinationLayer(LAYER_WALL);
	unit->setPosition(&pos);
	TheAI->pathfinder()->addObjectToPathfindMap(unit);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailSiegePortalTestCommands()
{
#if !RTS_GENERALS
	registerAction("siege-portal-probe", probeSiegePortal);
	registerAction("siege-portal-place-wall", placeOnWallNearPortal);
#endif
}
}
