#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/ThingTemplate.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"

namespace
{
const char* weaponStatusName(WeaponStatus status)
{
	switch (status)
	{
		case READY_TO_FIRE: return "READY";
		case OUT_OF_AMMO: return "EMPTY";
		case BETWEEN_FIRING_SHOTS: return "BETWEEN";
		case RELOADING_CLIP: return "RELOADING";
		case PRE_ATTACK: return "PRE_ATTACK";
	}
	return "UNKNOWN";
}

void handlePackage58(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;

	if (ctx.action == "pkg58-state" || ctx.action == "pkg58-passenger")
	{
		const char* label = ctx.argument && ctx.argument[0] ? ctx.argument : source->getTemplate()->getName().str();
		if (ctx.action == "pkg58-passenger")
		{
			const auto* contain = source->getContain();
			const auto* occupants = contain ? contain->getContainedItemsList() : nullptr;
			if (!occupants || occupants->empty())
			{
				std::fprintf(stderr, "TEST pkg58 frame=%u label=%s missing=1\n", ctx.frame, label);
				return;
			}
			source = occupants->front();
		}
		const Weapon* weapon = source->getCurrentWeapon();
		const AIUpdateInterface* ai = source->getAI();
		const PhysicsBehavior* physics = source->getPhysics();
		const ModelConditionFlags& mc = source->getLogicModelConditions();
		std::fprintf(stderr,
			"TEST pkg58 frame=%u label=%s object=%u weapon=%s status=%s ammo=%u shot=%u next=%u fireAllowed=%d idleWait=%d committed=%d holdReload=%d aiming=%d aimMask=%d%d%d%d%d moving=%d speed=%.2f x=%.1f y=%.1f health=%.1f\n",
			ctx.frame,
			label,
			static_cast<unsigned>(source->getID()),
			weapon ? weapon->getName().str() : "-",
			weapon ? weaponStatusName(weapon->getStatus()) : "-",
			weapon ? weapon->getRemainingAmmo() : 0,
			weapon ? weapon->getLastShotFrame() : 0,
			weapon ? weapon->getPossibleNextShotFrame() : 0,
			weapon && weapon->canFireInCurrentMovement(source) ? 1 : 0,
			weapon && weapon->isWaitingToIdle() ? 1 : 0,
			weapon && weapon->hasCommittedAttack() ? 1 : 0,
			source->isHoldingMovementForReload(ctx.frame) ? 1 : 0,
			source->testStatus(OBJECT_STATUS_IS_AIMING_WEAPON) ? 1 : 0,
			mc.test(MODELCONDITION_AIM_LOW) ? 1 : 0,
			mc.test(MODELCONDITION_AIM_STRAIGHT) ? 1 : 0,
			mc.test(MODELCONDITION_AIM_HIGH) ? 1 : 0,
			mc.test(MODELCONDITION_AIM_NEAR) ? 1 : 0,
			mc.test(MODELCONDITION_AIM_FAR) ? 1 : 0,
			ai && ai->isMoving() ? 1 : 0,
			physics ? physics->getVelocityMagnitude() : 0.0f,
			source->getPosition()->x,
			source->getPosition()->y,
			source->getBodyModule() ? source->getBodyModule()->getHealth() : 0.0f);
		std::fflush(stderr);
	}
	else if (ctx.action == "pkg58-charge")
	{
		if (std::string_view(ctx.argument) == "on")
			source->setModelConditionState(MODELCONDITION_CHARGING);
		else
			source->clearModelConditionState(MODELCONDITION_CHARGING);
	}
	else if (ctx.action == "pkg58-velocity")
	{
		if (PhysicsBehavior* physics = source->getPhysics())
		{
			const Coord3D vel{1.5f, 0.0f, 0.0f};
			physics->addVelocityTo(&vel);
		}
	}
	else if (ctx.action == "pkg58-teleport")
	{
		Coord3D pos = *source->getPosition();
		if (std::sscanf(ctx.argument, "%f,%f", &pos.x, &pos.y) == 2)
		{
			pos.z = TheTerrainLogic->getLayerHeight(pos.x, pos.y, source->getLayer());
			source->setPosition(&pos);
		}
	}
	else if (ctx.action == "pkg58-flank-probe")
	{
		Object* victim = ctx.findTarget(ctx.argument, true);
		const Bool flanked = victim ? HordeContain::isFlanked(victim, source, FALSE) : FALSE;
		std::fprintf(stderr, "TEST pkg58-flank frame=%u source=%u victim=%u flanked=%d\n",
			ctx.frame,
			static_cast<unsigned>(source->getID()),
			victim ? static_cast<unsigned>(victim->getID()) : 0,
			flanked ? 1 : 0);
		std::fflush(stderr);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage58TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg58-", handlePackage58);
#endif
}
}
