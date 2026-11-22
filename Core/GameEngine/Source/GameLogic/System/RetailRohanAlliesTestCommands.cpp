#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
void logTransform(const RetailTestCommands::Context& ctx, const Object& object, const char* kind, Int index)
{
	const Coord3D& pos = *object.getPosition();
	const Real ground = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
	std::fprintf(stderr, "TEST rohan-allies %u %s %d %s %d %.3f %.3f %.3f %.6f %.3f\n",
		ctx.frame, ctx.argument, ctx.ordinal, kind, index, pos.x - ctx.position.x,
		pos.y - ctx.position.y, pos.z, object.getOrientation(), ground);
}

void probeSummonedHorde(const RetailTestCommands::Context& ctx)
{
	const Object* anchor = ctx.findObject(true);
	if (!anchor)
		return;
	logTransform(ctx, *anchor, "anchor", 0);
	if (const HordeContain* horde = HordeContain::get(anchor))
	{
		Int index = 0;
		for (const Object* member : *horde->getContainedItemsList())
			if (member && !member->isEffectivelyDead())
				logTransform(ctx, *member, "member", index++);
	}
	std::fflush(stderr);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailRohanAlliesTestCommands()
{
#if !RTS_GENERALS
	registerAction("rohan-allies-trace", probeSummonedHorde);
#endif
}
}
