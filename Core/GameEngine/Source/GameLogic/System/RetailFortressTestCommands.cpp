#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/BuildAssistant.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void purchase(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;

	const CommandSet* set = TheControlBar ? TheControlBar->findCommandSet(source->getCommandSetString()) : nullptr;
	for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
	{
		const CommandButton* button = set->getCommandButton(slot);
		if (!button || button->getName().compareNoCase(ctx.argument) ||
			(button->getCommandType() != GUI_COMMAND_PLAYER_UPGRADE && button->getCommandType() != GUI_COMMAND_OBJECT_UPGRADE))
			continue;

		const UpgradeTemplate* upgrade = button->getUpgradeTemplate();
		ProductionUpdateInterface* production = source->getProductionUpdateInterface();
		const Bool available = upgrade && production && production->canQueueUpgrade(upgrade) == CANMAKE_OK;
		std::fprintf(stderr, "FORTRESS order %u %s %s %s available=%d frames=%d\n",
			ctx.frame, ctx.targetName, button->getName().str(), upgrade ? upgrade->getUpgradeName().str() : "-",
			available, upgrade ? upgrade->calcTimeToBuild(source->getControllingPlayer()) : 0);
		if (available)
		{
			const PlayerIndex player = source->getControllingPlayer()->getPlayerIndex();
			GameMessage* select = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
			select->friend_setPlayerIndex(player);
			select->appendBooleanArgument(TRUE);
			select->appendObjectIDArgument(source->getID());
			GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_QUEUE_UPGRADE);
			message->friend_setPlayerIndex(player);
			message->appendObjectIDArgument(source->getID());
			message->appendIntegerArgument(upgrade->getUpgradeNameKey());
		}
		return;
	}
	std::fprintf(stderr, "FORTRESS order %u %s %s - available=0 frames=0\n", ctx.frame, ctx.targetName, ctx.argument);
}

void sample(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;

	const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(ctx.argument);
	if (!upgrade)
		return;
	const Player* owner = source->getControllingPlayer();
	const Bool complete = upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER ?
		owner->hasUpgradeComplete(upgrade) : source->hasUpgrade(upgrade);
	const ProductionUpdateInterface* production = source->getProductionUpdateInterface();
	const Bool queued = production && production->isUpgradeInQueue(upgrade);
	const Drawable* drawable = source->getDrawable();
	std::fprintf(stderr, "FORTRESS state %u %s %s id=%u queued=%d complete=%d\n",
		ctx.frame, ctx.targetName, upgrade->getUpgradeName().str(),
		drawable ? static_cast<UnsignedInt>(drawable->getID()) : 0, queued, complete);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailFortressTestCommands()
{
#if !RTS_GENERALS
	registerAction("fortress-purchase", purchase);
	registerAction("fortress-sample", sample);
#endif
}
}
