#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/ExperienceLevelStore.h"
#include "GameLogic/Module/AISpecialPowerUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace
{
void probeBinding(const RetailTestCommands::Context& ctx)
{
	Object* obj = ctx.requireObject(true);
	if (!obj)
		return;

	const auto* module = obj->getTemplate()->getBehaviorModuleInfo().getNuggetWithTag(ctx.argument);
	if (module && module->first == "AISpecialPowerUpdate")
	{
		const auto* data = static_cast<const AISpecialPowerUpdateModuleData*>(module->second);
		const auto* button = TheRetailCommandCatalog->findCommandButton(data->m_commandButtonName);
		const auto* power = button ? button->getSpecialPowerTemplate() : nullptr;
		const auto* available = power ? TheRetailCommandCatalog->findPowerButton(obj, power) : nullptr;
		std::fprintf(stderr, "PKG67 binding %u %s ai=%d name=%s catalog=%d power=%s match=%d\n", ctx.frame,
			ctx.argument, obj->getControllingPlayer()->isSkirmishAIPlayer(), data->m_commandButtonName.str(),
			button != nullptr, power ? power->getName().str() : "-", button && button == available);
		return;
	}
	std::fprintf(stderr, "PKG67 missing module %u %s\n", ctx.frame, ctx.argument);
}

void probePower(const RetailTestCommands::Context& ctx)
{
	const Object* obj = ctx.requireObject(true);
	if (!obj)
		return;
	const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
	const auto* module = power ? obj->getSpecialPowerModule(power) : nullptr;
	std::fprintf(stderr, "PKG67 power %u %s %s rank=%d ready=%d\n", ctx.frame, ctx.targetName, ctx.argument,
		obj->getExperienceTracker()->getRank(), module ? module->isReady() : -1);
}

void setCommands(const RetailTestCommands::Context& ctx)
{
	if (Object* obj = ctx.requireObject(true))
		obj->setCommandSetStringOverride(ctx.argument);
}

void loadLevels(const RetailTestCommands::Context& ctx)
{
	std::ifstream input(ctx.argument);
	if (!input)
		return;
	std::ostringstream text;
	text << input.rdbuf();
	INI ini;
	ini.loadText(ctx.argument, text.str().c_str(), INI_LOAD_OVERWRITE, "ExperienceLevel", ExperienceLevelStore::parseDefinition);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage67TestCommands()
{
#if RTS_ZEROHOUR
	registerAction("pkg67-binding", probeBinding);
	registerAction("pkg67-load-levels", loadLevels);
	registerAction("pkg67-power", probePower);
	registerAction("pkg67-set-commands", setCommands);
#endif
}
}
