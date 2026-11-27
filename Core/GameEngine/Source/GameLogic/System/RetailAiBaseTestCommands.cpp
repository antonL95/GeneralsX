#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/Player.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include <cstdio>

namespace
{
void probeLayoutSite(const RetailTestCommands::Context& ctx)
{
	const Object* building = ctx.requireObject(true);
	if (!building)
		return;
	AsciiString name;
	name.format("Player_%d_Start", building->getControllingPlayer()->getMpStartIndex() + 1);
	const Waypoint* start = TheTerrainLogic->getWaypointByName(name);
	if (!start)
		return;
	const Coord3D offset = *building->getPosition() - *start->getLocation();
	std::fprintf(stderr, "TEST ai-base frame=%u offset=%.2f,%.2f angle=%.4f\n", ctx.frame,
		offset.x, offset.y, building->getOrientation());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailAiBaseTestCommands()
{
#if !RTS_GENERALS
	registerAction("ai-base-site", probeLayoutSite);
#endif
}
}
