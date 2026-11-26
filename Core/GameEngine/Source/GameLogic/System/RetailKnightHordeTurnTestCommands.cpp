#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "WWMath/wwmath.h"

namespace
{
Real distanceSquared2D(const Coord3D& a, const Coord3D& b)
{
	const Real dx = a.x - b.x;
	const Real dy = a.y - b.y;
	return dx * dx + dy * dy;
}

void logKnightTurn(const RetailTestCommands::Context& ctx)
{
	const Object* anchor = ctx.requireObject();
	const HordeContain* horde = HordeContain::get(anchor);
	const PhysicsBehavior* physics = anchor ? anchor->getPhysics() : nullptr;
	const AIUpdateInterface* ai = anchor ? anchor->getAI() : nullptr;
	const Locomotor* loco = ai ? ai->getCurLocomotor() : nullptr;
	if (!horde || !physics || !loco)
		return;

	const Coord3D& vel = *physics->getVelocity();
	const Real speed = WWMath::SqrtfOrigin(vel.x * vel.x + vel.y * vel.y);
	const Real scalar = loco->getRetailScalarSpeed();
	const Real anchorFacing = anchor->getOrientation();
	unsigned int members = 0;
	unsigned int overlaps = 0;
	unsigned int piles = 0;
	Real maxErrorSq = 0.0f;
	Real maxYawDiff = 0.0f;

	const auto& contents = *horde->getContainedItemsList();
	for (const Object* member : contents)
	{
		if (member->isEffectivelyDead() || horde->getMemberSlot(member->getID()) < 0)
			continue;
		++members;
		const Coord3D& pos = *member->getPosition();
		maxErrorSq = std::max(maxErrorSq, distanceSquared2D(pos, horde->getMemberPosition(member->getID(), FALSE)));
		const Real yawDiff = fabs(stdAngleDiff(member->getOrientation(), anchorFacing));
		if (yawDiff > maxYawDiff)
			maxYawDiff = yawDiff;
		for (const Object* other : contents)
		{
			if (other->getID() <= member->getID() || other->isEffectivelyDead() ||
				horde->getMemberSlot(other->getID()) < 0)
				continue;
			const Real diameter = member->getGeometryInfo().getBoundingCircleRadius() +
				other->getGeometryInfo().getBoundingCircleRadius();
			const Real distSq = distanceSquared2D(pos, *other->getPosition());
			if (distSq < diameter * diameter)
				++overlaps;
			if (distSq < 0.25f * diameter * diameter)
				++piles;
		}
	}

	std::fprintf(stderr, "TEST knight-turn %u %s members=%u facing=%.5f speed=%.4f scalar=%.4f maxErr=%.3f maxYawDiff=%.4f overlaps=%u piles=%u\n",
		ctx.frame, anchor->getTemplate()->getName().str(), members, anchorFacing, speed, scalar,
		WWMath::SqrtfOrigin(maxErrorSq), maxYawDiff, overlaps, piles);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailKnightHordeTurnTestCommands()
{
#if !RTS_GENERALS
	registerAction("knight-turn-probe", logKnightTurn);
#endif
}
}
