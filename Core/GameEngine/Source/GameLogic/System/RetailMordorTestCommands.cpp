#include "PreRTS.h"

#include <cstdio>
#include <string_view>

#include "Common/ActionManager.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/HotKey.h"
#include "GameClient/View.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/RetailTestCommands.h"

namespace
{
void runRetailMordorTestCommand(std::string_view action, Object* source, Object* target, const char* argument, UnsignedInt frame)
{
	auto* owner = source->getControllingPlayer();
	if (action == "mparity-focus" && TheTacticalView)
		TheTacticalView->lookAt(source->getPosition());
	else if (action == "mparity-enter")
	{
		const Bool allowed = source->getAI() && TheActionManager->canEnterObject(source, target, CMD_FROM_PLAYER, CHECK_CAPACITY);
		std::fprintf(stderr, "MPARITY enter frame=%u allowed=%d\n", frame, allowed);
		if (allowed)
			source->getAI()->aiEnter(target, CMD_FROM_PLAYER);
	}
	else if (action == "mparity-economy")
	{
		const auto* draw = source->getDrawable();
		const auto flag = [draw](ModelConditionFlagType condition) {
			return draw && draw->getModelConditionFlags().test(condition);
		};
		std::fprintf(stderr, "MPARITY economy frame=%u source=%s cash=%u used=%d total=%d prep=%d chop=%d timber=%d\n",
			frame, source->getTemplate()->getName().str(), owner->getMoney()->countMoney(), owner->getCommandPointsUsed(),
			owner->getCommandPointsTotal(), flag(MODELCONDITION_HARVEST_PREPARATION),
			flag(MODELCONDITION_HARVEST_ACTION), flag(MODELCONDITION_CARRYING));
	}
	else if (action == "mparity-price")
	{
		if (const auto* thing = TheThingFactory->findTemplate(argument, FALSE))
			std::fprintf(stderr, "MPARITY price frame=%u %s base=%d price=%d frames=%d\n", frame, argument,
				thing->friend_getBuildCost(), thing->calcCostToBuild(owner), thing->calcTimeToBuild(owner));
	}
	else if (action == "mparity-upgrade-price")
	{
		if (const auto* upgrade = TheUpgradeCenter->findUpgrade(argument))
			std::fprintf(stderr, "MPARITY upgrade-price frame=%u %s price=%d frames=%d\n", frame, argument,
				upgrade->calcCostToBuild(owner), upgrade->calcTimeToBuild(owner));
	}
	else if (action == "mparity-hotkeys" && TheControlBar && TheHotKeyManager)
	{
		const auto* set = TheControlBar->findCommandSet(source->getCommandSetString());
		if (!set)
			return;
		for (Int slot = 0; slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
		{
			const auto* button = set->getCommandButton(slot);
			if (button)
			{
				const auto key = TheHotKeyManager->searchHotKey(button->getTextLabel());
				std::fprintf(stderr, "MPARITY key frame=%u slot=%d %s key=%s radial=%d palantir=%d\n", frame,
					slot + 1, button->getName().str(), key.str(), button->isRadial(), button->isInPalantir());
			}
		}
	}
}
}

namespace RetailTestCommands
{
void registerRetailMordorTestCommands()
{
	registerPrefix("mparity-", [](const Context& ctx) {
		Object* source = ctx.requireObject();
		if (!source)
			return;
		runRetailMordorTestCommand(ctx.action, source, ctx.findTarget(false), ctx.argument, ctx.frame);
	});
}
}
