#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Object.h"

namespace
{
void checkRoute(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object || !ctx.hasPosition)
		return;

	const Coord3D target = ctx.groundLocation();
	const Bool found = TheAI->pathfinder()->slowDoesPathExist(object, object->getPosition(), &target);
	std::fprintf(stderr, "TEST pathfind-budget %u %s found=%d\n", ctx.frame, ctx.argument, static_cast<Int>(found));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPathfindBudgetTestCommands()
{
#if !RTS_GENERALS
	registerAction("pathfind-budget-route", checkRoute);
#endif
}
}
