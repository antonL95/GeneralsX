#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void pushTurnSubject(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (object && ctx.hasPosition)
	{
		if (PhysicsBehavior* physics = object->getPhysics())
			physics->addVelocityTo(&ctx.position);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailArcTurnSpeedTestCommands()
{
#if !RTS_GENERALS
	registerAction("arc-turn-push", pushTurnSubject);
#endif
}
}
