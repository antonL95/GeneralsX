#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "GameClient/View.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
void checkCameraEnteredArea(const RetailTestCommands::Context &ctx)
{
	auto *area = newInstance(PolygonTrigger)(4, 1);
	area->setTriggerName("CameraEnteredAreaFixture");
	area->addPoint({100, 100, 0});
	area->addPoint({150, 100, 0});
	area->addPoint({150, 150, 0});
	area->addPoint({100, 150, 0});
	PolygonTrigger::addPolygonTrigger(area);

	auto *script = newInstance(Script);
	auto *clause = newInstance(OrCondition);
	auto *condition = newInstance(Condition)(Condition::CAMERA_ENTERED_AREA);
	condition->getParameter(0)->friend_setString(area->getTriggerName());
	clause->setFirstAndCondition(condition);
	script->setOrCondition(clause);

	const Coord3D previous = TheTacticalView->getPosition();
	TheTacticalView->setPosition({125.0f, 125.0f, 100.0f});
	const Bool inside = TheScriptEngine->evaluateConditions(script);
	const GameMode mode = TheGameLogic->getGameMode();
	TheGameLogic->setGameMode(GAME_LAN);
	const Bool multiplayer = TheScriptEngine->evaluateConditions(script);
	TheGameLogic->setGameMode(GAME_REPLAY);
	const Bool replay = TheScriptEngine->evaluateConditions(script);
	TheGameLogic->setGameMode(mode);
	TheTacticalView->setPosition({160.0f, 125.0f, 100.0f});
	const Bool outside = TheScriptEngine->evaluateConditions(script);
	TheTacticalView->setPosition(previous);

	deleteInstance(script);
	PolygonTrigger::removePolygonTrigger(area);
	deleteInstance(area);
	std::fprintf(stderr, "TEST camera-entered-area frame=%u inside=%d outside=%d multiplayer=%d replay=%d\n",
		ctx.frame, inside, outside, multiplayer, replay);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailCameraEnteredAreaTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("camera-entered-area-check", checkCameraEnteredArea);
#endif
}
}
