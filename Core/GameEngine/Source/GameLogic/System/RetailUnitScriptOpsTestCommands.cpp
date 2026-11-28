#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
const char* const unitScriptName = "ScriptUnitOpsTarget";
const char* const attackWaypoint = "SpawnPoint_SkirmishGollum_3";

ScriptAction* actionNamed(const char* name)
{
	for (Int i = 0; i < ScriptEngine::getActionTemplateCount(); ++i)
		if (TheScriptEngine->getActionTemplate(i)->m_internalName == name)
			return newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(i));
	return nullptr;
}

void runUnitOps(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit)
		return;
	TheScriptEngine->setRetailUnitReference(AsciiString(unitScriptName), unit);

	const Int initialRank = unit->getExperienceTracker()->getRank();

	if (ScriptAction* gainLoud = actionNamed("UNIT_GAIN_LEVEL"))
	{
		gainLoud->getParameter(0)->friend_setString(unitScriptName);
		gainLoud->getParameter(1)->friend_setInt(1);
		TheScriptEngine->friend_executeAction(gainLoud, nullptr);
		deleteInstance(gainLoud);
	}
	const Int rankAfterFirst = unit->getExperienceTracker()->getRank();

	if (ScriptAction* gainQuiet = actionNamed("UNIT_GAIN_LEVEL"))
	{
		gainQuiet->getParameter(0)->friend_setString(unitScriptName);
		gainQuiet->getParameter(1)->friend_setInt(0);
		TheScriptEngine->friend_executeAction(gainQuiet, nullptr);
		deleteInstance(gainQuiet);
	}
	const Int rankAfterSecond = unit->getExperienceTracker()->getRank();

	if (ScriptAction* emotion = actionNamed("UNIT_FORCE_EMOTION"))
	{
		emotion->getParameter(0)->friend_setString(unitScriptName);
		emotion->getParameter(1)->friend_setInt(0);
		emotion->getParameter(2)->friend_setReal(1.5f);
		TheScriptEngine->friend_executeAction(emotion, nullptr);
		deleteInstance(emotion);
	}

	if (ScriptAction* attackMove = actionNamed("ATTACK_MOVE_NAMED_UNIT_TO"))
	{
		attackMove->getParameter(0)->friend_setString(unitScriptName);
		attackMove->getParameter(1)->friend_setString(attackWaypoint);
		TheScriptEngine->friend_executeAction(attackMove, nullptr);
		deleteInstance(attackMove);
	}

	TheScriptEngine->setRetailUnitReference(AsciiString(unitScriptName), nullptr);

	std::fprintf(stderr, "TEST script-unit-ops-init frame=%u r0=%d r1=%d r2=%d\n",
		ctx.frame, initialRank, rankAfterFirst, rankAfterSecond);
}

void probeUnitOps(const RetailTestCommands::Context& ctx)
{
	const Object* unit = ctx.requireObject();
	const Waypoint* waypoint = TheTerrainLogic->getWaypointByName(attackWaypoint);
	if (!unit || !waypoint)
		return;

	const Int rank = unit->getExperienceTracker()->getRank();
	const Bool taunting = unit->getLogicModelConditions().test(MODELCONDITION_EMOTION_TAUNTING);
	const AIUpdateInterface* ai = unit->getAI();
	const Coord3D* goal = ai ? ai->getGoalPosition() : nullptr;
	const Coord3D* target = waypoint->getLocation();
	const Bool hasGoal = goal && target &&
		std::fabs(goal->x - target->x) < 60.0f && std::fabs(goal->y - target->y) < 60.0f;

	std::fprintf(stderr, "TEST script-unit-ops frame=%u rank=%d taunting=%d goal=%d\n",
		ctx.frame, rank, taunting ? 1 : 0, hasGoal ? 1 : 0);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailUnitScriptOpsTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("script-unit-ops-run", runUnitOps);
	registerAction("script-unit-ops-probe", probeUnitOps);
#endif
}
}
