#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <algorithm>
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "WWMath/wwmath.h"

namespace
{
Real planarSpeed(const Object& object)
{
	const PhysicsBehavior* physics = object.getPhysics();
	if (!physics)
		return 0.0f;
	const Coord3D& velocity = *physics->getVelocity();
	return WWMath::SqrtfOrigin(velocity.x * velocity.x + velocity.y * velocity.y) * LOGICFRAMES_PER_SECOND;
}

Real authoredSpeed(const Object& object)
{
	const AIUpdateInterface* ai = object.getAI();
	const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
	return locomotor ? locomotor->getMaxSpeedForCondition(object.getBodyModule()->getDamageState(), &object) *
		LOGICFRAMES_PER_SECOND : 0.0f;
}

void logHordeSpeed(const RetailTestCommands::Context& ctx)
{
	const Object* anchor = ctx.requireObject();
	const HordeContain* horde = HordeContain::get(anchor);
	if (!horde)
		return;

	unsigned int members = 0;
	Real worst = 0.0f;
	Real total = 0.0f;
	Real speeds = 0.0f;
	Real memberAuthored = 0.0f;
	for (const Object* member : *horde->getContainedItemsList())
	{
		if (member->isEffectivelyDead() || horde->getMemberSlot(member->getID()) < 0)
			continue;
		++members;
		const Coord3D slot = horde->getMemberPosition(member->getID(), FALSE);
		const Real dx = slot.x - member->getPosition()->x;
		const Real dy = slot.y - member->getPosition()->y;
		const Real error = WWMath::SqrtfOrigin(dx * dx + dy * dy);
		worst = std::max(worst, error);
		total += error;
		speeds += planarSpeed(*member);
		memberAuthored = std::max(memberAuthored, authoredSpeed(*member));
	}
	std::fprintf(stderr, "TEST horde-speed %u %s %.2f %.2f %.2f %.2f %u %.2f %.2f %.2f %.2f\n", ctx.frame,
		anchor->getTemplate()->getName().str(), anchor->getPosition()->x, anchor->getPosition()->y,
		planarSpeed(*anchor), authoredSpeed(*anchor), members, worst, members ? total / members : 0.0f,
		members ? speeds / members : 0.0f, memberAuthored);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailHordeSpeedTestCommands()
{
#if !RTS_GENERALS
	registerAction("horde-speed", logHordeSpeed);
#endif
}
}
