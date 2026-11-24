#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "GameLogic/Object.h"

namespace
{
void checkTimedModelUpgrade(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (object)
		std::fprintf(stderr, "TEST timed-model-upgrade frame=%u case=%s user2=%d\n",
			ctx.frame, ctx.argument, object->getLogicModelConditions().test(MODELCONDITION_USER_2));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailTimedModelUpgradeTestCommands()
{
#if !RTS_GENERALS
	registerAction("timed-model-upgrade", checkTimedModelUpgrade);
#endif
}
}
