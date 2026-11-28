#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ActionManager.h"
#include "Common/SpecialPower.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

namespace
{
void probeUnpausePower(const RetailTestCommands::Context& ctx)
{
	Object* caster = ctx.requireObject();
	const SpecialPowerTemplate* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
	const auto* module = caster && power ? static_cast<const SpecialPowerModule*>(caster->getSpecialPowerModule(power)) : nullptr;
	if (!module)
		return;
	std::fprintf(stderr, "TEST unpause-sp frame=%u power=%s ready=%d paused=%d pct=%.3f\n",
		ctx.frame, ctx.argument, module->isReady() ? 1 : 0, module->isPaused() ? 1 : 0, module->getPercentReady());
}

void removeObjectUpgrade(const RetailTestCommands::Context& ctx)
{
	Object* caster = ctx.requireObject();
	const UpgradeTemplate* upgrade = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(ctx.argument) : nullptr;
	if (!caster || !upgrade)
		return;
	caster->removeUpgrade(upgrade);
}

void castUnpausePower(const RetailTestCommands::Context& ctx)
{
	Object* caster = ctx.requireObject();
	const CommandButton* button = TheRetailCommandCatalog ? TheRetailCommandCatalog->findCommandButton(ctx.argument) : nullptr;
	const SpecialPowerTemplate* power = button ? button->getSpecialPowerTemplate() : nullptr;
	if (!caster || !power || !ctx.hasPosition)
		return;
	const Coord3D point = ctx.groundLocation();
	const UnsignedInt options = button->getOptions();
	auto* module = static_cast<SpecialPowerModule*>(caster->getSpecialPowerModule(power));
	if (module)
		module->resetLastCastAccepted();
	const Bool allowed = TheActionManager->canDoSpecialPowerAtLocation(caster, &point, CMD_FROM_PLAYER, power, nullptr, options);
	if (allowed)
		caster->doSpecialPowerAtLocation(power, &point, INVALID_ANGLE, options, FALSE);
	std::fprintf(stderr, "TEST unpause-sp-cast frame=%u power=%s allowed=%d accepted=%d\n",
		ctx.frame, power->getName().str(), allowed ? 1 : 0, module && module->wasLastCastAccepted() ? 1 : 0);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailUnpauseSpecialPowerTestCommands()
{
#if !RTS_GENERALS
	registerAction("unpause-sp-probe", probeUnpausePower);
	registerAction("unpause-sp-remove", removeObjectUpgrade);
	registerAction("unpause-sp-cast", castUnpausePower);
#endif
}
}
