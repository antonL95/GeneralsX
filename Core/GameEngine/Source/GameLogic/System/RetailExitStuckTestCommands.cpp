#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

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
Real planarDistance(const Coord3D& a, const Coord3D& b)
{
	const Real dx = a.x - b.x;
	const Real dy = a.y - b.y;
	return WWMath::SqrtfOrigin(dx * dx + dy * dy);
}

void sampleExitStuck(const RetailTestCommands::Context& ctx)
{
	Object* producer = ctx.requireObject();
	if (!producer)
		return;
	Coord3D destination = {0.0f, 0.0f, 0.0f};
	const Bool hasDestination = std::sscanf(ctx.argument, "%f,%f", &destination.x, &destination.y) == 2;
	const Real footprint = producer->getGeometryInfo().getBoundingCircleRadius();
	for (const Object* anchor = TheGameLogic->getFirstObject(); anchor; anchor = anchor->getNextObject())
	{
		if (anchor->getProducerID() != producer->getID() || !anchor->getContain())
			continue;
		const HordeContain* horde = HordeContain::get(anchor);
		if (!horde)
			continue;
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
				"TEST exit-stuck %u %s %u anchor=%u slotdist=%.1f producerdist=%.1f footprint=%.1f destdist=%.1f state=%d blocked=%d stuck=%d waiting=%d moving=%d held=%d goal=%.1f,%.1f slot=%.1f,%.1f pos=%.1f,%.1f path=%d end=%.1f,%.1f\n",
				ctx.frame, producer->getTemplate()->getName().str(), static_cast<unsigned>(member->getID()),
				static_cast<unsigned>(anchor->getID()), planarDistance(*pos, slot), planarDistance(*pos, *producer->getPosition()),
				footprint, hasDestination ? planarDistance(*pos, destination) : -1.0f,
				ai ? static_cast<int>(ai->getCurrentStateID()) : -1, ai ? static_cast<int>(ai->getNumFramesBlocked()) : 0,
				ai ? static_cast<int>(ai->isBlockedAndStuck()) : 0, ai ? static_cast<int>(ai->isWaitingForPath()) : 0,
				ai ? static_cast<int>(ai->isMoving()) : 0, static_cast<int>(member->isDisabledByType(DISABLED_HELD)),
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
	if (std::sscanf(ctx.argument, "%f,%f", &destination.x, &destination.y) != 2)
		return;
	for (Object* anchor = TheGameLogic->getFirstObject(); anchor; anchor = anchor->getNextObject())
	{
		if (anchor->getProducerID() != producer->getID() || !anchor->getContain() || !anchor->getAI())
			continue;
		anchor->getAI()->aiMoveToPosition(&destination, CMD_FROM_PLAYER);
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
#endif
}
}
