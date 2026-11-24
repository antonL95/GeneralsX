#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <vector>

#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/RetailSiegeContain.h"
#include "GameLogic/Object.h"

namespace
{
void sampleGrond(const RetailTestCommands::Context& ctx)
{
	const Object* obj = ctx.requireObject();
	if (!obj)
		return;
	const AIUpdateInterface* ai = obj->getAI();
	const Locomotor* loco = ai ? ai->getCurLocomotor() : nullptr;
	const PhysicsBehavior* physics = obj->getPhysics();
	const Coord3D* vel = physics ? physics->getVelocity() : nullptr;
	const Coord3D* dir = obj->getUnitDirectionVector2D();
	const Coord3D* pos = obj->getPosition();
	const ModelConditionFlags& conds = obj->getLogicModelConditions();
	const Real maxSpeed = loco && obj->getBodyModule()
		? loco->getMaxSpeedForCondition(obj->getBodyModule()->getDamageState(), obj)
		: 0.0f;
	const Real backingSpeed = loco ? loco->getBackingUpSpeed() : 0.0f;
	std::fprintf(stderr, "TEST pkg69 %u %s %d %d %d %d %d %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n",
		ctx.frame, obj->getTemplate()->getName().str(), ctx.ordinal,
		conds.test(MODELCONDITION_BACKING_UP) ? 1 : 0,
		(loco && loco->isMovingBackwards()) ? 1 : 0,
		conds.test(MODELCONDITION_MOVING) ? 1 : 0,
		(conds.test(MODELCONDITION_TURN_LEFT) || conds.test(MODELCONDITION_TURN_RIGHT)) ? 1 : 0,
		vel ? vel->x : 0.0f, vel ? vel->y : 0.0f,
		dir ? dir->x : 0.0f, pos ? pos->x : 0.0f, pos ? pos->y : 0.0f,
		maxSpeed, backingSpeed);
}

void clearCrew(const RetailTestCommands::Context& ctx)
{
	Object* obj = ctx.requireObject();
	SiegeEngineContain* siege = SiegeEngineContain::find(obj);
	if (!siege)
		return;
	const ContainedItemsList* items = siege->getContainedItemsList();
	if (!items)
		return;
	const std::vector<Object*> riders(items->begin(), items->end());
	for (Object* rider : riders)
	{
		if (rider && siege->isCrew(rider))
		{
			siege->removeFromContain(rider, TRUE);
			TheGameLogic->destroyObject(rider);
		}
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage69TestCommands()
{
#if !RTS_GENERALS
	registerAction("pkg69-sample", sampleGrond);
	registerAction("pkg69-clear-crew", clearCrew);
#endif
}
}
