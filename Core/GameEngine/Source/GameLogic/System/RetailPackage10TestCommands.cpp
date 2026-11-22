#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "GameClient/Drawable.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"

void runRetailPackage10TestCommand(std::string_view action, Object* source, const char* argument, UnsignedInt frame)
{
	if (action == "pkg10-state")
	{
		const Weapon* weapon = source->getCurrentWeapon();
		const auto* ai = source->getAI();
		const Object* victim = ai ? ai->getCurrentVictim() : nullptr;
		std::fprintf(stderr, "TEST pkg10 frame=%u object=%u weapon=%s target=%u shot=%u next=%u ammo=%u ready=%.3f moving=%d aiming=%d fireAllowed=%d forceDisplay=%d aimNear=%d aimStraight=%d health=%.1f x=%.1f y=%.1f\n",
			frame, static_cast<unsigned>(source->getID()), weapon ? weapon->getName().str() : "-",
			victim ? static_cast<unsigned>(victim->getID()) : 0, weapon ? weapon->getLastShotFrame() : 0,
			weapon ? weapon->getPossibleNextShotFrame() : 0, weapon ? weapon->getRemainingAmmo() : 0,
			weapon ? weapon->getPercentReadyToFire() : 0.0f, ai && ai->isMoving(),
			source->testStatus(OBJECT_STATUS_IS_AIMING_WEAPON), weapon && weapon->canFireInCurrentMovement(source),
			weapon && weapon->getTemplate()->forcesDisplayPercentReady(),
			source->getLogicModelConditions().test(MODELCONDITION_AIM_NEAR),
			source->getLogicModelConditions().test(MODELCONDITION_AIM_STRAIGHT), source->getBodyModule()->getHealth(),
			source->getPosition()->x, source->getPosition()->y);
	}
	else if (action == "pkg10-review-state")
	{
		const Weapon* weapon = source->getCurrentWeapon();
		const Drawable* draw = source->getDrawable();
		const PhysicsBehavior* physics = source->getPhysics();
		std::fprintf(stderr, "TEST pkg10 review frame=%u object=%u weapon=%s shot=%u next=%u duration=%u idle=%u fireAllowed=%d firing=%d enclosed=%d speed=%.2f angle=%.3f x=%.2f y=%.2f\n",
			frame, static_cast<unsigned>(source->getID()), weapon ? weapon->getName().str() : "-",
			weapon ? weapon->getLastShotFrame() : 0, weapon ? weapon->getPossibleNextShotFrame() : 0,
			weapon ? weapon->getTemplate()->getFiringDuration() : 0, weapon ? weapon->getTemplate()->getIdleAfterFiringDelay() : 0,
			weapon && weapon->canFireInCurrentMovement(source),
			draw && draw->getModelConditionFlags().test(MODELCONDITION_FIRING_A), source->getEnclosingContainedBy() != nullptr,
			physics ? physics->getVelocityMagnitude() : 0.0f, source->getOrientation(), source->getPosition()->x, source->getPosition()->y);
	}
	else if (action == "pkg10-velocity")
	{
		if (auto* physics = source->getPhysics())
		{
			const Coord3D velocity{1.0f, 0.0f, 0.0f};
			physics->addVelocityTo(&velocity);
		}
	}
	else if (action == "pkg10-rock")
	{
		WeaponSetFlags flags;
		flags.setBitByName("WEAPONSET_TOGGLE_1");
		source->setWeaponSetFlags(flags, std::string_view(argument) == "on");
	}
	else if (action == "pkg10-lock-primary")
	{
		source->setWeaponLock(PRIMARY_WEAPON, LOCKED_PERMANENTLY);
	}
	else if (action == "pkg10-charge")
	{
		if (std::string_view(argument) == "on")
			source->setModelConditionState(MODELCONDITION_CHARGING);
		else
			source->clearModelConditionState(MODELCONDITION_CHARGING);
	}
	else if (action == "pkg10-position")
	{
		Coord3D point;
		Real degrees;
		const Int count = std::sscanf(argument, "%f,%f,%f", &point.x, &point.y, &degrees);
		if (count >= 2)
		{
			point.z = TheTerrainLogic->getGroundHeight(point.x, point.y);
			source->setPosition(&point);
			if (count == 3)
				source->setOrientation(degrees * PI / 180.0f);
		}
	}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage10TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg10-", [](const Context& ctx) {
		Object* source = ctx.requireObject();
		if (!source)
			return;
		runRetailPackage10TestCommand(ctx.action, source, ctx.argument, ctx.frame);
	});
#endif
}
}
