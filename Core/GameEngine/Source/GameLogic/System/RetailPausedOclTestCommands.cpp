#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include "Common/ActionManager.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
void pausedOclState(const RetailTestCommands::Context& ctx)
{
	std::fprintf(stderr, "TEST paused-ocl frame=%u object=%s alive=%d\n", ctx.frame, ctx.targetName, ctx.findObject(true) ? 1 : 0);
}

void pausedOclProbe(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	const CommandButton* button = TheRetailCommandCatalog->findCommandButton(ctx.argument);
	if (!object || !button || !button->getSpecialPowerTemplate() || !ctx.hasPosition)
		return;
	const Coord3D point = ctx.groundLocation();
	const SpecialPowerTemplate* power = button->getSpecialPowerTemplate();
	const SpecialPowerModuleInterface* module = object->getSpecialPowerModule(power);
	std::fprintf(stderr, "TEST paused-ocl-probe frame=%u ready=%d allowed=%d target=%d water=%d\n", ctx.frame,
		module && module->isReady(), TheActionManager->canDoSpecialPowerAtLocation(object, &point, CMD_FROM_PLAYER, power, nullptr, button->getOptions()),
		power->isValidRetailTarget(object, nullptr, &point, CMD_FROM_PLAYER), TheTerrainLogic->isUnderwater(point.x, point.y));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPausedOclTestCommands()
{
#if !RTS_GENERALS
	registerAction("paused-ocl-state", pausedOclState);
	registerAction("paused-ocl-probe", pausedOclProbe);
#endif
}
}
