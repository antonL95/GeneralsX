#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "GameLogic/Damage.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponSet.h"

namespace
{
void handleTrollDeath(const RetailTestCommands::Context& ctx)
{
	Object* troll = ctx.findObject();
	Object* unit = ctx.findTarget("Player_1:TrollSpinFixtureUnit:0", false);
	Object* building = ctx.findTarget("Player_1:TrollSpinFixtureBuilding:0", false);
	const BodyModuleInterface* body = troll ? troll->getBodyModule() : nullptr;
	if (troll && !body)
	{
		std::fprintf(stderr, "TEST troll-spin missing body %u %s\n", ctx.frame, ctx.targetName);
		return;
	}

	if (ctx.action == "troll-spin-state")
	{
		const AIUpdateInterface* ai = troll ? troll->getAI() : nullptr;
		const Weapon* weapon = troll ? troll->getCurrentWeapon() : nullptr;
		const ModelConditionFlags conditions = troll ? troll->getLogicModelConditions() : ModelConditionFlags();
		const Bool targetable = unit && troll &&
			unit->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, troll, CMD_FROM_PLAYER) >= ATTACKRESULT_POSSIBLE_AFTER_MOVING;
		std::fprintf(stderr,
			"TEST troll-spin state %u present=%d spin=%d deadline=%u targetable=%d dying=%d death1=%d death2=%d hp=%.1f unit=%.1f building=%.1f shot=%u\n",
			ctx.frame, troll != nullptr, conditions.test(MODELCONDITION_WEAPONSET_RAMPAGE),
			ai ? ai->getRetailRampageDeadline() : 0, targetable, conditions.test(MODELCONDITION_DYING),
			conditions.test(MODELCONDITION_DEATH_1), conditions.test(MODELCONDITION_DEATH_2),
			body ? body->getHealth() : 0.0f, unit ? unit->getBodyModule()->getHealth() : 0.0f,
			building ? building->getBodyModule()->getHealth() : 0.0f, weapon ? weapon->getLastShotFrame() : 0);
		return;
	}

	if (!troll || !unit)
		return;

	if (ctx.action == "troll-spin-stop")
	{
		if (AIUpdateInterface* ai = troll->getAI())
			ai->aiIdle(CMD_FROM_PLAYER);
		return;
	}

	if (ctx.action != "troll-spin-trigger" && ctx.action != "troll-spin-hit")
		return;

	DamageInfo damage;
	damage.in.m_sourceID = unit->getID();
	damage.in.m_damageType = DAMAGE_UNRESISTABLE;
	damage.in.m_deathType = DEATH_NORMAL;
	damage.in.m_amount = ctx.action == "troll-spin-trigger"
		? body->getHealth() - 75.0f
		: body->getMaxHealth();
	troll->attemptDamage(&damage);
	std::fprintf(stderr, "TEST troll-spin hit %u %s %.1f\n", ctx.frame, ctx.targetName, damage.out.m_actualDamageClipped);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailTrollDeathTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("troll-spin-", handleTrollDeath);
#endif
}
}
