#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <vector>

#include "Common/ThingTemplate.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Pathfinder/PathfindCell.h"
#include "GameLogic/Pathfinder/Path.h"
#include "GameLogic/Pathfinder/PathfindLayer.h"
#include "GameLogic/Pathfinder/PathfindZoneManager.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
struct PathfinderProbe : Pathfinder
{
	static constexpr auto findHierarchy = &PathfinderProbe::findHierarchicalPath;
};

void sampleShip(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (!object)
		return;
	const Coord3D& pos = *object->getPosition();
	const PathfindCell* cell = TheAI->pathfinder()->getCell(object->getLayer(), &pos);
	const Int cellType = cell ? static_cast<Int>(cell->getType()) : -1;
	Real waterZ = 0.0f;
	Real terrainZ = 0.0f;
	const Bool underwater = TheTerrainLogic->isUnderwater(pos.x, pos.y, &waterZ, &terrainZ);
	const WaterHandle* water = TheTerrainLogic->getWaterHandle(pos.x, pos.y);
	const Bool river = water && water->m_polygon && water->m_polygon->isRiver();
	const AIUpdateInterface* ai = object->getAI();
	const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
	const Real cap = locomotor ? locomotor->getMaxSpeedForCondition(object->getBodyModule()->getDamageState(), object) : -1.0f;
	const PhysicsBehavior* physics = object->getPhysics();
	const Real speed = physics ? physics->getForwardSpeed2D() : 0.0f;
	std::fprintf(stderr,
		"TEST deep-water-sample %u %s pos=%.2f,%.2f,%.2f cell=%d uw=%d river=%d depth=%.2f loco=%d cap=%.4f speed=%.4f waterOffset=%.2f\n",
		ctx.frame, object->getTemplate()->getName().str(),
		pos.x, pos.y, pos.z, cellType, underwater ? 1 : 0, river ? 1 : 0,
		underwater ? (waterZ - terrainZ) : 0.0f, locomotor ? 1 : 0, cap, speed,
		underwater ? pos.z - waterZ : 0.0f);
}

void probeRoute(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (!object || !ctx.hasPosition)
		return;
	const AIUpdateInterface* ai = object->getAI();
	if (!ai)
		return;
	const Coord3D start = *object->getPosition();
	const Coord3D goal = ctx.position;
	const PathfindCell* srcCell = TheAI->pathfinder()->getCell(object->getLayer(), &start);
	const PathfindCell* dstCell = TheAI->pathfinder()->getCell(LAYER_GROUND, &goal);
	const Bool exists = TheAI->pathfinder()->clientSafeQuickDoesPathExist(ai->getLocomotorSet(), &start, &goal);
	std::fprintf(stderr,
		"TEST deep-water-route %u %s exists=%d src=%d dst=%d goal=%.1f,%.1f\n",
		ctx.frame, object->getTemplate()->getName().str(),
		exists ? 1 : 0,
		srcCell ? static_cast<Int>(srcCell->getType()) : -1,
		dstCell ? static_cast<Int>(dstCell->getType()) : -1,
		goal.x, goal.y);
}

void probeHierarchy(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (!object || !ctx.hasPosition || !object->getAI())
		return;
	Pathfinder* pathfinder = TheAI->pathfinder();
	const LocomotorSet& locomotors = object->getAI()->getLocomotorSet();
	Path* path = (pathfinder->*PathfinderProbe::findHierarchy)(
		TRUE, locomotors, object->getPosition(), &ctx.position, FALSE);
	std::fprintf(stderr, "TEST deep-water-hierarchy %u %s exists=%d goal=%.1f,%.1f\n",
		ctx.frame, object->getTemplate()->getName().str(), path ? 1 : 0, ctx.position.x, ctx.position.y);
	deleteInstance(path);
}

