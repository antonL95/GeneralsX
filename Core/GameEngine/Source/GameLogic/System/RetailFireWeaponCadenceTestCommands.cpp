#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"

namespace
{
void sampleHealth(const RetailTestCommands::Context& ctx)
{
	const Object* target = ctx.requireObject();
	if (target && target->getBodyModule())
		std::fprintf(stderr, "TEST fireweapon-cadence health %u %s %.0f\n",
			ctx.frame, ctx.argument, target->getBodyModule()->getHealth());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailFireWeaponCadenceTestCommands()
{
#if !RTS_GENERALS
	registerAction("fireweapon-cadence-health", sampleHealth);
#endif
}
}
