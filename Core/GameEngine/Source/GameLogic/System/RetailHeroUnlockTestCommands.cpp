#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/HeroRoster.h"
#include "GameLogic/Object.h"

namespace
{
void reportHeroUnlock(const RetailTestCommands::Context& ctx)
{
	Object* producer = ctx.requireObject();
	Player* player = ctx.findPlayer();
	if (!producer || !player)
		return;
	const ThingTemplate* hero = TheThingFactory->findTemplate(ctx.argument, FALSE);
	const CommandSet* set = TheControlBar->findCommandSet(producer->getCommandSetString());
	Int slot = -1;
	Int found = 0;
	for (Int entry = 0; hero && set && entry < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++entry)
	{
		const Int heroIndex = HeroRoster::reviveSlotForEntry(set, entry);
		const ThingTemplate* candidate = HeroRoster::heroAt(player, heroIndex);
		if (heroIndex < 0 || !candidate || !candidate->isEquivalentTo(hero))
			continue;
		slot = static_cast<Int>(HeroRoster::slotFor(player, producer, set->getCommandButton(entry), heroIndex));
		found = 1;
	}
	std::fprintf(stderr, "TEST hero-unlock frame=%u set=%s hero=%s listed=%d slot=%d recruitable=%d alive=%d queued=%d\n",
		ctx.frame, producer->getCommandSetString().str(), ctx.argument, found, slot,
		HeroRoster::isReviveTemplateInSet(set, player, producer, hero) ? 1 : 0,
		hero && HeroRoster::isAlive(player, hero) ? 1 : 0, hero && HeroRoster::isQueued(player, hero) ? 1 : 0);
}

void reportFaction(const RetailTestCommands::Context& ctx)
{
	Player* player = ctx.findPlayer();
	std::fprintf(stderr, "TEST hero-unlock-native frame=%u faction=%s\n", ctx.frame,
		player && player->getPlayerTemplate() ? player->getPlayerTemplate()->getName().str() : "-");
}

void reportUpgrade(const RetailTestCommands::Context& ctx)
{
	Player* player = ctx.findPlayer();
	const UpgradeTemplate* upgrade = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(ctx.argument) : nullptr;
	if (!player || !upgrade)
		return;
	std::fprintf(stderr, "TEST hero-unlock-upgrade frame=%u upgrade=%s complete=%d\n", ctx.frame, ctx.argument,
		player->hasUpgradeComplete(upgrade) ? 1 : 0);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailHeroUnlockTestCommands()
{
#if !RTS_GENERALS
	registerAction("hero-unlock-state", reportHeroUnlock);
	registerAction("hero-unlock-upgrade", reportUpgrade);
	registerAction("hero-unlock-native", reportFaction);
#endif
}
}
