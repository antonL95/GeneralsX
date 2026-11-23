#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <algorithm>

#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/HeroRoster.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void handleUpgrade(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;

	const CommandSet* set = TheRetailCommandCatalog->findCommandSet(source->getCommandSetString());
	const ProductionUpdateInterface* production = source->getProductionUpdateInterface();
	const HordeContain* horde = HordeContain::get(source);
	const UpgradeTemplate* requested = TheUpgradeCenter->findUpgrade(ctx.argument);
	const CommandButton* match = nullptr;
	for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
	{
		const CommandButton* button = set->getCommandButton(slot);
		if (!button || (button->getCommandType() != GUI_COMMAND_OBJECT_UPGRADE &&
			button->getCommandType() != GUI_COMMAND_PLAYER_UPGRADE))
			continue;
		const UpgradeTemplate* upgrade = button->getUpgradeTemplate();
		if (!upgrade)
			continue;
		if (upgrade == requested)
			match = button;
		if (ctx.action != "weapon-upgrade-trace")
			continue;
		Player* owner = source->getControllingPlayer();
		const CommandAvailability state = TheControlBar->queryRetailCommandAvailability(button, source);
		for (const UpgradeTemplate* prerequisite : button->getNeededUpgrades())
		{
			const Bool owned = prerequisite->getUpgradeType() == UPGRADE_TYPE_PLAYER ?
				owner->hasUpgradeComplete(prerequisite) : source->hasUpgrade(prerequisite);
			std::fprintf(stderr, "WEAPON-UPGRADE prerequisite frame=%u actor=%s button=%s requires=%s owned=%d\n",
				ctx.frame, ctx.targetName, button->getName().str(), prerequisite->getUpgradeName().str(), owned);
		}
		const char* stateName = state == COMMAND_AVAILABLE ? "enabled" :
			state == COMMAND_HIDDEN || !button->isShowButton() ||
			button->hasRotwkOption(CommandButtonRules::ROTWK_HIDE_WHILE_DISABLED) ? "hidden" : "disabled";
		std::fprintf(stderr, "WEAPON-UPGRADE button frame=%u actor=%s slot=%d name=%s upgrade=%s state=%s prerequisite=%d affected=%d offered=%d affordable=%d\n",
			ctx.frame, ctx.targetName, slot + 1, button->getName().str(), upgrade->getUpgradeName().str(), stateName,
			HeroRoster::buttonUpgradesMet(button, owner, source), source->affectedByUpgrade(upgrade),
			source->canProduceUpgrade(upgrade), TheUpgradeCenter->canAffordUpgrade(owner, upgrade));
	}
	if (ctx.action == "weapon-upgrade-buy")
	{
		if (!match || TheControlBar->queryRetailCommandAvailability(match, source) != COMMAND_AVAILABLE)
			return;
		ctx.selectObject(source);
		GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_QUEUE_UPGRADE);
		msg->appendObjectIDArgument(source->getID());
		msg->appendIntegerArgument(requested->getUpgradeNameKey());
		return;
	}
	const Bool banner = horde && std::any_of(horde->getContainedItemsList()->begin(), horde->getContainedItemsList()->end(),
		[horde](const Object* member) { return !member->isEffectivelyDead() && horde->isBannerCarrier(member); });
	std::fprintf(stderr, "WEAPON-UPGRADE result frame=%u actor=%s set=%s rank=%d banner=%d button=%d queued=%d complete=%d\n",
		ctx.frame, ctx.targetName, source->getCommandSetString().str(), source->getExperienceTracker()->getRank(),
		banner, match != nullptr, production && requested && production->isUpgradeInQueue(requested),
		requested && source->hasUpgrade(requested));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailWeaponSwitchUpgradeTestCommands()
{
#if !RTS_GENERALS
	registerAction("weapon-upgrade-trace", handleUpgrade);
	registerAction("weapon-upgrade-buy", handleUpgrade);
#endif
}
}
