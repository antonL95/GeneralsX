#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <string>

#include "Common/Player.h"
#include "GameLogic/ExperienceTracker.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Object.h"

namespace
{
void handleToggleState(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;

	std::string owned;
	std::string_view names = ctx.argument;
	while (!names.empty())
	{
		const size_t comma = names.find(',');
		const std::string name(names.substr(0, comma));
		names = comma == std::string_view::npos ? std::string_view() : names.substr(comma + 1);
		const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(name.c_str());
		owned += ' ';
		owned += name;
		owned += '=';
		owned += upgrade && source->hasUpgrade(upgrade) ? '1' : '0';
	}
	const Drawable* draw = source->getDrawable();
	std::fprintf(stderr, "TOGGLE state frame=%u actor=%s complete=%d set=%s user7=%d money=%u xp=%.0f%s\n", ctx.frame,
		ctx.targetName, !source->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION), source->getCommandSetString().str(),
		draw && draw->getModelConditionFlags().test(MODELCONDITION_USER_7),
		source->getControllingPlayer()->getMoney()->countMoney(),
		source->getExperienceTracker()->getRetailExperience(), owned.c_str());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailToggleDefaultsTestCommands()
{
#if !RTS_GENERALS
	registerAction("toggle-state", handleToggleState);
#endif
}
}
