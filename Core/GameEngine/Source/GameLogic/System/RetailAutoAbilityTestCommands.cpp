#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

namespace
{
void handleAutoAbility(const RetailTestCommands::Context& ctx)
{
	if (ctx.action == "auto-ability-slot")
	{
		const CommandSet* set = TheRetailCommandCatalog->findCommandSet(ctx.argument);
		const Int entry = static_cast<Int>(ctx.position.x) - 1;
		const CommandButton* button = set ? set->getCommandButton(entry) : nullptr;
		const Bool ring = button && entry < CommandButtonRules::RETAIL_PALANTIR_SLOTS && button->isInPalantir();
		const Bool side = button && !ring && button->isRetailSideColumnEligible();
		std::fprintf(stderr, "TEST auto slot frame=%u entry=%d button=%s ring=%d side=%d radial=%d\n",
			ctx.frame, entry + 1, button ? button->getName().str() : "-", ring, side, button && button->isRadial());
		return;
	}

	Object* object = ctx.requireObject(false);
	if (!object)
		return;

	if (ctx.action == "auto-ability-hit")
	{
		const Object* attacker = ctx.findTarget();
		if (!attacker)
			return;
		DamageInfo damage;
		damage.in.m_sourceID = attacker->getID();
		damage.in.m_damageType = DAMAGE_UNRESISTABLE;
		damage.in.m_amount = ctx.position.x;
		damage.in.m_deathType = DEATH_NORMAL;
		object->attemptDamage(&damage);
		std::fprintf(stderr, "TEST auto hit frame=%u damage=%.1f\n", ctx.frame, damage.out.m_actualDamageClipped);
	}
	else if (ctx.action == "auto-ability-disable")
	{
		object->setDisabledUntil(DISABLED_EMP, ctx.frame + 5);
	}
	else if (ctx.action == "auto-ability-state")
	{
		const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
		const auto* module = object->getSpecialPowerModule(power);
		std::fprintf(stderr, "TEST auto state frame=%u power=%s ready=%d next=%u dead=%d disabled=%d bubble=%d\n",
			ctx.frame, ctx.argument, module && module->isReady(), module ? module->getReadyFrame() : 0,
			object->isEffectivelyDead(), object->isDisabled(), object->getLogicModelConditions().test(MODELCONDITION_USER_3));
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailAutoAbilityTestCommands()
{
#if RTS_ZEROHOUR
	registerPrefix("auto-ability-", handleAutoAbility);
#endif
}
}
