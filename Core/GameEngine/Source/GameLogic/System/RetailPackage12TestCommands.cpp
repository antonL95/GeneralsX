#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/AttributeModifierRules.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/RotwkParseOnlyModules.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/Weapon.h"

void runRetailPackage12TestCommand(std::string_view action, Object* source, Object* target,
	UnsignedInt frame)
{
	auto* ai = source->getAI();
	if (!ai)
		return;

	if (action == "pkg12-ai-hold")
	{
		if (auto* stances = StancesBehavior::find(source))
			stances->setStance(AttributeModifierRules::STANCE_HOLD_GROUND);
	}
	else if ((action == "pkg12-ai-attack" || action == "pkg12-ai-attack-player") && target)
		ai->aiAttackObject(target, NO_MAX_SHOTS_LIMIT,
			action == "pkg12-ai-attack-player" ? CMD_FROM_PLAYER : CMD_FROM_AI);
	else if (action == "pkg12-ai-leash" && target)
	{
		const Coord3D& from = *source->getPosition();
		const Coord3D& to = *target->getPosition();
		std::fprintf(stderr, "TEST pkg12 leash frame=%u source=%s allowed=%d x=%.2f y=%.2f targetX=%.2f targetY=%.2f order=%d target=%u owner=%d\n",
			frame, source->getTemplate()->getName().str(), ai->stanceAllowsChase(target),
			from.x, from.y, to.x, to.y, static_cast<Int>(ai->getLastCommandSource()),
			ai->getGoalObject() ? static_cast<unsigned>(ai->getGoalObject()->getID()) : 0,
			static_cast<Int>(source->getControllingPlayer()->getPlayerType()));
	}
	else if (action == "pkg12-ai-hurt" || action == "pkg12-ai-burn")
	{
		DamageInfo damage;
		damage.in.m_damageType = action == "pkg12-ai-burn" ? DAMAGE_FLAME : DAMAGE_UNRESISTABLE;
		damage.in.m_deathType = action == "pkg12-ai-burn" ? DEATH_BURNED : DEATH_NORMAL;
		damage.in.m_kill = action == "pkg12-ai-burn";
		damage.in.m_amount = source->getBodyModule()->getHealth() - 75.0f;
		source->attemptDamage(&damage);
	}
	else if (action == "pkg12-ai-cower" && target)
		ai->fleeFromFear(target, LOGICFRAMES_PER_SECOND, MODELCONDITION_EMOTION_AFRAID);
	else if (action == "pkg12-ai-state")
	{
		const Weapon* weapon = source->getCurrentWeapon();
		std::fprintf(stderr, "TEST pkg12 state frame=%u source=%s fear=%d burning=%u rampage=%d weapon=%s shot=%u health=%.2f x=%.2f y=%.2f\n",
			frame, source->getTemplate()->getName().str(),
			source->getLogicModelConditions().test(MODELCONDITION_EMOTION_AFRAID), ai->getRetailBurningDeathDeadline(),
			source->testStatus(OBJECT_STATUS_RAMPAGING), weapon ? weapon->getName().str() : "-",
			weapon ? weapon->getLastShotFrame() : 0,
			source->getBodyModule()->getHealth(), source->getPosition()->x, source->getPosition()->y);
	}
	else if (action == "pkg12-ai-priority" && target)
	{
		const AttackPriorityInfo* info = ai->getAttackInfo();
		std::fprintf(stderr, "TEST pkg12 priority frame=%u source=%s name=%s weight=%d\n",
			frame, source->getTemplate()->getName().str(),
			info && info->getName().isNotEmpty() ? info->getName().str() : "-",
			info ? info->getPriority(target->getTemplate()) : -1);
	}
	else if ((action == "pkg12-ai-infantry" || action == "pkg12-ai-siege") && target)
	{
		auto* priority = newInstance(ScriptAction)(ScriptAction::SET_ATTACK_PRIORITY_THING);
		priority->setNextAction(nullptr);
		priority->getParameter(0)->friend_setString(action == "pkg12-ai-infantry" ?
			"AttackPriority_Infantry" : "AttackPriority_SIEGEWEAPON");
		priority->getParameter(1)->friend_setString(target->getTemplate()->getName());
		priority->getParameter(2)->friend_setInt(action == "pkg12-ai-infantry" ? 61 : 81);
		TheScriptEngine->friend_executeAction(priority, source->getTeam());
		deleteInstance(priority);
	}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage12TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg12-ai-", [](const Context& ctx) {
		Object* source = ctx.requireObject();
		if (!source)
			return;
		runRetailPackage12TestCommand(ctx.action, source, ctx.target, ctx.frame);
	});
#endif
}
}
