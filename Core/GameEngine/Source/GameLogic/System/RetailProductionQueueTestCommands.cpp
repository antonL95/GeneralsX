#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/Player.h"
#include "Common/Upgrade.h"
#include <cstdio>
#endif

namespace RetailTestCommands
{
void registerRetailProductionQueueTestCommands()
{
#if !RTS_GENERALS
	registerAction("production-queue-science-points", [](const Context& ctx) {
		if (Player* player = ctx.findPlayer())
			player->addSciencePurchasePoints(ctx.ordinal);
	});
	registerAction("production-queue-player-upgrade", [](const Context& ctx) {
		const Player* player = ctx.findPlayer();
		const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(ctx.argument);
		std::fprintf(stderr, "TEST production-queue-upgrade %u %s complete=%d\n",
			ctx.frame, ctx.argument, player && upgrade && player->hasUpgradeComplete(upgrade));
	});
#endif
}
}
