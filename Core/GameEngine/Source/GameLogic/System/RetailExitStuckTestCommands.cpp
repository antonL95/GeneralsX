#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstring>
#include <map>

#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/Pathfinder/Path.h"
#include "GameLogic/Pathfinder/PathNode.h"
#include "WWMath/wwmath.h"

namespace
{
std::map<ObjectID, Coord3D> s_targets;
std::map<ObjectID, int> s_orders;

Real planarDistance(const Coord3D& a, const Coord3D& b)
{
	const Real dx = a.x - b.x;
	const Real dy = a.y - b.y;
	return WWMath::SqrtfOrigin(dx * dx + dy * dy);
}

Bool matchesFilter(const Object* anchor, const char* filter)
{
	return !filter[0] || std::strcmp(filter, "-") == 0 || std::strcmp(anchor->getTemplate()->getName().str(), filter) == 0;
}

Bool isProducedAnchor(const Object* anchor, const Object* producer, const char* filter)
{
	return anchor->getProducerID() == producer->getID() && anchor->getContain() && HordeContain::get(anchor) && matchesFilter(anchor, filter);
}

void orderAnchor(Object* anchor, const Coord3D& destination)
{
	s_targets[anchor->getID()] = destination;
	++s_orders[anchor->getID()];
	anchor->getAI()->aiMoveToPosition(&destination, CMD_FROM_PLAYER);
}

void sampleExitStuck(const RetailTestCommands::Context& ctx)
{
	Object* producer = ctx.requireObject();
	if (!producer)
		return;
	char filter[96] = "";
	std::sscanf(ctx.argument, "%95s", filter);
	const Real footprint = producer->getGeometryInfo().getBoundingCircleRadius();
	for (const Object* anchor = TheGameLogic->getFirstObject(); anchor; anchor = anchor->getNextObject())
	{
		if (!isProducedAnchor(anchor, producer, filter))
			continue;
		const HordeContain* horde = HordeContain::get(anchor);
		const auto target = s_targets.find(anchor->getID());
		const Bool hasTarget = target != s_targets.end();
		const AIUpdateInterface* anchorAI = anchor->getAI();
		std::fprintf(stderr, "TEST exit-stuck-anchor %u %s %u orders=%d state=%d moving=%d pos=%.1f,%.1f target=%.1f,%.1f\n", ctx.frame,
			anchor->getTemplate()->getName().str(), static_cast<unsigned>(anchor->getID()), hasTarget ? s_orders[anchor->getID()] : 0,
			anchorAI ? static_cast<int>(anchorAI->getCurrentStateID()) : -1, anchorAI ? static_cast<int>(anchorAI->isMoving()) : 0,
			anchor->getPosition()->x, anchor->getPosition()->y, hasTarget ? target->second.x : 0.0f, hasTarget ? target->second.y : 0.0f);
		for (Object* member : *horde->getContainedItemsList())
		{
			if (member->isEffectivelyDead() || horde->getMemberSlot(member->getID()) < 0)
				continue;
			const Coord3D* pos = member->getPosition();
			const Coord3D slot = horde->getMemberPosition(member->getID(), FALSE);
			AIUpdateInterface* ai = member->getAI();
			Path* path = ai ? ai->getPath() : nullptr;
			const PathNode* lastNode = path ? path->getLastNode() : nullptr;
			std::fprintf(stderr,
				"TEST exit-stuck %u %s %u anchor=%u slotdist=%.1f producerdist=%.1f footprint=%.1f destdist=%.1f state=%d blocked=%d stuck=%d waiting=%d moving=%d held=%d stealth=%d goal=%.1f,%.1f slot=%.1f,%.1f pos=%.1f,%.1f path=%d end=%.1f,%.1f\n",
				ctx.frame, producer->getTemplate()->getName().str(), static_cast<unsigned>(member->getID()),
				static_cast<unsigned>(anchor->getID()), planarDistance(*pos, slot), planarDistance(*pos, *producer->getPosition()),
				footprint, hasTarget ? planarDistance(*pos, target->second) : -1.0f,
				ai ? static_cast<int>(ai->getCurrentStateID()) : -1, ai ? static_cast<int>(ai->getNumFramesBlocked()) : 0,
				ai ? static_cast<int>(ai->isBlockedAndStuck()) : 0, ai ? static_cast<int>(ai->isWaitingForPath()) : 0,
				ai ? static_cast<int>(ai->isMoving()) : 0, static_cast<int>(member->isDisabledByType(DISABLED_HELD)),
				static_cast<int>(member->testStatus(OBJECT_STATUS_STEALTHED)),
				ai ? ai->getGoalPosition()->x : 0.0f, ai ? ai->getGoalPosition()->y : 0.0f, slot.x, slot.y, pos->x, pos->y,
				path ? 1 : 0, lastNode ? lastNode->getPosition()->x : 0.0f, lastNode ? lastNode->getPosition()->y : 0.0f);
		}
	}
}

void moveExitStuckHordes(const RetailTestCommands::Context& ctx)
{
	Object* producer = ctx.requireObject();
	if (!producer)
		return;
	Coord3D destination = {0.0f, 0.0f, 0.0f};
	char filter[96] = "";
	if (std::sscanf(ctx.argument, "%f,%f,%95s", &destination.x, &destination.y, filter) < 2)
		return;
	for (Object* anchor = TheGameLogic->getFirstObject(); anchor; anchor = anchor->getNextObject())
	{
		if (!isProducedAnchor(anchor, producer, filter) || !anchor->getAI())
			continue;
		orderAnchor(anchor, destination);
	}
}

void moveExitStuckLaggard(const RetailTestCommands::Context& ctx)
{
	Object* producer = ctx.requireObject();
	if (!producer)
		return;
	Coord3D offset = {0.0f, 0.0f, 0.0f};
	char filter[96] = "";
	if (std::sscanf(ctx.argument, "%f,%f,%95s", &offset.x, &offset.y, filter) < 2)
		return;
	for (Object* anchor = TheGameLogic->getFirstObject(); anchor; anchor = anchor->getNextObject())
	{
		if (!isProducedAnchor(anchor, producer, filter) || !anchor->getAI())
			continue;
		const HordeContain* horde = HordeContain::get(anchor);
		const Object* laggard = nullptr;
		Real worst = -1.0f;
		for (const Object* member : *horde->getContainedItemsList())
		{
			if (member->isEffectivelyDead() || horde->getMemberSlot(member->getID()) < 0)
				continue;
			const Real distance = planarDistance(*member->getPosition(), *anchor->getPosition());
			if (distance > worst)
			{
				worst = distance;
				laggard = member;
			}
		}
		if (!laggard)
			continue;
		Coord3D destination = *laggard->getPosition();
		destination.x += offset.x;
		destination.y += offset.y;
		std::fprintf(stderr, "TEST exit-stuck-laggard %u %s %u laggard=%u at=%.1f,%.1f order=%.1f,%.1f\n", ctx.frame,
			anchor->getTemplate()->getName().str(), static_cast<unsigned>(anchor->getID()), static_cast<unsigned>(laggard->getID()),
			laggard->getPosition()->x, laggard->getPosition()->y, destination.x, destination.y);
		orderAnchor(anchor, destination);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailExitStuckTestCommands()
{
#if !RTS_GENERALS
	registerAction("exit-stuck-sample", sampleExitStuck);
	registerAction("exit-stuck-move", moveExitStuckHordes);
	registerAction("exit-stuck-laggard", moveExitStuckLaggard);
#endif
}
}
