#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <algorithm>
#include <cstdio>

#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
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

Bool inChoke(const Coord3D& position)
{
	return position.x >= 2555.0f && position.x <= 2580.0f &&
		position.y > 2475.0f && position.y < 2580.0f;
}

void logHordeChoke(const RetailTestCommands::Context& ctx)
{
	const Object* anchor = ctx.requireObject(true);
	const HordeContain* horde = HordeContain::get(anchor);
	if (!horde)
		return;

	const auto& members = *horde->getContainedItemsList();
	const auto& slots = horde->getActiveFormation()->m_layout.slots();
	UnsignedInt active = 0;
	UnsignedInt steering = 0;
	UnsignedInt blockedRoutes = 0;
	UnsignedInt inThroat = 0;
	UnsignedInt throughCorner = 0;
	Real penetration = 0.0f;
	Real slotError = 0.0f;
	for (const Object* member : members)
	{
		const Int slot = horde->getMemberSlot(member->getID());
		if (slot < 0 || static_cast<size_t>(slot) >= slots.size() || member->isEffectivelyDead())
			continue;
		const Coord3D& position = *member->getPosition();
		const AIUpdateInterface* ai = member->getAI();
		++active;
		if (ai && ai->isMoving() && ai->getPath())
		{
			HordeContain::FormationGoal goal;
			blockedRoutes += horde->getFormationGoal(member, goal) && !HordeContain::canSteerToFormationGoal(member, goal);
		}
		steering += ai && ai->isIdle() && member->getLogicModelConditions().test(MODELCONDITION_MOVING);
		inThroat += inChoke(position);
		throughCorner += position.x > 2685.0f && position.y > 2580.0f;
		slotError = std::max(slotError, planarDistance(position, horde->getMemberPosition(member->getID(), FALSE)));
		for (const Object* other : members)
		{
			if (other->getID() <= member->getID() || other->isEffectivelyDead())
				continue;
			const Int otherSlot = horde->getMemberSlot(other->getID());
			const Coord3D& otherPosition = *other->getPosition();
			if (otherSlot < 0 || static_cast<size_t>(otherSlot) >= slots.size() ||
				(!inChoke(position) && !inChoke(otherPosition)))
				continue;
			const auto& a = slots[slot].position;
			const auto& b = slots[otherSlot].position;
			const Real dx = a.x - b.x;
			const Real dy = a.y - b.y;
			const Real spacing = WWMath::SqrtfOrigin(dx * dx + dy * dy);
			const Real diameter = member->getGeometryInfo().getBoundingCircleRadius() +
				other->getGeometryInfo().getBoundingCircleRadius();
			penetration = std::max(penetration, std::min(spacing, diameter) - planarDistance(position, otherPosition));
		}
	}

	const AIUpdateInterface* ai = anchor->getAI();
	const Coord3D& position = *anchor->getPosition();
	std::fprintf(stderr, "TEST horde-choke %u %u %d %u %u %u %.4f %.4f %u %.4f %.4f\n",
		ctx.frame, active, ai && ai->isMoving(), blockedRoutes, inThroat, throughCorner, penetration, slotError, steering,
		position.x, position.y);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailHordeChokeTestCommands()
{
#if !RTS_GENERALS
	registerAction("horde-choke", logHordeChoke);
#endif
}
}
