#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
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
	const Object* anchor = ctx.requireObject();
	const HordeContain* horde = HordeContain::get(anchor);
	if (!horde)
		return;

	for (const Object* member : *horde->getContainedItemsList())
	{
		const PhysicsBehavior* physics = member->getPhysics();
		if (physics)
			std::fprintf(stderr, "TEST locomotor-fields %u %s %d %d %.5f %.5f %.5f %.6f %.6f\n",
				ctx.frame, anchor->getTemplate()->getName().str(), ctx.ordinal,
				member->getLogicModelConditions().test(MODELCONDITION_BACKING_UP),
				physics->getVelocity()->x, physics->getVelocity()->y, member->getUnitDirectionVector2D()->x,
				member->getPosition()->x, member->getPosition()->y);
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
