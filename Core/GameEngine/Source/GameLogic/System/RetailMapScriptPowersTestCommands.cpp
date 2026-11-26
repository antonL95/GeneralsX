#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/Team.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Module/RotwkAbilityModules.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptActions.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
const AsciiString powerName("MapScriptPowerFixture");
const AsciiString buttonName("MapScriptPowerButton");
const AsciiString waypointName("SpawnPoint_SkirmishGollum_3");

void runScriptPower(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;
	Int opcode = std::atoi(ctx.argument);
	const Bool disable = opcode == -482;
	if (disable)
		opcode = 482;
	const Int mapped = ScriptEngine::findRotwkActionByRetailIndex(opcode);
	if (mapped < 0)
	{
		std::fprintf(stderr, "TEST map-script-power unregistered %d\n", opcode);
		return;
	}

	Object* subject = opcode == 482 ? source : ctx.findTarget("PlyrCreeps:MapScriptPowerReference:0", true);
	if (!subject)
		return;

	auto* action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(mapped));
	if (opcode == 482)
	{
		action->getParameter(0)->friend_setString("<This Team>");
		action->getParameter(1)->friend_setString(buttonName);
		action->getParameter(2)->friend_setInt(!disable);
	}
	else
	{
		action->getParameter(0)->friend_setString(source->getControllingPlayer()->getPlayerName());
		action->getParameter(1)->friend_setString(powerName);
		if (opcode == 445 || opcode == 446)
		{
			action->getParameter(2)->friend_setString("MapScriptPowerTarget");
			action->getParameter(3)->friend_setString("<This Team>");
			if (opcode == 446)
				action->getParameter(4)->friend_setString("Player_1");
		}
		else
			action->getParameter(2)->friend_setString(opcode == 571 ? waypointName : AsciiString("<This Team>"));
	}

	Team* original = subject->getTeam();
	Team* team = newInstance(Team)(const_cast<TeamPrototype*>(original->getPrototype()), TEAM_ID_INVALID);
	subject->setTemporaryTeam(team);
	TheScriptEngine->friend_executeAction(action, team);
	subject->setTemporaryTeam(original);
	deleteInstance(team);
	deleteInstance(action);
}

void probeScriptPower(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;
	if (ctx.action == "map-script-power-auto")
	{
		const SpecialPowerTemplate* power = TheSpecialPowerStore->findSpecialPowerTemplate(powerName);
		const AutoAbilityBehavior* behavior = AutoAbilityBehavior::find(source, power);
		const CommandButton* button = TheRetailCommandCatalog ? TheRetailCommandCatalog->findCommandButton(buttonName) : nullptr;
		std::fprintf(stderr, "TEST map-script-power auto %u %d power=%d behavior=%d button=%d auto=%d\n",
			ctx.frame, behavior && behavior->isAutoActive(), power != nullptr, behavior != nullptr, button != nullptr, button && button->isAutoAbility());
		return;
	}

	const std::string_view name = ctx.argument;
	const Coord3D* position = nullptr;
	if (name == "waypoint")
	{
		if (const Waypoint* waypoint = TheTerrainLogic->getWaypointByName(waypointName))
			position = waypoint->getLocation();
	}
	else
	{
		const char* target = name == "team" ? "PlyrCreeps:MapScriptPowerReference:0" :
			name == "nearest" ? "PlyrCreeps:MapScriptPowerTarget:0" : "Player_1:MapScriptPowerTarget:0";
		if (const Object* object = ctx.findTarget(target, true))
			position = object->getPosition();
	}
	const Coord3D& actual = *source->getPosition();
	const Bool atTarget = position && actual.x == position->x && actual.y == position->y;
	std::fprintf(stderr, "TEST map-script-power marker %u %s %d\n", ctx.frame, ctx.argument, atTarget);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailMapScriptPowersTestCommands()
{
#if !RTS_GENERALS
	registerAction("map-script-power-fire", runScriptPower);
	registerAction("map-script-power-marker", probeScriptPower);
	registerAction("map-script-power-auto", probeScriptPower);
#endif
}
}
