#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/Upgrade.h"
#include "GameLogic/Object.h"

namespace
{
void sampleCharging(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject();
	if (!source)
		return;
	const auto& conditions = source->getLogicModelConditions();
	const auto* level2 = TheUpgradeCenter->findUpgrade("Upgrade_MiniHordeLvl2");
	const auto* level3 = TheUpgradeCenter->findUpgrade("Upgrade_MiniHordeLvl3");
	std::fprintf(stderr, "TEST flamesprayer frame=%u source=%s toggle=%d level2=%d level3=%d charging=%d\n",
		ctx.frame, ctx.targetName, conditions.test(MODELCONDITION_WEAPONSET_TOGGLE_1),
		level2 && source->hasUpgrade(level2), level3 && source->hasUpgrade(level3),
		conditions.test(MODELCONDITION_CHARGING));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailFlamesprayerTestCommands()
{
#if !RTS_GENERALS
	registerAction("flamesprayer-state", sampleCharging);
#endif
}
}
