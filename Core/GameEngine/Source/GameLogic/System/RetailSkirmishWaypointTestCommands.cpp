#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
void createProbe(const RetailTestCommands::Context& ctx)
{
	const auto* type = TheThingFactory->findTemplate(ctx.targetName, FALSE);
	if (!type || !ctx.hasPosition)
		return;
	auto* unit = TheThingFactory->newObject(type, ThePlayerList->getNeutralPlayer()->getDefaultTeam());
	const Coord3D position = ctx.groundLocation();
	unit->setPosition(&position);
}

Bool canPath(Team* team, const char* waypoint)
{
	for (Int i = 0; i < Condition::NUM_ITEMS; ++i)
	{
		if (TheScriptEngine->getConditionTemplate(i)->m_internalName != "TEAM_CAN_PATH_TO_WAYPOINT")
			continue;
		auto* condition = newInstance(Condition)(static_cast<Condition::ConditionType>(i));
		condition->getParameter(0)->friend_setString("<This Team>");
		condition->getParameter(1)->friend_setString(waypoint);
		auto* clause = newInstance(OrCondition);
		clause->setFirstAndCondition(condition);
		auto* script = newInstance(Script);
		script->setOrCondition(clause);
		const Bool result = TheScriptEngine->evaluateConditions(script, team);
		deleteInstance(script);
		return result;
	}
	return FALSE;
}

void promoteMember(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit)
		return;
	Team* original = unit->getTeam();
	Team* temporary = newInstance(Team)(const_cast<TeamPrototype*>(original->getPrototype()), TEAM_ID_INVALID);
	unit->setTemporaryTeam(temporary);
	unit->setTemporaryTeam(original);
	deleteInstance(temporary);
}

void waypointProbe(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit)
		return;
	Team* team = unit->getTeam();
	const char* cliff = "AOR_Random Waypoint_17";
	const Int retailType = ScriptEngine::findRotwkConditionByRetailIndex(139);
	const Template* definition = retailType >= 0 ? TheScriptEngine->getConditionTemplate(retailType) : nullptr;
	const Bool signature = definition && definition->m_internalName == "TEAM_CAN_PATH_TO_WAYPOINT" &&
		definition->getNumParameters() == 2 && definition->getParameterType(0) == Parameter::TEAM &&
		definition->getParameterType(1) == Parameter::WAYPOINT;
	std::fprintf(stderr, "TEST skirmish-waypoint frame=%u first=%d reachable=%d missing=%d cliff=%d signature=%d\n",
		ctx.frame, team->getFirstItemIn_TeamMemberList() == unit,
		canPath(team, "SpawnPoint_SkirmishGollum_8"), canPath(team, "SkirmishWaypointMissing"),
		TheTerrainLogic->getWaypointByName(cliff) && !canPath(team, cliff), signature);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailSkirmishWaypointTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("skirmish-waypoint-create", createProbe);
	registerAction("skirmish-waypoint-promote", promoteMember);
	registerAction("skirmish-waypoint-probe", waypointProbe);
#endif
}
}
