#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "Common/ThingTemplate.h"
#include "GameLogic/ExperienceTracker.h"
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

void logFountainGuardReform(const RetailTestCommands::Context& ctx)
{
	const Object* anchor = ctx.requireObject();
	const HordeContain* horde = HordeContain::get(anchor);
	const PhysicsBehavior* physics = anchor ? anchor->getPhysics() : nullptr;
	if (!horde || !physics)
		return;

	const Coord3D& vel = *physics->getVelocity();
	const Real speed = WWMath::SqrtfOrigin(vel.x * vel.x + vel.y * vel.y);
	const Real facing = anchor->getOrientation();
	const Real lateral = fabs(-vel.x * Sin(facing) + vel.y * Cos(facing));
	unsigned int members = 0;
	unsigned int piles = 0;
	Real maxErrorSq = 0.0f;

	const auto& contents = *horde->getContainedItemsList();
	for (const Object* member : contents)
	{
		if (member->isEffectivelyDead() || horde->getMemberSlot(member->getID()) < 0)
			continue;
		++members;
		const Coord3D& pos = *member->getPosition();
		maxErrorSq = std::max(maxErrorSq, distanceSquared2D(pos, horde->getMemberPosition(member->getID(), FALSE)));
		for (const Object* other : contents)
		{
			if (other->getID() <= member->getID() || other->isEffectivelyDead() ||
				horde->getMemberSlot(other->getID()) < 0)
				continue;
			const Real diameter = member->getGeometryInfo().getBoundingCircleRadius() +
				other->getGeometryInfo().getBoundingCircleRadius();
			if (distanceSquared2D(pos, *other->getPosition()) < 0.25f * diameter * diameter)
				++piles;
		}
	}

	std::fprintf(stderr, "TEST fg-reform %u %s alt=%d members=%u facing=%.5f speed=%.4f lateral=%.4f maxErr=%.3f piles=%u\n",
		ctx.frame, anchor->getTemplate()->getName().str(), horde->isAlternateFormation() ? 1 : 0,
		members, facing, speed, lateral, WWMath::SqrtfOrigin(maxErrorSq), piles);
}

void setFountainGuardRank(const RetailTestCommands::Context& ctx)
{
	Object* anchor = ctx.requireObject();
	if (!anchor || !anchor->getExperienceTracker())
		return;
	const char* raw = (ctx.argument && *ctx.argument) ? ctx.argument : ctx.targetName;
	const Int rank = raw ? std::atoi(raw) : 0;
	if (rank > 0)
		anchor->getExperienceTracker()->setMinRetailRank(rank);
}

void checkFountainGuardBanner(const RetailTestCommands::Context& ctx)
{
	const Object* anchor = ctx.requireObject();
	const HordeContain* horde = HordeContain::get(anchor);
	if (!horde || !anchor->getExperienceTracker())
		return;
	Bool hasBanner = FALSE;
	for (const Object* member : *horde->getContainedItemsList())
	{
		if (!member->isEffectivelyDead() && horde->isBannerCarrier(member))
		{
			hasBanner = TRUE;
			break;
		}
	}
	std::fprintf(stderr, "TEST fg-banner %u %s rank=%d hasBanner=%d count=%u\n",
		ctx.frame, anchor->getTemplate()->getName().str(),
		anchor->getExperienceTracker()->getRank(), hasBanner ? 1 : 0, horde->getContainCount());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailFountainGuardReformTestCommands()
{
#if !RTS_GENERALS
	registerAction("fg-reform-probe", logFountainGuardReform);
	registerAction("fg-set-rank", setFountainGuardRank);
	registerAction("fg-banner-check", checkFountainGuardBanner);
#endif
}
}
