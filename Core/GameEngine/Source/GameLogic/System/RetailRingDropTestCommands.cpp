#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cmath>
#include <cstdio>
#include <string_view>

#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
void handleRingDrop(const RetailTestCommands::Context& ctx)
{
	const std::string_view action(ctx.action);
	const Object* ring = nullptr;
	const Object* holder = nullptr;
	const Object* fortress = nullptr;
	Int rings = 0;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->isKindOf(KINDOF_ONE_RING))
		{
			ring = object;
			++rings;
		}
		if (object->testStatus(OBJECT_STATUS_HOLDING_THE_RING))
			holder = object;
		if (object->getControllingPlayer() == ctx.player && object->isKindOf(KINDOF_COMMANDCENTER) &&
			object->isKindOf(KINDOF_CASTLE_KEEP) && (!fortress || object->getID() < fortress->getID()))
			fortress = object;
	}

	if (action == "ring-drop-state")
	{
		const Real distance = ring && fortress ? std::hypot(ring->getPosition()->x - fortress->getPosition()->x,
			ring->getPosition()->y - fortress->getPosition()->y) : -1.0f;
		std::fprintf(stderr, "TEST ring-drop frame=%u rings=%d near-fortress=%d holder=%s\n", ctx.frame, rings,
			ring && fortress && distance <= fortress->getVisionRange(),
			holder ? holder->getTemplate()->getName().str() : "-");
		return;
	}
	if (!ring)
		return;

	if (action == "ring-drop-spawn")
	{
		const ThingTemplate* what = TheThingFactory->findTemplate(ctx.targetName, FALSE);
		if (!what || !ctx.player)
			return;
		Object* carrier = TheThingFactory->newObject(what, ctx.player->getDefaultTeam());
		Coord3D position = *ring->getPosition();
		position.x += 40.0f;
		position.z = TheTerrainLogic->getGroundHeight(position.x, position.y);
		carrier->setPosition(&position);
		return;
	}

	Object* carrier = ctx.requireObject(TRUE);
	if (action == "ring-drop-order" && carrier)
	{
		ctx.selectObject(carrier);
		TheMessageStream->appendMessage(GameMessage::MSG_DO_MOVETO)->appendLocationArgument(*ring->getPosition());
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailRingDropTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("ring-drop-", handleRingDrop);
#endif
}
}