void probeZoneLabels(const RetailTestCommands::Context& ctx)
{
	constexpr Int side = 420;
	constexpr Int oldZoneLimit = 1 << 14;
	constexpr Int expected = side * side / PathfindZoneManager::ZONE_BLOCK_SIZE;
	constexpr auto deepWaterCell = static_cast<PathfindCell::CellType>(7);
	const IRegion2D bounds{{0, 0}, {side - 1, side - 1}};
	std::vector<PathfindCell> cells(side * side);
	std::vector<PathfindCell*> map(side);
	for (Int x = 0; x < side; ++x)
	{
		map[x] = &cells[x * side];
		for (Int y = 0; y < side; ++y)
			map[x][y].setType(x % 2 ? deepWaterCell : PathfindCell::CELL_WATER);
	}
	PathfindLayer layers[LAYER_LAST + 1];
	PathfindZoneManager zones;
	zones.allocateBlocks(bounds);
	zones.calculateZones(map.data(), layers, bounds);
	const zoneStorageType last = map.back()[side - 1].getZone();
	const Bool unique = last == expected && last > oldZoneLimit &&
		map[side - 2][side - 1].getZone() == last - 1;
	Bool roundTrip = FALSE;
	if (unique && ctx.argument[0])
	{
		XferSave save;
		save.open(ctx.argument);
		zones.xferConnectivity(&save, map.data(), layers, bounds);
		save.close();
		map.back()[side - 1].setZone(0);
		PathfindZoneManager loaded;
		loaded.allocateBlocks(bounds);
		XferLoad load;
		load.open(ctx.argument);
		loaded.xferConnectivity(&load, map.data(), layers, bounds);
		load.close();
		std::remove(ctx.argument);
		roundTrip = map.back()[side - 1].getZone() == last &&
			loaded.getBlockZone(LOCOMOTORSURFACE_DEEP_WATER, FALSE, side - 1, side - 1, map.data()) == last;
	}
	std::fprintf(stderr, "TEST deep-water-zone-labels %u last=%u unique=%d roundTrip=%d\n",
		ctx.frame, static_cast<unsigned>(last), unique, roundTrip);
}

void dropStunnedShip(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	PhysicsBehavior* physics = object ? object->getPhysics() : nullptr;
	if (!physics)
		return;
	Coord3D pos = *object->getPosition();
	const PathfindCell* cell = TheAI->pathfinder()->getCell(object->getLayer(), &pos);
	if (!cell)
		return;
	pos.z = TheTerrainLogic->getLayerHeight(pos.x, pos.y, object->getLayer()) + 30.0f;
	object->setPosition(&pos);
	physics->setAllowToFall(TRUE);
	physics->setAllowBouncing(TRUE);
	physics->setImmuneToFallingDamage(TRUE);
	physics->setStunned(TRUE);
	const Coord3D force{0.0f, 0.0f, -60.0f * physics->getMass()};
	physics->applyForce(&force);
	std::fprintf(stderr, "TEST deep-water-stun-drop %u %s cell=%d\n",
		ctx.frame, object->getTemplate()->getName().str(), static_cast<Int>(cell->getType()));
}

void probeStunSurvival(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.findObject();
	const PhysicsBehavior* physics = object ? object->getPhysics() : nullptr;
	const Bool alive = object && !object->isEffectivelyDead() && !object->isDestroyed();
	const Bool bounced = physics && physics->getIsStunned() &&
		(object->getHeightAboveTerrain() <= 0.01f || physics->getVelocity()->z > 0.0f);
	std::fprintf(stderr, "TEST deep-water-stun-survival %u %s alive=%d bounced=%d\n",
		ctx.frame, ctx.targetName, alive, bounced);
}

void setDryRubble(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object)
		return;
	object->getBodyModule()->setDamageState(BODY_RUBBLE);
	TheAI->pathfinder()->forceMapRecalculation();
}
}
#endif

namespace RetailTestCommands
{
void registerRetailDeepWaterTestCommands()
{
#if !RTS_GENERALS
	registerAction("deep-water-sample", sampleShip);
	registerAction("deep-water-route", probeRoute);
	registerAction("deep-water-hierarchy", probeHierarchy);
	registerAction("deep-water-zone-labels", probeZoneLabels);
	registerAction("deep-water-stun-drop", dropStunnedShip);
	registerAction("deep-water-stun-survival", probeStunSurvival);
	registerAction("deep-water-test-rubble", setDryRubble);
#endif
}
}
