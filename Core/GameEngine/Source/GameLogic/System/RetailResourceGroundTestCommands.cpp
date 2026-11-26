#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#include <cstdio>

#include "Common/BuildAssistant.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Module/TerrainResourceBehavior.h"
#include "GameLogic/Object.h"

#if !RTS_GENERALS
namespace
{
void handleResourceGround(const RetailTestCommands::Context& ctx)
{
	if (!ctx.player)
		return;
	if (ctx.action == "resground-preview")
	{
		const ThingTemplate* type = TheThingFactory->findTemplate(ctx.argument, FALSE);
		Real radius = 0.0f;
		Real efficiency = 0.0f;
		if (type && TerrainResourceBehavior::getDisplayInfo(*type, ctx.position, *ctx.player, nullptr, radius, efficiency))
			std::fprintf(stderr, "TEST resground preview frame=%u x=%.0f efficiency=%.3f\n", ctx.frame,
				ctx.position.x, efficiency);
		return;
	}
	Object* object = ctx.requireObject();
	if (!object)
		return;
	if (ctx.action == "resground-kill")
	{
		object->kill();
		return;
	}
	Real radius = 0.0f;
	Real efficiency = 0.0f;
	if (TerrainResourceBehavior::getDisplayInfo(*object->getTemplate(), *object->getPosition(), *ctx.player, object,
		radius, efficiency))
		std::fprintf(stderr, "TEST resground site frame=%u x=%.0f complete=%d progress=%.1f efficiency=%.3f\n", ctx.frame,
			object->getPosition()->x, object->getConstructionPercent() == CONSTRUCTION_COMPLETE,
			object->getConstructionPercent(), efficiency);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailResourceGroundTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("resground-", handleResourceGround);
#endif
}
}
