#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include "Common/ThingTemplate.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void probe(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	AIUpdateInterface* ai = object ? object->getAI() : nullptr;
	PhysicsBehavior* physics = object ? object->getPhysics() : nullptr;
	if (!ai || !physics || !ai->getCurLocomotor())
		return;
	std::fprintf(stderr, "TEST corner-speed %u %s scalar=%.5f planar=%.5f,%.5f\n", ctx.frame,
		object->getTemplate()->getName().str(), ai->getCurLocomotor()->getRetailScalarSpeed(),
		physics->getVelocity()->x, physics->getVelocity()->y);
}

void checkCell(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object)
		return;
	Coord3D from = *object->getPosition();
	Coord3D to = from;
	to.x += 1.0f;
	Coord3D adjacent = from;
	adjacent.y += 10.0f;
	const Bool same = TheAI->pathfinder()->isArcSegmentPassable(object, LOCOMOTORSURFACE_GROUND,
		object->getLayer(), from, to, FALSE);
	const Bool neighbor = TheAI->pathfinder()->isArcSegmentPassable(object, LOCOMOTORSURFACE_GROUND,
		object->getLayer(), adjacent, adjacent, FALSE);
	std::fprintf(stderr, "TEST corner-cell %u same=%d neighbor=%d\n", ctx.frame, same, neighbor);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailCornerRoundingTestCommands()
{
#if !RTS_GENERALS
	registerAction("corner-speed", probe);
	registerAction("corner-cell", checkCell);
#endif
}
}
