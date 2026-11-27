#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#include <cstdio>

#if !RTS_GENERALS
#include "Common/BuildAssistant.h"
#include "Common/ThingFactory.h"

namespace
{
void buildTerrainClaimFarm(const RetailTestCommands::Context& ctx)
{
	Object* builder = ctx.requireObject();
	const ThingTemplate* farm = TheThingFactory->findTemplate(ctx.argument, FALSE);
	if (!builder || !farm || !ctx.player || !ctx.hasPosition)
		return;
	const Coord3D location = ctx.groundLocation();
	const Object* foundation = TheBuildAssistant->buildObjectNow(builder, farm, &location, ctx.angle, ctx.player);
	std::fprintf(stderr, "TEST terrain-claim build frame=%u placed=%d\n", ctx.frame, foundation != nullptr);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailTerrainClaimTestCommands()
{
#if !RTS_GENERALS
	registerAction("terrain-claim-build", buildTerrainClaimFarm);
#endif
}
}
