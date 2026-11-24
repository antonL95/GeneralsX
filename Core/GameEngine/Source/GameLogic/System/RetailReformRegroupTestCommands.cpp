#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "WWMath/wwmath.h"

namespace
{
void logRegroup(const RetailTestCommands::Context& ctx)
{
	const Object* anchor = ctx.requireObject();
	const HordeContain* horde = HordeContain::get(anchor);
	const PhysicsBehavior* physics = anchor ? anchor->getPhysics() : nullptr;
	if (!horde || !physics)
		return;
	const Coord3D& velocity = *physics->getVelocity();
	std::fprintf(stderr, "TEST regroup %u facing=%.6f speed=%.6f\n", ctx.frame, anchor->getOrientation(),
		WWMath::SqrtfOrigin(velocity.x * velocity.x + velocity.y * velocity.y));
	for (const Object* member : *horde->getContainedItemsList())
	{
		if (member->isEffectivelyDead() || horde->getMemberSlot(member->getID()) < 0)
			continue;
		const Coord3D slot = horde->getMemberPosition(member->getID(), FALSE);
		const Coord3D& pos = *member->getPosition();
		std::fprintf(stderr, "TEST regroup-member %u %u error=%.6f\n", ctx.frame, member->getID(),
			WWMath::SqrtfOrigin(sqr(pos.x - slot.x) + sqr(pos.y - slot.y)));
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailReformRegroupTestCommands()
{
#if !RTS_GENERALS
	registerAction("reform-regroup", logRegroup);
#endif
}
}
