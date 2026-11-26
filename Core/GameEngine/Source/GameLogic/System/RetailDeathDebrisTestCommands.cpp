#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

namespace
{
void probeDeathDebris(const RetailTestCommands::Context& ctx)
{
	Int count = 0;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		if (!object->isDestroyed() && object->getTemplate()->getName().compareNoCase(ctx.argument) == 0)
			++count;
	std::fprintf(stderr, "TEST death-debris frame=%u template=%s count=%d\n", ctx.frame, ctx.argument, count);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailDeathDebrisTestCommands()
{
#if !RTS_GENERALS
	registerAction("death-debris-count", probeDeathDebris);
#endif
}
}
