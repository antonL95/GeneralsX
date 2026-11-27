#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include "Common/ActionManager.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
void pausedOclState(const RetailTestCommands::Context& ctx)
{
	std::fprintf(stderr, "TEST paused-ocl frame=%u object=%s alive=%d\n", ctx.frame, ctx.targetName, ctx.findObject(true) ? 1 : 0);
}

void pausedOclProbe(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	const CommandButton* button = TheRetailCommandCatalog->findCommandButton(ctx.argument);
	if (!object || !button || !button->getSpecialPowerTemplate() || !ctx.hasPosition)
		return;
	const Coord3D point = ctx.groundLocation();
	const SpecialPowerTemplate* power = button->getSpecialPowerTemplate();
	const SpecialPowerModuleInterface* module = object->getSpecialPowerModule(power);
	std::fprintf(stderr, "TEST paused-ocl-probe frame=%u ready=%d allowed=%d target=%d water=%d\n", ctx.frame,
		module && module->isReady(), TheActionManager->canDoSpecialPowerAtLocation(object, &point, CMD_FROM_PLAYER, power, nullptr, button->getOptions()),
		power->isValidRetailTarget(object, nullptr, &point, CMD_FROM_PLAYER), TheTerrainLogic->isUnderwater(point.x, point.y));
}

void pausedOclCountdown(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	const SpecialPowerTemplate* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
	const SpecialPowerModuleInterface* module = object && power ? object->getSpecialPowerModule(power) : nullptr;
	if (!module)
		return;

	AsciiString name;
	name.format("PausedOclCountdownTarget%u", static_cast<UnsignedInt>(object->getID()));
	object->setName(name);
	TheScriptEngine->addObjectToCache(object);
	const auto type = ctx.action == "paused-ocl-stop" ? ScriptAction::NAMED_STOP_SPECIAL_POWER_COUNTDOWN :
		ScriptAction::NAMED_START_SPECIAL_POWER_COUNTDOWN;
	auto* action = newInstance(ScriptAction)(type);
	action->getParameter(0)->friend_setString(name);
	action->getParameter(1)->friend_setString(ctx.argument);
	TheScriptEngine->friend_executeAction(action, object->getTeam());
	deleteInstance(action);
	std::fprintf(stderr, "TEST paused-ocl-countdown frame=%u ready=%d\n", ctx.frame, module->isReady());
}

void pausedOclLuaEvent(const RetailTestCommands::Context& ctx)
{
	if (Object* object = ctx.requireObject())
	{
		object->clearModelConditionState(MODELCONDITION_USER_6);
		object->setModelConditionState(MODELCONDITION_USER_6);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPausedOclTestCommands()
{
#if !RTS_GENERALS
	registerAction("paused-ocl-state", pausedOclState);
	registerAction("paused-ocl-probe", pausedOclProbe);
	registerAction("paused-ocl-stop", pausedOclCountdown);
	registerAction("paused-ocl-start", pausedOclCountdown);
	registerAction("paused-ocl-lua", pausedOclLuaEvent);
#endif
}
}
