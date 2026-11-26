#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>
#include "Common/Team.h"
#include "Common/INI.h"
#include "GameLogic/ExperienceLevelStore.h"
#include "GameLogic/ExperienceTracker.h"
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

void experienceProbe(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	Object* receiver = ctx.findTarget(std::string_view("Player_1:MapScriptUnitOpsXP:1"));
	Object* control = ctx.findTarget(std::string_view("Player_1:MapScriptUnitOpsXP:2"));
	if (!source || !receiver || !control)
		return;
	INI ini;
	ini.loadText("map-unit-ops-levels", R"(
ExperienceLevel MapScriptUnitOpsXPLevel
    TargetNames = MapScriptUnitOpsXP
    RequiredExperience = 1
    Rank = 1
End
)", INI_LOAD_OVERWRITE, "ExperienceLevel", ExperienceLevelStore::parseDefinition);
	if (source->getName().isEmpty())
	{
		source->setName("MapScriptUnitOpsXPSource");
		TheScriptEngine->addObjectToCache(source);
	}
	auto* from = source->getExperienceTracker();
	auto* to = receiver->getExperienceTracker();
	auto* comparison = control->getExperienceTracker();
	from->setTrainable(TRUE);
	to->setTrainable(TRUE);
	comparison->setTrainable(TRUE);
	from->initializeRetailLevel();
	to->initializeRetailLevel();
	comparison->initializeRetailLevel();
	from->setExperienceScalar(0.5f);
	from->setExperienceSink(receiver->getID());
	const Real before = to->getRetailExperience();
	const Real sourceBefore = from->getRetailExperience();
	const Real comparisonBefore = comparison->getRetailExperience();
	comparison->addRetailExperience(5.0f, TRUE);
	const Real expected = comparison->getRetailExperience() - comparisonBefore;
	comparison->addRetailExperience(2.0f, TRUE);
	const Real expectedLoss = comparison->getRetailExperience() - comparisonBefore - expected;
	Bool registered = FALSE;
	Real gain = 0.0f;
	for (Int i = 0; i < ScriptEngine::getActionTemplateCount(); ++i)
	{
		const Template* definition = TheScriptEngine->getActionTemplate(i);
		if (definition->m_internalName != "UNIT_GIVE_EXPERIENCE_POINTS" ||
			ScriptEngine::findRotwkActionByRetailIndex(359) != i || definition->getNumParameters() != 2 ||
			definition->getParameterType(0) != Parameter::UNIT || definition->getParameterType(1) != Parameter::INT)
			continue;
		auto* action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(i));
		action->getParameter(0)->friend_setString("MapScriptUnitOpsXPSource");
		action->getParameter(1)->friend_setInt(10);
		TheScriptEngine->friend_executeAction(action, nullptr);
		gain = to->getRetailExperience() - before;
		action->getParameter(1)->friend_setInt(-4);
		TheScriptEngine->friend_executeAction(action, nullptr);
		deleteInstance(action);
		registered = TRUE;
	}
	const Real loss = before + gain - to->getRetailExperience();
	std::fprintf(stderr, "TEST map-unit-ops xp frame=%u sink=%d amount=%d negative=%d\n", ctx.frame,
		registered && from->getRetailExperience() == sourceBefore && gain > 0.0f, gain == expected && expected > 0.0f,
		loss == expectedLoss && expectedLoss > 0.0f);
}

void teleportProbe(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit)
		return;
	if (unit->getName().isEmpty())
	{
		unit->setName("MapScriptUnitOpsNamed");
		TheScriptEngine->addObjectToCache(unit);
	}
	const Coord3D origin = *unit->getPosition();
	const Waypoint* destination = TheTerrainLogic->getWaypointByName("SpawnPoint_SkirmishGollum_3");
	Bool moved = FALSE;
	Bool missing = FALSE;
	for (Int i = 0; i < ScriptEngine::getActionTemplateCount(); ++i)
	{
		const Template* definition = TheScriptEngine->getActionTemplate(i);
		if (definition->m_internalName != "UNIT_TELEPORT_TO_WAYPOINT" ||
			ScriptEngine::findRotwkActionByRetailIndex(502) != i || definition->getNumParameters() != 2 ||
			definition->getParameterType(0) != Parameter::UNIT || definition->getParameterType(1) != Parameter::WAYPOINT)
			continue;
		auto* action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(i));
		action->getParameter(0)->friend_setString("MapScriptUnitOpsNamed");
		action->getParameter(1)->friend_setString("MapUnitOpsMissingWaypoint");
		TheScriptEngine->friend_executeAction(action, nullptr);
		missing = unit->getPosition()->x == origin.x && unit->getPosition()->y == origin.y;
		action->getParameter(0)->friend_setString("MapUnitOpsMissingUnit");
		action->getParameter(1)->friend_setString("SpawnPoint_SkirmishGollum_3");
		TheScriptEngine->friend_executeAction(action, nullptr);
		missing = missing && unit->getPosition()->x == origin.x && unit->getPosition()->y == origin.y;
		action->getParameter(0)->friend_setString("MapScriptUnitOpsNamed");
		TheScriptEngine->friend_executeAction(action, nullptr);
		deleteInstance(action);
		const Coord3D* position = unit->getPosition();
		moved = destination && position->x == destination->getLocation()->x && position->y == destination->getLocation()->y;
		unit->setPosition(&origin);
	}
	std::fprintf(stderr, "TEST map-unit-ops teleport frame=%u moved=%d missing=%d\n", ctx.frame, moved, missing);
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
	registerAction("map-unit-ops-teleport", teleportProbe);
	registerAction("map-unit-ops-xp", experienceProbe);
#endif
}
}
