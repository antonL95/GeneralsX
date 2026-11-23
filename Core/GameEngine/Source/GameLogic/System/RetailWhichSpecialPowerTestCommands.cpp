#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"

#include <cstdio>
#endif

namespace RetailTestCommands
{
void registerRetailWhichSpecialPowerTestCommands()
{
#if !RTS_GENERALS
	registerAction("which-special-power-state", [](const Context& ctx) {
		const Object* caster = ctx.requireObject();
		if (!caster)
			return;

		const auto& conditions = caster->getLogicModelConditions();
		std::fprintf(stderr, "TEST which-special-power %u %s power1=%d invulnerable=%d unpacking=%d health=%.2f\n",
			ctx.frame, ctx.targetName, conditions.test(MODELCONDITION_SPECIAL_POWER_1),
			conditions.test(MODELCONDITION_INVULNERABLE), conditions.test(MODELCONDITION_UNPACKING),
			caster->getBodyModule()->getHealth());
	});
#endif
}
}
