#include "PreRTS.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/MessageStream.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"

void runRetailPackage12PickupTestCommand(std::string_view action, Object* source, const char* argument, UnsignedInt frame)
{
	if (action == "pkg12-pickup-order")
	{
		const CommandSet* set = TheRetailCommandCatalog->findCommandSet(source->getCommandSetString());
		for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
		{
			const CommandButton* button = set->getCommandButton(slot);
			if (!button || button->getName() != argument)
				continue;

			GameMessage* selection = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
			selection->appendBooleanArgument(TRUE);
			selection->appendObjectIDArgument(source->getID());
			GameMessage* order = TheMessageStream->appendMessage(GameMessage::MSG_RETAIL_HORDE_COMMAND);
			order->appendObjectIDArgument(source->getID());
			order->appendIntegerArgument(slot);
			order->appendIntegerArgument(-1);
			return;
		}
		std::fprintf(stderr, "TEST pkg12 pickup missing-button frame=%u name=%s\n", frame, argument);
	}
	else if (action == "pkg12-pickup-prey")
	{
		std::fprintf(stderr, "TEST pkg12 pickup-prey frame=%u alive=%d contained=%d\n", frame,
			!source->isEffectivelyDead(), source->getEnclosingContainedBy() != nullptr);
	}
	else if (action == "pkg12-pickup-state")
	{
		const auto* contain = source->getContain();
		const auto* passengers = contain ? contain->getContainedItemsList() : nullptr;
		const Object* passenger = passengers && !passengers->empty() ? passengers->front() : nullptr;
		const Object* goal = source->getAI() ? source->getAI()->getGoalObject() : nullptr;
		std::fprintf(stderr, "TEST pkg12 pickup frame=%u object=%s target=%s passenger=%s club=%d health=%.1f x=%.1f y=%.1f\n",
			frame, source->getTemplate()->getName().str(), goal ? goal->getTemplate()->getName().str() : "-",
			passenger ? passenger->getTemplate()->getName().str() : "-", passenger && passenger->isKindOf(KINDOF_CLUB),
			source->getBodyModule()->getHealth(), source->getPosition()->x, source->getPosition()->y);
	}
}
#endif
