#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>

#include "Common/ActionManager.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
void setBypassDisabled(const RetailTestCommands::Context& ctx)
{
	Object* obj = ctx.requireObject();
	if (!obj)
		return;
	if (ctx.argument && std::atoi(ctx.argument) != 0)
		obj->setDisabled(DISABLED_EMP);
	else
		obj->clearDisabled(DISABLED_EMP);
	std::fprintf(stderr, "TEST script-cast-disable frame=%u disabled=%d\n", ctx.frame, obj->isDisabled() ? 1 : 0);
}

void castPlayer(const RetailTestCommands::Context& ctx)
{
	Object* caster = ctx.requireObject();
	const CommandButton* button = TheRetailCommandCatalog ? TheRetailCommandCatalog->findCommandButton(ctx.argument) : nullptr;
	const SpecialPowerTemplate* power = button ? button->getSpecialPowerTemplate() : nullptr;
	if (!caster || !power || !ctx.hasPosition)
		return;
	const Coord3D point = ctx.groundLocation();
	const UnsignedInt options = button->getOptions();
	auto* module = static_cast<SpecialPowerModule*>(caster->getSpecialPowerModule(power));
	if (module)
		module->resetLastCastAccepted();
	const Bool allowed = TheActionManager->canDoSpecialPowerAtLocation(caster, &point, CMD_FROM_PLAYER, power, nullptr, options);
	caster->doSpecialPowerAtLocation(power, &point, INVALID_ANGLE, options, TRUE);
	std::fprintf(stderr, "TEST script-cast-player frame=%u allowed=%d accepted=%d\n",
		ctx.frame, allowed ? 1 : 0, module && module->wasLastCastAccepted() ? 1 : 0);
}

void castCommandButton(const RetailTestCommands::Context& ctx)
{
	Object* caster = ctx.requireObject();
	const CommandButton* button = TheRetailCommandCatalog ? TheRetailCommandCatalog->findCommandButton(ctx.argument) : nullptr;
	const SpecialPowerTemplate* power = button ? button->getSpecialPowerTemplate() : nullptr;
	if (!caster || !power || !ctx.hasPosition)
		return;
	const Coord3D point = ctx.groundLocation();
	auto* module = static_cast<SpecialPowerModule*>(caster->getSpecialPowerModule(power));
	if (module)
		module->resetLastCastAccepted();
	const CommandSourceType source = ctx.action == "script-cast-script-btn" ? CMD_FROM_SCRIPT : CMD_FROM_AI;
	caster->doCommandButtonAtPosition(button, &point, source);
	std::fprintf(stderr, "TEST %.*s frame=%u accepted=%d\n",
		static_cast<Int>(ctx.action.size()), ctx.action.data(), ctx.frame, module && module->wasLastCastAccepted() ? 1 : 0);
}

void castViaMapScript(const RetailTestCommands::Context& ctx)
{
	Object* caster = ctx.requireObject();
	Object* target = ctx.findTarget("PlyrCreeps:ScriptBypassTarget:0", true);
	const SpecialPowerTemplate* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
	auto* module = caster && power ? static_cast<SpecialPowerModule*>(caster->getSpecialPowerModule(power)) : nullptr;
	if (!caster || !target || !module)
		return;
	module->resetLastCastAccepted();
	AsciiString casterName;
	casterName.format("ScriptCastBypassCaster%u", static_cast<UnsignedInt>(caster->getID()));
	caster->setName(casterName);
	TheScriptEngine->addObjectToCache(caster);
	AsciiString targetName;
	targetName.format("ScriptCastBypassTarget%u", static_cast<UnsignedInt>(target->getID()));
	target->setName(targetName);
	TheScriptEngine->addObjectToCache(target);
	auto* action = newInstance(ScriptAction)(ScriptAction::NAMED_FIRE_SPECIAL_POWER_AT_NAMED);
	action->getParameter(0)->friend_setString(casterName);
	action->getParameter(1)->friend_setString(ctx.argument);
	action->getParameter(2)->friend_setString(targetName);
	TheScriptEngine->friend_executeAction(action, caster->getTeam());
	deleteInstance(action);
	std::fprintf(stderr, "TEST script-cast-map frame=%u accepted=%d\n",
		ctx.frame, module->wasLastCastAccepted() ? 1 : 0);
}

void castViaObjectScriptFlag(const RetailTestCommands::Context& ctx)
{
	Object* caster = ctx.requireObject();
	const SpecialPowerTemplate* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
	auto* module = caster && power ? static_cast<SpecialPowerModule*>(caster->getSpecialPowerModule(power)) : nullptr;
	if (!caster || !module || !ctx.hasPosition)
		return;
	const Coord3D point = ctx.groundLocation();
	module->resetLastCastAccepted();
	caster->doSpecialPowerAtLocation(power, &point, INVALID_ANGLE, COMMAND_FIRED_BY_SCRIPT, FALSE);
	std::fprintf(stderr, "TEST script-cast-obj frame=%u accepted=%d\n",
		ctx.frame, module->wasLastCastAccepted() ? 1 : 0);
}

void countBypassMarkers(const RetailTestCommands::Context& ctx)
{
	Int count = 0;
	for (const Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		if (!obj->isDestroyed() && !obj->isEffectivelyDead() && obj->getTemplate()->getName() == ctx.targetName)
			++count;
	std::fprintf(stderr, "TEST script-cast-count frame=%u markers=%d\n", ctx.frame, count);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailScriptCastBypassTestCommands()
{
#if !RTS_GENERALS
	registerAction("script-cast-disable", setBypassDisabled);
	registerAction("script-cast-player", castPlayer);
	registerAction("script-cast-ai-btn", castCommandButton);
	registerAction("script-cast-script-btn", castCommandButton);
	registerAction("script-cast-map", castViaMapScript);
	registerAction("script-cast-obj", castViaObjectScriptFlag);
	registerAction("script-cast-count", countBypassMarkers);
#endif
}
}
