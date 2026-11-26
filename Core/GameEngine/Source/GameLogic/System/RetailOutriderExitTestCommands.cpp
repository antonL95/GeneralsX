#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <vector>

#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object.h"
#include "WWMath/wwmath.h"

namespace
{
Real planarDistance(const Coord3D& a, const Coord3D& b)
{
	const Real dx = a.x - b.x;
	const Real dy = a.y - b.y;
	return WWMath::SqrtfOrigin(dx * dx + dy * dy);
}

void queueOutriderExit(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	const ThingTemplate* unit = TheThingFactory->findTemplate(ctx.argument, FALSE);
	ProductionUpdateInterface* production = source ? source->getProductionUpdateInterface() : nullptr;
	if (!source || !unit || !production)
		return;
	const PlayerIndex player = source->getControllingPlayer()->getPlayerIndex();
	GameMessage* select = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
	select->friend_setPlayerIndex(player);
	select->appendBooleanArgument(TRUE);
	select->appendObjectIDArgument(source->getID());
	GameMessage* queue = TheMessageStream->appendMessage(GameMessage::MSG_QUEUE_UNIT_CREATE);
	queue->friend_setPlayerIndex(player);
	queue->appendIntegerArgument(unit->getTemplateID());
	queue->appendIntegerArgument(production->requestUniqueUnitID());
}

void moveOutriderExit(const RetailTestCommands::Context& ctx)
{
	Object* producer = ctx.requireObject();
	if (!producer)
		return;
	Coord3D destination = {0.0f, 0.0f, 0.0f};
	if (std::sscanf(ctx.argument, "%f,%f", &destination.x, &destination.y) < 2)
		return;
	std::vector<const Object*> anchors;
	for (const Object* anchor = TheGameLogic->getFirstObject(); anchor; anchor = anchor->getNextObject())
	{
		if (anchor->getProducerID() == producer->getID() && anchor->getContain() && HordeContain::get(anchor))
			anchors.push_back(anchor);
	}
	if (anchors.empty())
		return;
	const PlayerIndex player = anchors.front()->getControllingPlayer()->getPlayerIndex();
	GameMessage* select = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
	select->friend_setPlayerIndex(player);
	select->appendBooleanArgument(TRUE);
	for (const Object* anchor : anchors)
		select->appendObjectIDArgument(anchor->getID());
	GameMessage* move = TheMessageStream->appendMessage(GameMessage::MSG_DO_MOVETO);
	move->friend_setPlayerIndex(player);
	move->appendLocationArgument(destination);
}

void sampleOutriderExit(const RetailTestCommands::Context& ctx)
{
	Object* producer = ctx.requireObject();
	if (!producer)
		return;
	for (const Object* anchor = TheGameLogic->getFirstObject(); anchor; anchor = anchor->getNextObject())
	{
		if (anchor->getProducerID() != producer->getID() || !anchor->getContain())
			continue;
		const HordeContain* horde = HordeContain::get(anchor);
		if (!horde)
			continue;
		int members = 0;
		for (const Object* member : *horde->getContainedItemsList())
		{
			if (!member->isEffectivelyDead() && horde->getMemberSlot(member->getID()) >= 0)
				++members;
		}
		std::fprintf(stderr, "TEST outrider-exit-anchor %u %u members=%d moved=%.1f pos=%.1f,%.1f\n",
			ctx.frame, static_cast<unsigned>(anchor->getID()), members,
			planarDistance(*anchor->getPosition(), *producer->getPosition()),
			anchor->getPosition()->x, anchor->getPosition()->y);
		for (Object* member : *horde->getContainedItemsList())
		{
			if (member->isEffectivelyDead() || horde->getMemberSlot(member->getID()) < 0)
				continue;
			const Coord3D* pos = member->getPosition();
			const Coord3D slot = horde->getMemberPosition(member->getID(), FALSE);
			const AIUpdateInterface* ai = member->getAI();
			std::fprintf(stderr, "TEST outrider-exit %u %u state=%d slotdist=%.1f proddist=%.1f pos=%.1f,%.1f\n",
				ctx.frame, static_cast<unsigned>(member->getID()),
				ai ? static_cast<int>(ai->getCurrentStateID()) : -1,
				planarDistance(*pos, slot), planarDistance(*pos, *producer->getPosition()),
				pos->x, pos->y);
		}
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailOutriderExitTestCommands()
{
#if !RTS_GENERALS
	registerAction("outrider-exit-queue", queueOutriderExit);
	registerAction("outrider-exit-move", moveOutriderExit);
	registerAction("outrider-exit-sample", sampleOutriderExit);
#endif
}
}
