#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"

namespace
{
void logTurnActor(const RetailTestCommands::Context& ctx, const Object& object, const char* role, Int slot,
	const Coord3D& target)
{
	const Coord3D& position = *object.getPosition();
	const AIUpdateInterface* ai = object.getAI();
	const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
	const BodyDamageType damage = object.getBodyModule()->getDamageState();
	std::fprintf(stderr, "TEST cavalry-c1 %u %s %u %d %.9g %.9g %.9g %.9g %.9g %.9g %.9g %d %d\n",
		ctx.frame, role, static_cast<unsigned>(object.getID()), slot,
		position.x, position.y, object.getOrientation(), target.x, target.y,
		locomotor ? locomotor->getMaxTurnRate(damage) : 0.0f,
		locomotor ? locomotor->getMaxSpeedForCondition(damage, &object) : 0.0f,
		locomotor && locomotor->getAppearance() == LOCO_WHEELS_FOUR && ai->getCurLocomotorSetType() == LOCOMOTORSET_MOUNTED,
		ai && ai->isMoving());
}

void logCavalryTurn(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (!object)
		return;
	const HordeContain* horde = HordeContain::get(object);
	logTurnActor(ctx, *object, horde ? "anchor" : "hero", -1, *object->getPosition());
	if (!horde)
		return;
	for (const Object* member : *horde->getContainedItemsList())
	{
		if (member->isEffectivelyDead())
			continue;
		const Int slot = horde->getMemberSlot(member->getID());
		logTurnActor(ctx, *member, "rider", slot, horde->getMemberPosition(member->getID(), FALSE));
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailCavalryTurnC1TestCommands()
{
#if !RTS_GENERALS
	registerAction("cavalry-c1-probe", logCavalryTurn);
#endif
}
}
