#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Pathfinder/Path.h"
#include "GameLogic/Weapon.h"

namespace
{
void traceDogs(const RetailTestCommands::Context& ctx)
{
	const Object* owner = ctx.requireObject();
	if (!owner)
		return;

	for (Object* dog = TheGameLogic->getFirstObject(); dog; dog = dog->getNextObject())
	{
		if (dog->getProducerID() != owner->getID() || dog->isEffectivelyDead() ||
			dog->getTemplate()->getName() != "ArnorMaggotsHoundSkirmish")
			continue;

		AIUpdateInterface* ai = dog->getAI();
		const Locomotor* loco = ai ? ai->getCurLocomotor() : nullptr;
		const PhysicsBehavior* physics = dog->getPhysics();
		if (!loco || !physics)
			continue;

		const Coord3D& pos = *dog->getPosition();
		const Coord3D& goal = *ai->getGoalPosition();
		const Coord3D& velocity = *physics->getVelocity();
		const Object* target = ai->getGoalObject();
		const Path* path = ai->getPath();
		const ClosestPointOnPathInfo* route = path ? path->getCachedPointOnPath() : nullptr;
		const BodyDamageType damage = dog->getBodyModule()->getDamageState();
		const Weapon* weapon = dog->getCurrentWeapon();
		const Bool inRange = weapon && ctx.target && weapon->isWithinAttackRange(dog, ctx.target);
		std::fprintf(stderr, "TEST maggot-dog %u %d %u %.5f %.5f %.6f %.5f %.5f %.5f %.5f %d %u %d %s %d %.6f %.6f %u %d",
			ctx.frame, ctx.ordinal, static_cast<UnsignedInt>(dog->getID()), pos.x, pos.y, dog->getOrientation(),
			goal.x, goal.y, velocity.x, velocity.y, static_cast<Int>(ai->getAIStateType()),
			target ? static_cast<UnsignedInt>(target->getID()) : 0, inRange, loco->getTemplateName().str(),
			static_cast<Int>(loco->getAppearance()), loco->getMaxSpeedForCondition(damage, dog), loco->getMaxTurnRate(damage),
			static_cast<UnsignedInt>(ai->getGuardObject()), ai->isMoving());
		if (route)
			std::fprintf(stderr, " route=%.5f,%.5f", route->posOnPath.x, route->posOnPath.y);
		std::fprintf(stderr, " waiting=%d stuck=%d\n", ai->isWaitingForPath(), ai->isBlockedAndStuck());
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailMaggotDogsTestCommands()
{
#if !RTS_GENERALS
	registerAction("maggot-dogs-trace", traceDogs);
#endif
}
}
