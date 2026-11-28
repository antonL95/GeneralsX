#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "Common/Player.h"
#include "Common/Team.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/RetailGate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
class ScopedTeam
{
public:
	explicit ScopedTeam(Object& object) :
		m_object(object),
		m_original(object.getTeam()),
		m_team(newInstance(Team)(const_cast<TeamPrototype*>(m_original->getPrototype()), TEAM_ID_INVALID))
	{
		m_object.setTemporaryTeam(m_team);
	}

	~ScopedTeam()
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

Bool canTeamPathToWaypoint(Team* team, const char* waypointName)
{
	const Int conditionType = ScriptEngine::findRotwkConditionByRetailIndex(139);
	if (conditionType < 0)
		return FALSE;
	auto* condition = newInstance(Condition)(static_cast<Condition::ConditionType>(conditionType));
	condition->getParameter(0)->friend_setString("<This Team>");
	condition->getParameter(1)->friend_setString(waypointName);
	auto* clause = newInstance(OrCondition);
	clause->setFirstAndCondition(condition);
	auto* script = newInstance(Script);
	script->setOrCondition(clause);
	const Bool result = TheScriptEngine->evaluateConditions(script, team);
	deleteInstance(script);
	return result;
}

void probePortalPath(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit)
		return;
	const Object* ladder = ctx.findTarget();
	const auto* portal = DynamicPortalBehaviour::find(ladder);
	Coord3D destination{1150.0f, 2600.0f, 0.0f};
	destination.z = TheAI->pathfinder()->getWallHeightAt(destination);

	const AIUpdateInterface* ai = unit->getAI();
	const Bool direct = ai && TheAI->pathfinder()->clientSafeQuickDoesPathExist(
		ai->getLocomotorSet(), unit->getPosition(), &destination, unit);

	Bool scriptResult = FALSE;
	if (Waypoint* head = TheTerrainLogic->getFirstWaypoint())
	{
		auto* temporary = newInstance(Waypoint)(static_cast<WaypointID>(0x7fff0099), AsciiString("ScriptPortalPathTemp"),
			&destination, AsciiString::TheEmptyString, AsciiString::TheEmptyString, AsciiString::TheEmptyString, FALSE);
		Waypoint* next = head->getNext();
		head->setNext(temporary);
		temporary->setNext(next);
		ScopedTeam team(*unit);
		scriptResult = canTeamPathToWaypoint(team.get(), "ScriptPortalPathTemp");
		head->setNext(next);
		deleteInstance(temporary);
	}

	std::fprintf(stderr, "TEST script-portal-path frame=%u object=%s active=%d direct=%d script=%d\n",
		ctx.frame, unit->getTemplate()->getName().str(),
		portal && portal->isActive() ? 1 : 0, direct ? 1 : 0, scriptResult ? 1 : 0);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPortalPathTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("script-portal-path-probe", probePortalPath);
#endif
}
}
