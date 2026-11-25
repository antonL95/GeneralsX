#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/Pathfinder/Path.h"
#include "GameLogic/Pathfinder/PathNode.h"
#include "WWMath/wwmath.h"

namespace
{
Real distanceBetween(const Coord3D& a, const Coord3D& b)
{
	const Real dx = a.x - b.x;
	const Real dy = a.y - b.y;
	return WWMath::SqrtfOrigin(dx * dx + dy * dy);
}

void probeYieldStall(const RetailTestCommands::Context& ctx)
{
	Object* anchor = ctx.requireObject();
	if (!anchor)
		return;

	Coord3D destination = {0.0f, 0.0f, 0.0f};
	if (std::sscanf(ctx.argument, "%f,%f", &destination.x, &destination.y) != 2)
		return;

	AIUpdateInterface* ai = anchor->getAI();
	const HordeContain* horde = HordeContain::get(anchor);
	unsigned int members = 0;
	unsigned int idleShort = 0;
	Real worstSlot = 0.0f;
	Real worstDestination = 0.0f;
	if (horde)
	{
		for (const Object* member : *horde->getContainedItemsList())
		{
			if (member->isEffectivelyDead() || horde->getMemberSlot(member->getID()) < 0)
				continue;
			++members;
			const Coord3D slot = horde->getMemberPosition(member->getID(), FALSE);
			const Real slotError = distanceBetween(*member->getPosition(), slot);
			const Real destinationError = distanceBetween(*member->getPosition(), destination);
			if (slotError > worstSlot)
				worstSlot = slotError;
			if (destinationError > worstDestination)
				worstDestination = destinationError;
		}
	}
	else
	{
		members = 1;
		worstDestination = distanceBetween(*anchor->getPosition(), destination);
	}
	const PathNode* lastNode = ai && ai->getPath() ? ai->getPath()->getLastNode() : nullptr;
	const Coord3D* pathEnd = lastNode ? lastNode->getPosition() : nullptr;
	const Bool idle = ai && ai->isIdle();
	if (idle && distanceBetween(*anchor->getPosition(), destination) > 40.0f)
		idleShort = 1;
	std::fprintf(stderr, "TEST yield-stall %u %s %s members=%u anchor=%.3f slot=%.3f far=%.3f idle=%d idleshort=%u state=%d pos=%.1f,%.1f goal=%.1f,%.1f end=%.1f,%.1f\n",
		ctx.frame, anchor->getTemplate()->getName().str(), ctx.argument, members,
		distanceBetween(*anchor->getPosition(), destination), worstSlot, worstDestination,
		static_cast<int>(idle), idleShort, ai ? static_cast<int>(ai->getCurrentStateID()) : -1,
		anchor->getPosition()->x, anchor->getPosition()->y, ai ? ai->getGoalPosition()->x : 0.0f, ai ? ai->getGoalPosition()->y : 0.0f,
		pathEnd ? pathEnd->x : 0.0f, pathEnd ? pathEnd->y : 0.0f);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailYieldStallTestCommands()
{
#if !RTS_GENERALS
	registerAction("yield-stall-probe", probeYieldStall);
#endif
}
}
