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
	auto *area = newInstance(PolygonTrigger)(8, 78002);
	area->setTriggerName(areaName);
	area->addPoint({200, 200, 0});
	area->addPoint({400, 200, 0});
	area->addPoint({400, 400, 0});
	area->addPoint({350, 400, 0});
	area->addPoint({350, 250, 0});
	area->addPoint({250, 250, 0});
	area->addPoint({250, 400, 0});
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

void slideCamera(const RetailTestCommands::Context &ctx)
{
	auto *area = makeArea();
	const Coord3D previous = TheTacticalView->getPosition();

	TheTacticalView->setPosition({380.0f, 300.0f, 0.0f});
	TheTacticalView->updateView();
	TheTacticalView->setPosition({430.0f, 350.0f, 0.0f});
	TheTacticalView->updateView();
	const Coord3D slid = TheTacticalView->getPosition();
	const Bool clamp = area->pointInTrigger(slid) && slid.x > 395.0f && slid.x < 400.0f;
	const Bool slide = slid.y > 330.0f && slid.y < 345.0f;

	auto *probeArea = newInstance(PolygonTrigger)(4, 78003);
	probeArea->setTriggerName("CameraRestrictionProbeFixture");
	const Int px = static_cast<Int>(previous.x);
	const Int py = static_cast<Int>(previous.y);
	probeArea->addPoint({px - 100, py - 100, 0});
	probeArea->addPoint({px + 100, py - 100, 0});
	probeArea->addPoint({px + 100, py + 100, 0});
	probeArea->addPoint({px - 100, py + 100, 0});
	PolygonTrigger::addPolygonTrigger(probeArea);

	TheTacticalView->setPosition(previous);
	TheTacticalView->setScriptCameraArea(probeArea->getTriggerName());
	TheTacticalView->setPosition({previous.x + 140.0f, previous.y, 0.0f});
	TheTacticalView->updateView();
	const Coord3D probed = TheTacticalView->getPosition();
	const Bool probe = probeArea->pointInTrigger(probed) && probed.x > previous.x + 40.0f && probed.x < previous.x + 100.0f;

	TheTacticalView->setPosition({300.0f, 225.0f, 0.0f});
	TheTacticalView->setScriptCameraArea(areaName);
	TheTacticalView->setPosition(previous);
	PolygonTrigger::removePolygonTrigger(probeArea);
	deleteInstance(probeArea);
	PolygonTrigger::removePolygonTrigger(area);
	deleteInstance(area);
	std::fprintf(stderr, "TEST camera-restriction-slide frame=%u clamp=%d slide=%d probe=%d\n",
		ctx.frame, clamp, slide, probe);
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
	registerAction("camera-restriction-slide", slideCamera);
#endif
}
}
