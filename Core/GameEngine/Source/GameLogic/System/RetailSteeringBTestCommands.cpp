#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <algorithm>
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "WWMath/wwmath.h"

namespace
{
void logOrderReaction(const RetailTestCommands::Context& ctx)
{
	const Object* anchor = ctx.requireObject();
	const HordeContain* horde = HordeContain::get(anchor);
	if (!horde)
		return;

	const AIUpdateInterface* ai = anchor->getAI();
	const PhysicsBehavior* physics = anchor->getPhysics();
	if (!ai || !physics)
		return;

	Real peakErrorSquared = 0.0f;
	for (const Object* member : *horde->getContainedItemsList())
	{
		if (member->isEffectivelyDead() || horde->getMemberSlot(member->getID()) < 0)
			continue;
		const Coord3D slot = horde->getMemberPosition(member->getID(), FALSE);
		const Coord3D& pos = *member->getPosition();
		peakErrorSquared = std::max(peakErrorSquared, sqr(pos.x - slot.x) + sqr(pos.y - slot.y));
	}

	const Coord3D& position = *anchor->getPosition();
	const Coord3D& velocity = *physics->getVelocity();
	std::fprintf(stderr, "TEST steering-reaction %u %s pos=%.4f,%.4f facing=%.5f velocity=%.5f,%.5f path=%d waiting=%d groupRoute=%d blocked=%d reform=%d slotError=%.3f reformTravel=%u\n",
		ctx.frame, anchor->getTemplate()->getName().str(), position.x, position.y, anchor->getOrientation(),
		velocity.x, velocity.y, static_cast<Int>(ai->getPath() != nullptr), static_cast<Int>(ai->isWaitingForPath()),
		static_cast<Int>(ai->isFollowingRetailGroupRoute()), ai->getNumFramesBlocked(),
		static_cast<Int>(HordeAIUpdate::isReforming(ai)), WWMath::SqrtfOrigin(peakErrorSquared), horde->getFormationReformTravelFrames());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailSteeringBTestCommands()
{
#if !RTS_GENERALS
	registerAction("steering-reaction", logOrderReaction);
#endif
}
}
