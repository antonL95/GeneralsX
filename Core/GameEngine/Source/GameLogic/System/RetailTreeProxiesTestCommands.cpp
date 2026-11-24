#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include "Common/GameState.h"
#include "Common/GameType.h"
#include "Common/MapObject.h"
#include "Common/ThingTemplate.h"
#include "Common/MessageStream.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
void inspectTree(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;
	if (ctx.action == "tree-proxies-order")
	{
		GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_RETAIL_HORDE_COMMAND);
		msg->appendObjectIDArgument(source->getID());
		msg->appendIntegerArgument(4);
		msg->appendIntegerArgument(-1);
		return;
	}
	const auto* contain = source->getContain();
	const auto* items = contain ? contain->getContainedItemsList() : nullptr;
	const Object* held = items && !items->empty() ? items->front() : nullptr;
	const MapObject* closest = nullptr;
	Real best = 600.0f * 600.0f;
	for (const MapObject* map = MapObject::getFirstMapObject(); map; map = map->getNext())
	{
		const ThingTemplate* type = map->getThingTemplate();
		if (!type || !type->isKindOf(KINDOF_TREE) || !type->isKindOf(KINDOF_CLUB) ||
			!type->getRotWKObjectData().isGrabbable)
			continue;
		const Coord3D* pos = map->getLocation();
		const Real distance = sqr(pos->x - 145.0f) + sqr(pos->y - 2295.0f);
		if (distance < best)
		{
			best = distance;
			closest = map;
		}
	}
	const Bool standing = closest && TheTerrainLogic->isNearMapTree(*closest->getLocation(), 0.0f);
	std::fprintf(stderr, "TEST tree-proxies frame=%u held=%s tree=%d standing=%d same=%d\n", ctx.frame,
		held ? held->getTemplate()->getName().str() : "-",
		held && held->isKindOf(KINDOF_TREE), standing,
		held && closest && held->getTemplate() == closest->getThingTemplate());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailTreeProxiesTestCommands()
{
#if !RTS_GENERALS
	registerAction("tree-proxies-state", inspectTree);
	registerAction("tree-proxies-order", inspectTree);
#endif
}
}
