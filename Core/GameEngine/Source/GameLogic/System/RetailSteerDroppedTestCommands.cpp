#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void sampleSteerDropped(const RetailTestCommands::Context& ctx)
{
	Object* anchor = ctx.requireObject();
	const HordeContain* horde = anchor ? HordeContain::get(anchor) : nullptr;
	if (!horde)
		return;
	for (Object* member : *horde->getContainedItemsList())
	{
		const AIUpdateInterface* ai = member->getAI();
		const Locomotor* loco = ai ? ai->getCurLocomotor() : nullptr;
		if (!loco || loco->getAppearance() != LOCO_WHEELS_FOUR || horde->getMemberSlot(member->getID()) < 0)
			continue;
		if (ctx.frame == 240)
			member->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_BRAKING));
		const Coord3D* position = member->getPosition();
		std::fprintf(stderr, "TEST steer-dropped %u %u %.3f %.3f braking=%d\n", ctx.frame,
			static_cast<unsigned>(member->getID()), position->x, position->y,
			member->testStatus(OBJECT_STATUS_BRAKING));
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailSteerDroppedTestCommands()
{
#if !RTS_GENERALS
	registerAction("steer-dropped-sample", sampleSteerDropped);
#endif
}
}
