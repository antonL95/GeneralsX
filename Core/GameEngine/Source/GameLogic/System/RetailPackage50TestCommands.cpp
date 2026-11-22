#include "PreRTS.h"

#if !RTS_GENERALS
#include "Common/ActionManager.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

#include <cstdio>
#include <string_view>

void runRetailPackage50TestCommand(std::string_view action, Object* source, Object* target, const char* name, const char* argument, UnsignedInt frame)
{
	if (action == "pkg50-state")
	{
		const ModelConditionFlags flags = source ? source->getLogicModelConditions() : ModelConditionFlags();
		const Drawable* draw = source ? source->getDrawable() : nullptr;
		const auto* body = source ? source->getBodyModule() : nullptr;
		std::fprintf(stderr, "TEST pkg50 frame=%u name=%s present=%d hp=%.2f opacity=%.3f hit=%d%d%d%d ability=%d%d%d%d%d oath=%d x=%.1f\n",
			frame, name, source != nullptr, body ? body->getHealth() : 0.0f, draw ? draw->getEffectiveOpacity() : -1.0f,
			flags.test(MODELCONDITION_HIT_REACTION), flags.test(MODELCONDITION_HIT_LEVEL_1),
			flags.test(MODELCONDITION_HIT_LEVEL_2), flags.test(MODELCONDITION_HIT_LEVEL_3),
			flags.test(MODELCONDITION_UNPACKING), flags.test(MODELCONDITION_PREPARING), flags.test(MODELCONDITION_PACKING),
			flags.test(MODELCONDITION_SPECIAL_POWER_1), flags.test(MODELCONDITION_SPECIAL_WEAPON_ONE),
			flags.test(MODELCONDITION_OATH_FULLFILLED), source ? source->getPosition()->x : 0.0f);
	}
	else if (action == "pkg50-power" && source)
	{
		const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(argument);
		if (power)
			source->doSpecialPower(power, 0);
	}
	else if (action == "pkg50-kill" && source)
		source->kill();
	else if (action == "pkg50-bored" && source && target)
	{
		const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityMountainTrollBored");
		const auto* module = power ? source->getSpecialPowerModule(power) : nullptr;
		std::fprintf(stderr, "TEST pkg50 bored frame=%u idle=%d module=%d ready=%d valid=%d\n", frame,
			source->getAI() && source->getAI()->isIdle(), module != nullptr, module && module->isReady(),
			power && TheActionManager->canDoSpecialPowerAtObject(source, target, CMD_FROM_AI, power, NEED_TARGET_ALLY_OBJECT));
	}
}
#endif
