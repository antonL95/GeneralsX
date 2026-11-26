#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "Common/Team.h"
#include "GameClient/RetailEva.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
const char* const guardArea = "TeamScriptOpsFixtureArea";
const Int guardAreaID = 0x7fff0000;
const char* const rally = "SpawnPoint_SkirmishGollum_3";

class FixtureTeam
{
public:
	explicit FixtureTeam(Object& object) :
		m_object(object),
		m_original(object.getTeam()),
		m_team(newInstance(Team)(const_cast<TeamPrototype*>(m_original->getPrototype()), TEAM_ID_INVALID))
	{
		m_object.setTemporaryTeam(m_team);
	}

	~FixtureTeam()
	{
		m_object.setTemporaryTeam(m_original);
		deleteInstance(m_team);
	}

	Team* get() const { return m_team; }

private:
	Object& m_object;
	Team* m_original;
	Team* m_team;
};

ScriptAction* actionNamed(const char* name)
{
	for (Int i = 0; i < ScriptEngine::getActionTemplateCount(); ++i)
		if (TheScriptEngine->getActionTemplate(i)->m_internalName == name)
			return newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(i));
	return nullptr;
}

void emotion(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	ScriptAction* action = actionNamed("TEAM_FORCE_EMOTION");
	if (unit && action)
	{
		FixtureTeam team(*unit);
		action->getParameter(0)->friend_setString("<This Team>");
		action->getParameter(1)->friend_setInt(6);
		action->getParameter(2)->friend_setReal(2.0f);
		TheScriptEngine->friend_executeAction(action, team.get());
	}
	deleteInstance(action);
}

void emotionProbe(const RetailTestCommands::Context& ctx)
{
	const Object* unit = ctx.requireObject();
	if (unit)
		std::fprintf(stderr, "TEST team-script-ops emotion frame=%u active=%d\n", ctx.frame,
			unit->getLogicModelConditions().test(MODELCONDITION_EMOTION_TERROR));
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

void pathProbe(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (unit)
	{
		FixtureTeam team(*unit);
		std::fprintf(stderr, "TEST team-script-ops path frame=%u first=%d reachable=%d\n", ctx.frame,
			team.get()->getFirstItemIn_TeamMemberList() == unit, canPath(team.get(), rally));
	}
}

Bool guardFrom(Team* team, const char* waypoint)
{
	ScriptAction* action = actionNamed("TEAM_GUARD_AREA_FROM_POSITION");
	if (!action)
		return FALSE;
	action->getParameter(0)->friend_setString("<This Team>");
	action->getParameter(1)->friend_setString(guardArea);
	action->getParameter(2)->friend_setString(waypoint);
	TheScriptEngine->friend_executeAction(action, team);
	deleteInstance(action);
	return TRUE;
}

void guardAndPath(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	Waypoint* waypoint = TheTerrainLogic->getWaypointByName(rally);
	if (!unit || !unit->getAI() || !waypoint)
		return;

	auto* area = newInstance(PolygonTrigger)(4, guardAreaID);
	area->setTriggerName(guardArea);
	area->addPoint({980, 3480, 0});
	area->addPoint({1040, 3480, 0});
	area->addPoint({1040, 3540, 0});
	area->addPoint({980, 3540, 0});
	PolygonTrigger::addPolygonTrigger(area);

	Bool reachable, missing, fallback, origin;
	{
		FixtureTeam team(*unit);
		reachable = canPath(team.get(), rally);
		missing = canPath(team.get(), "TeamScriptOpsMissingWaypoint");
		const auto* ai = unit->getAI();
		Coord3D center;
		area->getCenterPoint(&center);
		fallback = guardFrom(team.get(), "TeamScriptOpsMissingWaypoint") && ai->getAreaToGuard() == area &&
			ai->getGuardLocation()->x == center.x && ai->getGuardLocation()->y == center.y;
		const Bool ordered = guardFrom(team.get(), rally);
		const Coord3D& anchor = *waypoint->getLocation();
		origin = ordered && ai->getAreaToGuard() == area &&
			ai->getGuardLocation()->x == anchor.x && ai->getGuardLocation()->y == anchor.y;
	}
	unit->getAI()->aiIdle(CMD_FROM_SCRIPT);
	TheGameLogic->destroyObject(unit);
	PolygonTrigger::removePolygonTrigger(area);
	deleteInstance(area);
	std::fprintf(stderr, "TEST team-script-ops guard frame=%u reachable=%d missing=%d origin=%d fallback=%d removed=%d\n",
		ctx.frame, reachable, missing, origin, fallback, !PolygonTrigger::findByName(guardArea, TRUE));
}

void score(const RetailTestCommands::Context& ctx)
{
	const Object* killer = ctx.requireObject();
	Object* victim = ctx.findTarget();
	if (!killer || !victim)
		return;

	DamageInfo damage;
	damage.in.m_sourceID = killer->getID();
	damage.in.m_damageType = DAMAGE_UNRESISTABLE;
	damage.in.m_deathType = DEATH_NORMAL;
	damage.in.m_kill = TRUE;
	victim->attemptDamage(&damage);
}

void scoreProbe(const RetailTestCommands::Context& ctx)
{
	UnsignedInt frame;
	Coord3D winPosition;
	Coord3D lossPosition;
	const Bool win = RetailEva::getLastPlayed("TeamScriptOpsFixtureWin", frame, &winPosition);
	const Bool loss = RetailEva::getLastPlayed("TeamScriptOpsFixtureLoss", frame, &lossPosition);
	std::fprintf(stderr, "TEST team-script-ops score frame=%u win=%d loss=%d winPosition=%d lossPosition=%d\n",
		ctx.frame, win, loss, win && winPosition.x == 1030.0f && winPosition.y == 3560.0f,
		loss && lossPosition.x == 1060.0f && lossPosition.y == 3560.0f);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailTeamScriptOpsTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("team-script-ops-emotion", emotion);
	registerAction("team-script-ops-emotion-probe", emotionProbe);
	registerAction("team-script-ops-path-probe", pathProbe);
	registerAction("team-script-ops-guard", guardAndPath);
	registerAction("team-script-ops-score", score);
	registerAction("team-script-ops-score-probe", scoreProbe);
#endif
}
}
