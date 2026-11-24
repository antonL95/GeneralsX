#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>
#include "Common/Team.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
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
	std::fprintf(stderr, "TEST skirmish-waypoint frame=%u first=%d reachable=%d missing=%d\n",
		ctx.frame, team->getFirstItemIn_TeamMemberList() == unit,
		canPath(team, "SpawnPoint_SkirmishGollum_3"), canPath(team, "SkirmishWaypointMissing"));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailSkirmishWaypointTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("skirmish-waypoint-promote", promoteMember);
	registerAction("skirmish-waypoint-probe", waypointProbe);
#endif
}
}
