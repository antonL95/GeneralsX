#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <algorithm>
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "WWMath/wwmath.h"

namespace
{
Real distanceSquared(const Coord3D& a, const Coord3D& b)
{
	const Real dx = a.x - b.x;
	const Real dy = a.y - b.y;
	return dx * dx + dy * dy;
}

void logHordeTurn(const RetailTestCommands::Context& ctx)
{
	const Object* anchor = ctx.requireObject();
	const HordeContain* horde = HordeContain::get(anchor);
	if (!horde)
		return;

	const ModelConditionFlagType conditions[] = {
		MODELCONDITION_MOVING, MODELCONDITION_TURN_LEFT, MODELCONDITION_TURN_RIGHT,
		MODELCONDITION_ACCELERATE, MODELCONDITION_DECELERATE, MODELCONDITION_ALTERNATE_FORMATION
	};
	unsigned int counts[sizeof conditions / sizeof *conditions] = {};
	unsigned int drawables = 0;
	unsigned int drawableMismatches = 0;
	unsigned int members = 0;
	unsigned int overlaps = 0;
	Real error = 0.0f;
	const auto& contents = *horde->getContainedItemsList();
	for (const Object* member : contents)
	{
		if (member->isEffectivelyDead() || horde->getMemberSlot(member->getID()) < 0)
			continue;
		++members;
		const Drawable* drawable = member->getDrawable();
		drawables += drawable != nullptr;
		const auto& position = *member->getPosition();
		error = std::max(error, distanceSquared(position, horde->getMemberPosition(member->getID(), FALSE)));
		for (const Object* other : contents)
		{
			if (other->getID() <= member->getID() || other->isEffectivelyDead() ||
				horde->getMemberSlot(other->getID()) < 0)
				continue;
			const Real diameter = member->getGeometryInfo().getBoundingCircleRadius() +
				other->getGeometryInfo().getBoundingCircleRadius();
			if (distanceSquared(position, *other->getPosition()) < diameter * diameter)
				++overlaps;
		}
		for (unsigned int index = 0; index < sizeof conditions / sizeof *conditions; ++index)
		{
			const Bool enabled = member->getLogicModelConditions().test(conditions[index]);
			counts[index] += enabled;
			if (drawable)
				drawableMismatches += enabled != drawable->getModelConditionFlags().test(conditions[index]);
		}
	}
	std::fprintf(stderr, "TEST horde-turn %u %s %u %u %.3f %.4f %u %u %u %u %u %u %u %u\n",
		ctx.frame, anchor->getTemplate()->getName().str(), members, overlaps, WWMath::SqrtfOrigin(error),
		anchor->getOrientation(), counts[0], counts[1], counts[2], counts[3], counts[4], counts[5], drawables, drawableMismatches);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailHordeTurnTestCommands()
{
#if !RTS_GENERALS
	registerAction("horde-turn", logHordeTurn);
#endif
}
}
