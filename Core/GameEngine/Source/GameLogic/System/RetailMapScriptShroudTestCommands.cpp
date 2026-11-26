#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include "Common/Player.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

#include <cstdio>

namespace
{
const char* const revealArea = "MapScriptShroudRevealArea";
const char* const overlapArea = "MapScriptShroudOverlapArea";
const char* const pathArea = "MapScriptShroudPathArea";
const char* const firstLook = "MapScriptShroudFirstLook";
const char* const secondLook = "MapScriptShroudSecondLook";

void addArea(const char* name, std::initializer_list<ICoord3D> points)
{
	if (PolygonTrigger::findByName(name))
		return;
	auto* area = newInstance(PolygonTrigger)(static_cast<Int>(points.size()));
	area->setTriggerName(name);
	for (const ICoord3D& point : points)
		area->addPoint(point);
	PolygonTrigger::addPolygonTrigger(area);
}

void areas(const RetailTestCommands::Context&)
{
	addArea(revealArea, {{720, 105, 0}, {850, 105, 0}, {850, 155, 0}, {780, 155, 0}, {780, 230, 0}, {720, 230, 0}});
	addArea(overlapArea, {{785, 110, 0}, {845, 110, 0}, {845, 145, 0}, {785, 145, 0}});
	addArea(pathArea, {{660, 250, 0}, {680, 250, 0}, {680, 305, 0}, {660, 305, 0}});
}

void run(const char* name, const char* first, const char* second, const char* third, Int enabled = 0)
{
	for (Int i = 0; i < ScriptEngine::getActionTemplateCount(); ++i)
	{
		if (TheScriptEngine->getActionTemplate(i)->m_internalName != name)
			continue;
		auto* action = newInstance(ScriptAction)(static_cast<ScriptAction::ScriptActionType>(i));
		action->getParameter(0)->friend_setString(first);
		if (Parameter* parameter = action->getParameter(1))
		{
			parameter->friend_setString(second);
			parameter->friend_setInt(enabled);
		}
		if (Parameter* parameter = action->getParameter(2))
			parameter->friend_setString(third);
		TheScriptEngine->friend_executeAction(action, nullptr);
		deleteInstance(action);
		return;
	}
}

void reveal(const RetailTestCommands::Context&)
{
	run("MAP_REVEAL_PERMANENTLY_IN_TRIGGER", revealArea, "PlyrCivilian", firstLook);
	run("MAP_REVEAL_PERMANENTLY_IN_TRIGGER", revealArea, "PlyrCivilian", firstLook);
	run("MAP_REVEAL_PERMANENTLY_IN_TRIGGER", overlapArea, "PlyrCivilian", secondLook);
	run("HUMAN_IMPASSABLE_AREA", pathArea, "", "", TRUE);
}

void undoFirst(const RetailTestCommands::Context&)
{
	run("MAP_UNDO_REVEAL_PERMANENTLY_IN_TRIGGER", firstLook, "", "");
	run("MAP_UNDO_REVEAL_PERMANENTLY_IN_TRIGGER", firstLook, "", "");
}

void undoSecond(const RetailTestCommands::Context&)
{
	run("MAP_UNDO_REVEAL_PERMANENTLY_IN_TRIGGER", secondLook, "", "");
	run("HUMAN_IMPASSABLE_AREA", pathArea, "", "", FALSE);
}

void probe(const RetailTestCommands::Context& ctx)
{
	const Player* viewer = ctx.findPlayer("PlyrCivilian");
	Object* human = ctx.requireObject();
	Object* other = ctx.findTarget("PlyrCreeps:MapScriptShroudFixtureUnit:0", true);
	if (!viewer || !human || !other)
		return;
	const auto clear = [viewer](Real x, Real y) {
		const Coord3D spot{x, y, 0.0f};
		return ThePartitionManager->getShroudStatusForPlayer(viewer->getPlayerIndex(), &spot) == CELLSHROUD_CLEAR;
	};
	const auto canCross = [](const Object& unit) {
		Coord3D destination = *unit.getPosition();
		destination.x = 690.0f;
		return TheAI->pathfinder()->isMovementSegmentPassable(&unit, LOCOMOTORSURFACE_GROUND, LAYER_GROUND,
			*unit.getPosition(), destination, TRUE);
	};
	const auto canReach = [](Object& unit) {
		Coord3D destination = *unit.getPosition();
		destination.x = 675.0f;
		return TheAI->pathfinder()->slowDoesPathExist(&unit, unit.getPosition(), &destination);
	};
	const Bool humanGoal = canReach(*human);
	const Bool otherGoal = canReach(*other);
	std::fprintf(stderr, "TEST map-script-shroud frame=%u first=%d overlap=%d notch=%d human=%d other=%d humanGoal=%d otherGoal=%d\n",
		ctx.frame, clear(745, 175), clear(810, 125), clear(815, 195), canCross(*human), canCross(*other), humanGoal, otherGoal);
}

void cleanup(const RetailTestCommands::Context&)
{
	for (const char* name : {revealArea, overlapArea, pathArea})
	{
		PolygonTrigger* area = PolygonTrigger::findByName(name);
		if (!area)
			continue;
		PolygonTrigger::removePolygonTrigger(area);
		deleteInstance(area);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailMapScriptShroudTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("map-script-shroud-areas", areas);
	registerAction("map-script-shroud-reveal", reveal);
	registerAction("map-script-shroud-undo-first", undoFirst);
	registerAction("map-script-shroud-undo-second", undoSecond);
	registerAction("map-script-shroud-probe", probe);
	registerAction("map-script-shroud-cleanup", cleanup);
#endif
}
}
