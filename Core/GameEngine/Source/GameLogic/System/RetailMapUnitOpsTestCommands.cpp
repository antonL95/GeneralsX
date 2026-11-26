#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>
#include "Common/Team.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
Bool attack(Team* team, Object* unit, const char* waypoint)
{
	for (Int i = 0; i < ScriptEngine::getActionTemplateCount(); ++i)
	{
		if (TheScriptEngine->getActionTemplate(i)->m_internalName != "ATTACK_MOVE_TEAM_TO" ||
			ScriptEngine::findRotwkActionByRetailIndex(544) != i ||
			TheScriptEngine->getActionTemplate(i)->getNumParameters() != 2 ||
			TheScriptEngine->getActionTemplate(i)->getParameterType(0) != Parameter::TEAM ||
			TheScriptEngine->getActionTemplate(i)->getParameterType(1) != Parameter::WAYPOINT)
			continue;
		auto* action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(i));
		action->getParameter(0)->friend_setString("<This Team>");
		action->getParameter(1)->friend_setString(waypoint);
		TheScriptEngine->friend_executeAction(action, team);
		deleteInstance(action);
		const Waypoint* destination = TheTerrainLogic->getWaypointByName(waypoint);
		const Coord3D* goal = unit->getAI()->getGoalPosition();
		return destination && goal && goal->x == destination->getLocation()->x && goal->y == destination->getLocation()->y &&
			unit->getAI()->getCurrentStateID() == AI_ATTACK_MOVE_TO;
	}
	return FALSE;
}

void probe(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit || !unit->getAI())
		return;
	Team* original = unit->getTeam();
	Team* team = newInstance(Team)(const_cast<TeamPrototype*>(original->getPrototype()), TEAM_ID_INVALID);
	unit->setTemporaryTeam(team);
	const Bool ordered = attack(team, unit, "SpawnPoint_SkirmishGollum_3");
	const Bool missing = !attack(team, unit, "MapUnitOpsMissingWaypoint") &&
		unit->getAI()->getCurrentStateID() == AI_ATTACK_MOVE_TO;
	unit->setTemporaryTeam(original);
	deleteInstance(team);
	std::fprintf(stderr, "TEST map-unit-ops frame=%u attack=%d missing=%d\n", ctx.frame, ordered, missing);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailMapUnitOpsTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("map-unit-ops-attack", probe);
#endif
}
}
