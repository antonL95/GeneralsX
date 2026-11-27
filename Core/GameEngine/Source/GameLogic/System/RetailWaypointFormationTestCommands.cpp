#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void sampleWaypointFormation(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	const AIUpdateInterface* ai = object ? object->getAI() : nullptr;
	if (!ai)
		return;

	const Locomotor* locomotor = ai->getCurLocomotor();
	const PhysicsBehavior* physics = object->getPhysics();
	const Coord3D velocity = physics ? *physics->getVelocity() : Coord3D{};
	std::fprintf(stderr, "TEST waypoint-formation %u %s %d %d %d %.6f\n", ctx.frame,
		object->getTemplate()->getName().str(), ai->friend_getCurrentGoalPathIndex(),
		static_cast<Int>(ai->isFollowingRetailGroupRoute()), static_cast<Int>(locomotor && locomotor->getWaitForFormation()),
		velocity.x * velocity.x + velocity.y * velocity.y);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailWaypointFormationTestCommands()
{
#if !RTS_GENERALS
	registerAction("waypoint-formation-sample", sampleWaypointFormation);
#endif
}
}
