#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/Player.h"
#include "Common/Team.h"
#include "GameLogic/AI.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
const char* const fixtureUnitName = "ScriptRecruitRadiusUnit";
const char* const emptyFixtureUnitName = "ScriptRecruitRadiusEmptyUnit";

void runRecruit(Team* team, Int count, const char* unitType)
{
	for (Int i = 0; i < ScriptEngine::getActionTemplateCount(); ++i)
	{
		if (TheScriptEngine->getActionTemplate(i)->m_internalName != "TEAM_RECRUIT_UNITS")
			continue;
		auto* action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(i));
		action->getParameter(0)->friend_setString("<This Team>");
		action->getParameter(1)->friend_setInt(count);
		action->getParameter(2)->friend_setString(unitType);
		TheScriptEngine->friend_executeAction(action, team);
		deleteInstance(action);
		return;
	}
}

void runTest(const RetailTestCommands::Context& ctx)
{
	Player* owner = ctx.findPlayer("Player_1");
	Object* anchor = ctx.findTarget("Player_1:ScriptRecruitRadiusUnit:0", true);
	Object* nearUnit = ctx.findTarget("Player_1:ScriptRecruitRadiusUnit:1", true);
	Object* farUnit = ctx.findTarget("Player_1:ScriptRecruitRadiusUnit:2", true);
	Object* emptyFar = ctx.findTarget("Player_1:ScriptRecruitRadiusEmptyUnit:0", true);
	const Waypoint* home = TheTerrainLogic->getFirstWaypoint();
	if (!owner || !anchor || !nearUnit || !farUnit || !emptyFar || !home)
		return;

	const TeamPrototype* defaultProto = owner->getDefaultTeam()->getPrototype();
	TeamPrototype* recruitProto = nullptr;
	for (TeamPrototype* candidate : *owner->getPlayerTeams())
	{
		if (candidate && candidate != defaultProto)
		{
			recruitProto = candidate;
			break;
		}
	}
	if (!recruitProto)
		return;

	auto* info = const_cast<TeamTemplateInfo*>(recruitProto->getTemplateInfo());
	const Int savedPriority = info->m_productionPriority;
	const Bool savedHasHome = info->m_hasHomeLocation;
	const Coord3D savedHome = info->m_homeLocation;

	info->m_productionPriority = defaultProto->getTemplateInfo()->m_productionPriority + 100;
	Team* memberTeam = newInstance(Team)(recruitProto, TEAM_ID_INVALID);
	memberTeam->setActive();
	anchor->setTeam(memberTeam);

	runRecruit(memberTeam, 10, fixtureUnitName);
	const Bool anchorOk = anchor->getTeam() == memberTeam;
	const Bool nearOk = nearUnit->getTeam() == memberTeam;
	const Bool farOk = farUnit->getTeam() == memberTeam;

	nearUnit->setTeam(owner->getDefaultTeam());
	anchor->setTeam(owner->getDefaultTeam());
	deleteInstance(memberTeam);

	const Coord3D& homePos = *home->getLocation();
	const Coord3D savedEmptyPos = *emptyFar->getPosition();
	const Real x = homePos.x > 2000.0f ? homePos.x - 600.0f : homePos.x + 600.0f;
	Coord3D farPos{x, homePos.y, TheTerrainLogic->getGroundHeight(x, homePos.y)};
	emptyFar->setPosition(&farPos);

	info->m_hasHomeLocation = TRUE;
	info->m_homeLocation = homePos;
	Team* emptyTeam = newInstance(Team)(recruitProto, TEAM_ID_INVALID);
	emptyTeam->setActive();

	runRecruit(emptyTeam, 1, emptyFixtureUnitName);
	const Bool emptyFarOk = emptyFar->getTeam() == emptyTeam;

	emptyFar->setTeam(owner->getDefaultTeam());
	emptyFar->setPosition(&savedEmptyPos);
	deleteInstance(emptyTeam);

	info->m_productionPriority = savedPriority;
	info->m_hasHomeLocation = savedHasHome;
	info->m_homeLocation = savedHome;

	const Real radius = TheAI && TheAI->getAiData() ? TheAI->getAiData()->m_maxRecruitDistance : 0.0f;
	const Real dx = homePos.x - farPos.x;
	const Real dy = homePos.y - farPos.y;
	const Bool farFromHome = dx * dx + dy * dy > radius * radius;

	std::fprintf(stderr,
		"TEST script-recruit-radius frame=%u radius=%.0f anchor=%d near=%d far=%d emptyFar=%d farFromHome=%d\n",
		ctx.frame, radius, anchorOk, nearOk, farOk, emptyFarOk, farFromHome);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailScriptRecruitRadiusTestCommands()
{
#if !RTS_GENERALS
	registerAction("script-recruit-radius-test", runTest);
#endif
}
}
