#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "GameLogic/Object.h"

namespace
{
void observeSubframeCondition(const RetailTestCommands::Context& context)
{
	const Object* object = context.requireObject();
	if (object)
		std::fprintf(stderr, "TEST upgrade-rows subframe %u user2=%d\n",
			context.frame, object->getLogicModelConditions().test(MODELCONDITION_USER_2));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailUpgradeRowsTestCommands()
{
#if !RTS_GENERALS
	registerAction("upgrade-rows-subframe", observeSubframeCondition);
#endif
}
}
