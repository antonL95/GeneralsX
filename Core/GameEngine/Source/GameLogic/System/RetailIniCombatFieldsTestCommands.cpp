#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/MessageStream.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"

namespace
{
void inspectCombatField(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object)
		return;

	if (ctx.action == "combat-fields-cargo")
	{
		const auto* contain = object->getContain();
		const auto* passengers = contain ? contain->getContainedItemsList() : nullptr;
		Int matching = 0;
		Int other = 0;
		if (passengers)
			for (const Object* passenger : *passengers)
				if (!passenger->isEffectivelyDead())
					++(passenger->getTemplate()->getName() == "CombatFieldsPassenger" ? matching : other);
		std::fprintf(stderr, "TEST combat-fields cargo %u matching=%d other=%d\n", ctx.frame, matching, other);
	}
	else if (ctx.action == "combat-fields-script")
	{
		if (TheRetailCommandCatalog)
			if (const CommandButton* button = TheRetailCommandCatalog->findCommandButton(ctx.argument))
				object->doCommandButton(button, CMD_FROM_SCRIPT);
	}
	else if (ctx.action == "combat-fields-command" || ctx.action == "combat-fields-slot")
	{
		const CommandSet* set = TheRetailCommandCatalog ? TheRetailCommandCatalog->findCommandSet(object->getCommandSetString()) : nullptr;
		for (Int index = 0; set && index < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++index)
		{
			const CommandButton* button = set->getCommandButton(index);
			if (!button || button->getName() != ctx.argument)
				continue;
			if (ctx.action == "combat-fields-command")
			{
				ctx.selectObject(object);
				GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_RETAIL_HORDE_COMMAND);
				msg->appendObjectIDArgument(object->getID());
				msg->appendIntegerArgument(index);
				msg->appendIntegerArgument(-1);
			}
			else
			{
				WeaponSlotType slot = WEAPONSLOT_COUNT;
				object->getCurrentWeapon(&slot);
				const Int state = button->getWeaponToggleIndex(object);
				const auto& images = button->getStateImageNames();
				const char* image = state >= 0 && state < static_cast<Int>(images.size()) ? images[state].str() : "-";
				std::fprintf(stderr, "TEST combat-fields slot %u %d image=%s next=%d available=%d\n", ctx.frame, slot,
					image, button->getNextWeaponToggleSlot(object), button->canUseRetailToggle(object, -1));
			}
			return;
		}
		std::fprintf(stderr, "TEST combat-fields missing button %u %s\n", ctx.frame, ctx.argument);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailIniCombatFieldsTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("combat-fields-", inspectCombatField);
#endif
}
}
