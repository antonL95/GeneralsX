#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "GameClient/View.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
const char *const areaName = "CameraRestrictionFixture";

PolygonTrigger *makeArea()
{
	auto *area = newInstance(PolygonTrigger)(4, 78002);
	area->setTriggerName(areaName);
	area->addPoint({200, 200, 0});
	area->addPoint({400, 200, 0});
	area->addPoint({400, 400, 0});
	area->addPoint({200, 400, 0});
	PolygonTrigger::addPolygonTrigger(area);
	return area;
}

void restrictCamera(const RetailTestCommands::Context &ctx)
{
	auto *area = makeArea();
	const Coord3D previous = TheTacticalView->getPosition();
	TheTacticalView->setPosition({450.0f, 300.0f, 0.0f});
	auto *action = newInstance(ScriptAction)(ScriptAction::CAMERA_RESTRICT_TO_AREA);
	action->getParameter(0)->friend_setString(areaName);
	TheScriptEngine->friend_executeAction(action);
	deleteInstance(action);
	const Bool inside = area->pointInTrigger(TheTacticalView->getPosition());
	TheTacticalView->setPosition(previous);
	PolygonTrigger::removePolygonTrigger(area);
	deleteInstance(area);
	std::fprintf(stderr, "TEST camera-restriction-arm frame=%u inside=%d\n", ctx.frame, inside);
}

void checkCamera(const RetailTestCommands::Context &ctx)
{
	auto *area = makeArea();
	const Coord3D previous = TheTacticalView->getPosition();
	TheTacticalView->setPosition({450.0f, 300.0f, 0.0f});
	TheTacticalView->updateView();
	const Bool inside = area->pointInTrigger(TheTacticalView->getPosition());
	TheTacticalView->setPosition(previous);
	PolygonTrigger::removePolygonTrigger(area);
	deleteInstance(area);
	std::fprintf(stderr, "TEST camera-restriction-check frame=%u inside=%d\n", ctx.frame, inside);
}

void releaseCamera(const RetailTestCommands::Context &ctx)
{
	auto *action = newInstance(ScriptAction)(ScriptAction::CAMERA_REMOVE_AREA_RESTRICTION);
	TheScriptEngine->friend_executeAction(action);
	deleteInstance(action);
	checkCamera(ctx);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailCameraRestrictionTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("camera-restriction-arm", restrictCamera);
	registerAction("camera-restriction-check", checkCamera);
	registerAction("camera-restriction-release", releaseCamera);
#endif
}
}
