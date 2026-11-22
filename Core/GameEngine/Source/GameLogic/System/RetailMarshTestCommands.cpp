#include "PreRTS.h"

#include <cstdio>

#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/RetailTestCommands.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
void runRetailMarshWaterTestCommand(const Coord3D& position, Real height, UnsignedInt frame)
{
	const auto* water = TheTerrainLogic->getWaterHandle(position.x, position.y);
	if (!water)
		return;
	TheTerrainLogic->setWaterHeight(water, height, 0.0f, TRUE);
	Real waterHeight = 0.0f;
	TheTerrainLogic->isUnderwater(position.x, position.y, &waterHeight);
	std::fprintf(stderr, "TEST marsh water frame=%u height=%.1f\n", frame, waterHeight);
}

void runRetailMarshRouteTestCommand(const Object* object, const Coord3D& destination, UnsignedInt frame)
{
	const auto* ai = object ? object->getAIUpdateInterface() : nullptr;
	if (!ai)
		return;
	const Bool connected = TheAI->pathfinder()->clientSafeQuickDoesPathExist(
		ai->getLocomotorSet(), object->getPosition(), &destination);
	std::fprintf(stderr, "TEST marsh route frame=%u connected=%d\n", frame, connected);
}
}

namespace RetailTestCommands
{
void registerRetailMarshTestCommands()
{
	registerAction("marsh-water", [](const Context& ctx) {
		if (ctx.hasPosition)
			runRetailMarshWaterTestCommand(ctx.position, ctx.angle, ctx.frame);
	});
	registerAction("marsh-route", [](const Context& ctx) {
		if (ctx.hasPosition)
			runRetailMarshRouteTestCommand(ctx.findObject(true), ctx.groundLocation(), ctx.frame);
	});
}
}
