#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/Upgrade.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

#include <cstdio>
#include <cstdlib>

namespace
{
void probePower(const RetailTestCommands::Context& ctx)
{
	const Object* caster = ctx.requireObject(true);
	if (!caster)
		return;
	const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
	const auto* module = power ? caster->getSpecialPowerModule(power) : nullptr;
	if (module)
		std::fprintf(stderr, "PKG44_POWER %u %s %d\n", ctx.frame, ctx.argument, module->isReady());
}

void probeCapture(const RetailTestCommands::Context& ctx)
{
	const Player* player = ctx.findPlayer();
	const Object* creep = ctx.findObject(true);
	std::fprintf(stderr, "PKG44_CAPTURE %u %.*s %s %d\n", ctx.frame, static_cast<Int>(ctx.playerName.size()),
		ctx.playerName.data(), ctx.targetName, player && creep);
}

void probeUpgrade(const RetailTestCommands::Context& ctx)
{
	const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(ctx.argument);
	const Player* player = ctx.findPlayer();
	if (!upgrade || !player)
		return;
	const Object* unit = ctx.findObject(true);
	const Bool done = upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER ? player->hasUpgradeComplete(upgrade) :
		unit && unit->hasUpgrade(upgrade);
	std::fprintf(stderr, "PKG44_UPGRADE %u %s %d\n", ctx.frame, ctx.argument, done);
}

void addPurchasePoints(const RetailTestCommands::Context& ctx)
{
	if (Player* player = ctx.findPlayer())
		player->addSciencePurchasePoints(std::atoi(ctx.argument));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage44TestCommands()
{
#if RTS_ZEROHOUR
	registerAction("pkg44-power", probePower);
	registerAction("pkg44-capture", probeCapture);
	registerAction("pkg44-upgrade", probeUpgrade);
	registerAction("pkg44-points", addPurchasePoints);
#endif
}
}
