#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/HeroRoster.h"
#include "GameLogic/Object.h"

namespace
{
void reportUnlockButton(const RetailTestCommands::Context& ctx)
{
	Object* producer = ctx.requireObject();
	Player* player = ctx.findPlayer();
	if (!producer || !player)
		return;
	const CommandSet* set = TheControlBar->findCommandSet(producer->getCommandSetString());
	const CommandButton* button = nullptr;
	for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
	{
		const CommandButton* candidate = set->getCommandButton(slot);
		if (candidate && !candidate->getName().compareNoCase(ctx.argument))
			button = candidate;
	}
	const ThingTemplate* unit = button ? button->getThingTemplate() : nullptr;
	Int owned = 0;
	if (unit)
	{
		const ThingTemplate* templates[] = { unit };
		Int counts[] = { 0 };
		player->countObjectsByThingTemplate(1, templates, FALSE, counts);
		owned = counts[0];
	}
	std::fprintf(stderr, "TEST unlock frame=%u button=%s listed=%d upgrades=%d available=%d owned=%d\n",
		ctx.frame, ctx.argument, button != nullptr,
		button && HeroRoster::buttonUpgradesMet(button, player, producer),
		button && TheControlBar->queryRetailCommandAvailability(button, producer) == COMMAND_AVAILABLE, owned);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailConditionalUnlockTestCommands()
{
#if !RTS_GENERALS
	registerAction("unlock-button-state", reportUnlockButton);
#endif
}
}
