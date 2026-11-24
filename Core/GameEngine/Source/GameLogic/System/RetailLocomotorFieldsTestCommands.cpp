#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void beginRetreat(const RetailTestCommands::Context& ctx)
{
	Object* horde = ctx.requireObject();
	if (horde && ctx.target && horde->getAI())
		horde->getAI()->fleeFromFear(ctx.target, LOGICFRAMES_PER_SECOND, MODELCONDITION_EMOTION_UNCONTROLLABLY_AFRAID, 1.0f, TRUE);
}

void sampleRetreat(const RetailTestCommands::Context& ctx)
{
	const HordeContain* horde = HordeContain::get(ctx.requireObject());
	if (!horde)
		return;

	for (const Object* member : *horde->getContainedItemsList())
	{
		const PhysicsBehavior* physics = member->getPhysics();
		if (physics)
			std::fprintf(stderr, "TEST locomotor-fields %u %d %.5f %.5f\n", ctx.frame,
				member->getLogicModelConditions().test(MODELCONDITION_BACKING_UP),
				physics->getVelocity()->x, member->getUnitDirectionVector2D()->x);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailLocomotorFieldsTestCommands()
{
#if !RTS_GENERALS
	registerAction("locomotor-fields-back", beginRetreat);
	registerAction("locomotor-fields-sample", sampleRetreat);
#endif
}
}
