#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
const char* const revealAreaName = "ScriptFogRevealArea";
const Int revealAreaID = 0x7fff00a1;
const Coord3D probeLocation{745.0f, 175.0f, 0.0f};

ScriptAction* actionNamed(const char* name)
{
	for (Int i = 0; i < ScriptEngine::getActionTemplateCount(); ++i)
		if (TheScriptEngine->getActionTemplate(i)->m_internalName == name)
			return newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(i));
	return nullptr;
}

void triggerRevealAndDisableFog(const RetailTestCommands::Context& ctx)
{
	const Player* player = ctx.findPlayer("PlyrCivilian");
	if (!player)
		return;
	const PlayerIndex index = player->getPlayerIndex();
	const CellShroudStatus before = ThePartitionManager->getShroudStatusForPlayer(index, &probeLocation);

	auto* area = newInstance(PolygonTrigger)(4, revealAreaID);
	area->setTriggerName(revealAreaName);
	area->addPoint({720, 105, 0});
	area->addPoint({850, 105, 0});
	area->addPoint({850, 230, 0});
	area->addPoint({720, 230, 0});
	PolygonTrigger::addPolygonTrigger(area);

	if (ScriptAction* action = actionNamed("MAP_REVEAL_IN_TRIGGER"))
	{
		action->getParameter(0)->friend_setString(revealAreaName);
		action->getParameter(1)->friend_setString("PlyrCivilian");
		TheScriptEngine->friend_executeAction(action, nullptr);
		deleteInstance(action);
	}
	PolygonTrigger::removePolygonTrigger(area);
	deleteInstance(area);

	const CellShroudStatus afterReveal = ThePartitionManager->getShroudStatusForPlayer(index, &probeLocation);

	if (ScriptAction* action = actionNamed("SET_LOGIC_FOG_STATE"))
	{
		action->getParameter(0)->friend_setInt(0);
		TheScriptEngine->friend_executeAction(action, nullptr);
		deleteInstance(action);
	}
	const CellShroudStatus afterFogOff = ThePartitionManager->getShroudStatusForPlayer(index, &probeLocation);

	std::fprintf(stderr, "TEST script-fog-reveal frame=%u before=%d afterReveal=%d afterFogOff=%d fog=%d\n",
		ctx.frame, static_cast<Int>(before), static_cast<Int>(afterReveal),
		static_cast<Int>(afterFogOff), ThePartitionManager->isLogicFogEnabled() ? 1 : 0);
}

void probeAndRestoreFog(const RetailTestCommands::Context& ctx)
{
	const Player* player = ctx.findPlayer("PlyrCivilian");
	if (!player)
		return;
	const PlayerIndex index = player->getPlayerIndex();
	const CellShroudStatus current = ThePartitionManager->getShroudStatusForPlayer(index, &probeLocation);
	const Bool fogBefore = ThePartitionManager->isLogicFogEnabled();

	CellShroudStatus afterFogOn = current;
	if (ctx.frame == 256)
	{
		if (ScriptAction* action = actionNamed("SET_LOGIC_FOG_STATE"))
		{
			action->getParameter(0)->friend_setInt(1);
			TheScriptEngine->friend_executeAction(action, nullptr);
			deleteInstance(action);
		}
		afterFogOn = ThePartitionManager->getShroudStatusForPlayer(index, &probeLocation);
	}

	std::fprintf(stderr, "TEST script-fog-reveal-probe frame=%u current=%d fogBefore=%d afterFogOn=%d fogAfter=%d\n",
		ctx.frame, static_cast<Int>(current), fogBefore ? 1 : 0,
		static_cast<Int>(afterFogOn), ThePartitionManager->isLogicFogEnabled() ? 1 : 0);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailFogRevealTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("script-fog-reveal-run", triggerRevealAndDisableFog);
	registerAction("script-fog-reveal-probe", probeAndRestoreFog);
#endif
}
}
