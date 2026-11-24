#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Pathfinder/PathfindCell.h"
#include "GameLogic/Pathfinder/Path.h"
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
#endif
}
}
