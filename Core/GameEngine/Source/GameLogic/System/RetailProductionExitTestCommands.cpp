#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Object.h"

namespace
{
void queueProductionExit(const RetailTestCommands::Context& ctx)
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

void sampleProductionExit(const RetailTestCommands::Context& ctx)
{
	Object* producer = ctx.requireObject();
	if (!producer || !producer->getObjectExitInterface())
		return;
	Coord3D exit;
	if (!producer->getObjectExitInterface()->getExitPosition(exit))
		return;
	Coord3D natural;
	if (!producer->getObjectExitInterface()->getNaturalRallyPoint(natural))
		return;
	for (const Object* horde = TheGameLogic->getFirstObject(); horde; horde = horde->getNextObject())
	{
		if (horde->getProducerID() != producer->getID() || !horde->getContain())
			continue;
		const auto* members = horde->getContain()->getContainedItemsList();
		if (!members)
			continue;
		for (const Object* member : *members)
		{
			const Coord3D* pos = member->getPosition();
			const AIUpdateInterface* ai = member->getAI();
			std::fprintf(stderr, "TEST production-exit %u %s %u %.3f %.3f %.3f %.3f %.3f %.3f state=%d held=%d mobile=%d\n", ctx.frame,
				producer->getTemplate()->getName().str(), static_cast<unsigned>(member->getID()),
				pos->x, pos->y, exit.x, exit.y, natural.x, natural.y, ai ? static_cast<Int>(ai->getCurrentStateID()) : -1,
				member->isDisabledByType(DISABLED_HELD), member->isMobile());
		}
	}
}

void sampleProductionCavalry(const RetailTestCommands::Context& ctx)
{
	const Object* horde = ctx.requireObject();
	const auto* members = horde && horde->getContain() ? horde->getContain()->getContainedItemsList() : nullptr;
	if (!members || members->empty())
		return;
	const Object* member = members->front();
	const AIUpdateInterface* ai = member->getAI();
	const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
	if (!locomotor || !member->getBodyModule())
		return;
	const Coord3D* pos = member->getPosition();
	std::fprintf(stderr, "TEST production-cavalry %u %.3f %.3f %.3f\n", ctx.frame, pos->x, pos->y,
		locomotor->getMaxSpeedForCondition(member->getBodyModule()->getDamageState()) * static_cast<Real>(LOGICFRAMES_PER_SECOND));
}

void sampleRetiredProducerHorde(const RetailTestCommands::Context& ctx)
{
	const Object* horde = ctx.requireObject();
	if (!horde)
		return;
	const Coord3D* pos = horde->getPosition();
	const auto* members = horde->getContain() ? horde->getContain()->getContainedItemsList() : nullptr;
	std::fprintf(stderr, "TEST production-exit-retired %u %.3f %.3f selectable=%d collisions=%d members=%zu\n",
		ctx.frame, pos->x, pos->y, !horde->testStatus(OBJECT_STATUS_UNSELECTABLE),
		!horde->testStatus(OBJECT_STATUS_NO_COLLISIONS), members ? members->size() : 0);
}

void killProductionMember(const RetailTestCommands::Context& ctx)
{
	static ObjectID tested = INVALID_ID;
	Object* horde = ctx.findObject();
	const auto* members = horde && horde->getContain() ? horde->getContain()->getContainedItemsList() : nullptr;
	if (!members)
		return;
	Bool removed = FALSE;
	if (tested == INVALID_ID && !members->empty())
	{
		Object* member = members->front();
		horde->getContain()->removeFromContain(member);
		member->kill();
		tested = horde->getID();
		removed = TRUE;
	}
	if (tested == horde->getID())
		std::fprintf(stderr, "TEST production-exit-casualty %u removed=%d members=%zu\n", ctx.frame, removed, members->size());
}

void sampleProductionQueue(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	const ProductionUpdateInterface* production = source ? source->getProductionUpdateInterface() : nullptr;
	std::fprintf(stderr, "TEST production-exit-queue-state %u active=%d\n", ctx.frame,
		production && production->firstProduction());
}

void sampleProductionAnchor(const RetailTestCommands::Context& ctx)
{
	std::fprintf(stderr, "TEST production-exit-anchor %u exists=%d\n", ctx.frame, ctx.findObject(true) != nullptr);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailProductionExitTestCommands()
{
#if !RTS_GENERALS
	registerAction("production-exit-queue", queueProductionExit);
	registerAction("production-exit-sample", sampleProductionExit);
	registerAction("production-exit-cavalry", sampleProductionCavalry);
	registerAction("production-exit-retired", sampleRetiredProducerHorde);
	registerAction("production-exit-kill-member", killProductionMember);
	registerAction("production-exit-queue-state", sampleProductionQueue);
	registerAction("production-exit-anchor", sampleProductionAnchor);
#endif
}
}
