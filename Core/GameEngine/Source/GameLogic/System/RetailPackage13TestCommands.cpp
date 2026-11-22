#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/ThingTemplate.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponSet.h"

void runRetailPackage13TestCommand(std::string_view action, Object* source, Object* target, const char* argument, UnsignedInt frame)
{
	if (!source)
		return;

	if (action == "pkg13-state")
	{
		WeaponSlotType slot = PRIMARY_WEAPON;
		const Weapon* weapon = source->getCurrentWeapon(&slot);
		const AIUpdateInterface* ai = source->getAI();
		const auto& mc = source->getLogicModelConditions();
		const Int canAttack = target
			? static_cast<Int>(source->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, target, CMD_FROM_PLAYER))
			: -1;
		const Int stanceChase = (ai && target) ? (ai->stanceAllowsChase(target) ? 1 : 0) : -1;
		const Int consecShots = target ? source->getNumConsecutiveShotsFiredAtTarget(target) : 0;
		std::fprintf(stderr,
			"TEST pkg13 state frame=%u object=%u template=%s slot=%d weapon=%s status=%d closeRange=%d closeRangeMC=%d aimLow=%d aimStraight=%d aimHigh=%d aimNear=%d aimFar=%d chase=%d stanceChase=%d antiStruct=%d canTarget=%d clearIdle=%d consec=%d shot=%u hp=%.1f\n",
			frame,
			static_cast<unsigned>(source->getID()),
			source->getTemplate()->getName().str(),
			weapon ? static_cast<int>(slot) : -1,
			weapon ? weapon->getName().str() : "-",
			weapon ? static_cast<int>(weapon->getStatus()) : -1,
			source->testWeaponSetFlag(WEAPONSET_CLOSE_RANGE) ? 1 : 0,
			mc.test(MODELCONDITION_WEAPONSET_CLOSE_RANGE) ? 1 : 0,
			mc.test(MODELCONDITION_AIM_LOW) ? 1 : 0,
			mc.test(MODELCONDITION_AIM_STRAIGHT) ? 1 : 0,
			mc.test(MODELCONDITION_AIM_HIGH) ? 1 : 0,
			mc.test(MODELCONDITION_AIM_NEAR) ? 1 : 0,
			mc.test(MODELCONDITION_AIM_FAR) ? 1 : 0,
			weapon && weapon->isChaseWeapon() ? 1 : 0,
			stanceChase,
			weapon && (weapon->getAntiMask() & WEAPON_ANTI_STRUCTURE) != 0 ? 1 : 0,
			canAttack,
			source->getTemplate()->shouldClearShotsOnIdle() ? 1 : 0,
			consecShots,
			weapon ? weapon->getLastShotFrame() : 0u,
			source->getBodyModule() ? source->getBodyModule()->getHealth() : 0.0f);
		return;
	}

	if (action == "pkg13-choose")
	{
		const Bool found = source->chooseBestWeaponForTarget(target, USE_WEAPONSET_DEFAULT_CRITERIA, CMD_FROM_PLAYER);
		WeaponSlotType slot = PRIMARY_WEAPON;
		const Weapon* weapon = source->getCurrentWeapon(&slot);
		std::fprintf(stderr,
			"TEST pkg13 choose frame=%u object=%u template=%s target=%u found=%d slot=%d weapon=%s status=%d\n",
			frame,
			static_cast<unsigned>(source->getID()),
			source->getTemplate()->getName().str(),
			target ? static_cast<unsigned>(target->getID()) : 0u,
			found ? 1 : 0,
			weapon ? static_cast<int>(slot) : -1,
			weapon ? weapon->getName().str() : "-",
			weapon ? static_cast<int>(weapon->getStatus()) : -1);
		return;
	}

	if (action == "pkg13-condition")
	{
		if (argument && *argument)
		{
			std::string_view arg(argument);
			Bool setFlag = TRUE;
			if (arg.starts_with("!"))
			{
				setFlag = FALSE;
				arg.remove_prefix(1);
			}
			AsciiString name(arg.data(), static_cast<Int>(arg.size()));
			ModelConditionFlags flags;
			if (flags.setBitByName(name.str()))
			{
				if (setFlag)
					source->setModelConditionFlags(flags);
				else
					source->clearModelConditionFlags(flags);
			}
		}
		return;
	}

	if (action == "pkg13-idle")
	{
		if (AIUpdateInterface* ai = source->getAI())
			ai->aiIdle(CMD_FROM_PLAYER);
		return;
	}

	if (action == "pkg13-shot")
	{
		const Weapon* weapon = source->getCurrentWeapon();
		if (weapon && target)
			source->notifyFiringTrackerShotFired(weapon, target->getID());
		return;
	}

	if (action == "pkg13-fire-once")
	{
		if (target)
		{
			source->chooseBestWeaponForTarget(target, USE_WEAPONSET_DEFAULT_CRITERIA, CMD_FROM_PLAYER);
			source->fireCurrentWeapon(target);
		}
		return;
	}

	if (action == "pkg13-position")
	{
		if (argument && *argument)
		{
			Coord3D point = *source->getPosition();
			Real z = 0.0f;
			const Int count = std::sscanf(argument, "%f,%f,%f", &point.x, &point.y, &z);
			if (count >= 2)
			{
				point.z = count == 3 ? z : TheTerrainLogic->getGroundHeight(point.x, point.y);
				source->setPosition(&point);
			}
		}
		return;
	}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage13TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg13-", [](const Context& ctx) {
		Object* source = ctx.requireObject();
		if (!source)
			return;
		runRetailPackage13TestCommand(ctx.action, source, ctx.target, ctx.argument, ctx.frame);
	});
#endif
}
}
