#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include "Common/GameState.h"
#include "Common/GameType.h"
#include "Common/MapObject.h"
#include "Common/ThingTemplate.h"
#include "Common/MessageStream.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameClient/CommandXlat.h"
#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"
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
	if (ctx.action == "tree-proxies-direct")
	{
		const MapObject* closest = nullptr;
		Real best = 600.0f * 600.0f;
		for (const MapObject* map = MapObject::getFirstMapObject(); map; map = map->getNext())
		{
			const ThingTemplate* type = map->getThingTemplate();
			const Coord3D* pos = map->getLocation();
			if (!type || !type->isKindOf(KINDOF_TREE) || !type->isKindOf(KINDOF_CLUB) ||
				!type->getRotWKObjectData().isGrabbable || !TheTerrainLogic->isNearMapTree(*pos, 0.0f))
				continue;
			const Real distance = sqr(pos->x - source->getPosition()->x) + sqr(pos->y - source->getPosition()->y);
			if (distance < best)
			{
				best = distance;
				closest = map;
			}
		}
		const auto* button = TheRetailCommandCatalog->findCommandButton("Command_SpecialAbilityTrollGrabTree");
		if (closest && button)
		{
			Coord3D pos = *closest->getLocation();
			pos.z += TheTerrainLogic->getGroundHeight(pos.x, pos.y);
			ctx.selectObject(source);
			Object* occupied = ctx.findTarget(std::string_view("Player_1:TreeProxiesGrabber:0"));
			if (occupied && occupied->getDrawable())
				TheInGameUI->selectDrawable(occupied->getDrawable());
			TheInGameUI->setGUICommand(button);
			CommandTranslator translator;
			const Bool blockedFirst = occupied && occupied->getContain() && occupied->getContain()->getContainCount() > 0 &&
				TheInGameUI->getFirstSelectedDrawable() == occupied->getDrawable();
			const auto result = translator.evaluateContextCommand(nullptr, &pos, CommandTranslator::DO_COMMAND);
			std::fprintf(stderr, "TEST tree-proxies direct order frame=%u sent=%d blocked-first=%d\n", ctx.frame,
				result == GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT, blockedFirst);
		}
		return;
	}
	if (ctx.action == "tree-proxies-held")
	{
		const auto* contain = source->getContain();
		const auto* items = contain ? contain->getContainedItemsList() : nullptr;
		const Object* held = items && !items->empty() ? items->front() : nullptr;
		const Coord3D position{141.802963f, 2209.429688f, 0.0f};
		Bool scaleMatches = FALSE;
		for (MapObject* map = MapObject::getFirstMapObject(); held && map; map = map->getNext())
		{
			const Coord3D* point = map->getLocation();
			if (sqr(point->x - position.x) + sqr(point->y - position.y) >= 1.0f || map->getThingTemplate() != held->getTemplate())
				continue;
			Bool hasScale = FALSE;
			const Real mapScale = map->getProperties()->getReal(NAMEKEY("objectPrototypeScale"), &hasScale);
			const Real expected = held->getTemplate()->getAssetScale() * (hasScale ? mapScale : 1.0f);
			scaleMatches = held->getDrawable() && held->getDrawable()->getInstanceScale() == expected;
			break;
		}
		std::fprintf(stderr, "TEST tree-proxies direct frame=%u tree=%d standing=%d scale=%d\n", ctx.frame,
			held && held->isKindOf(KINDOF_TREE), TheTerrainLogic->isNearMapTree(position, 1.0f), scaleMatches);
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
	registerAction("tree-proxies-direct", inspectTree);
	registerAction("tree-proxies-held", inspectTree);
#endif
}
}
