#include "PreRTS.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/CommandButtonRules.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Module/ClickReactionBehavior.h"
#include "GameLogic/Module/RotwkAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

void runRetailPackage16TestCommand(std::string_view action, Object* source, const char* argument, UnsignedInt frame)
{
	if (action == "pkg16-button")
	{
		const CommandButton* btn = (TheRetailCommandCatalog && argument && *argument)
			? TheRetailCommandCatalog->findCommandButton(argument)
			: nullptr;
		const CommandAvailability avail = (btn && source && TheControlBar)
			? TheControlBar->queryRetailCommandAvailability(btn, source)
			: COMMAND_HIDDEN;
		const ThingTemplate* targetObj = btn ? btn->getThingTemplate() : nullptr;
		std::fprintf(stderr,
			"TEST pkg16 button frame=%u name=%s found=%d clickable=%d show=%d passive=%d doubleClick=%d triggerReady=%d autoDelay=%.2f trigger=%s toggle=%s obj=%s avail=%d\n",
			frame,
			argument ? argument : "-",
			btn ? 1 : 0,
			btn && btn->isClickable() ? 1 : 0,
			btn && btn->isShowButton() ? 1 : 0,
			btn && btn->isRetailPassive() ? 1 : 0,
			btn && btn->isDoubleClick() ? 1 : 0,
			btn && btn->isTriggerWhenReady() ? 1 : 0,
			btn ? btn->getAutoDelay() : 0.0f,
			btn && !btn->getCommandTrigger().isEmpty() ? btn->getCommandTrigger().str() : "-",
			btn && !btn->getToggleButtonName().isEmpty() ? btn->getToggleButtonName().str() : "-",
			targetObj ? targetObj->getName().str() : "-",
			static_cast<int>(avail));
		return;
	}

	if (!source)
		return;

	if (action == "pkg16-click")
	{
		GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
		msg->appendBooleanArgument(TRUE);
		msg->appendObjectIDArgument(source->getID());
		return;
	}

	if (action == "pkg16-click-state")
	{
		const ClickReactionBehavior* click = ClickReactionBehavior::find(source);
		const auto& mc = source->getLogicModelConditions();
		const Int reactAny =
			(mc.test(MODELCONDITION_REACT_1) || mc.test(MODELCONDITION_REACT_2) ||
			 mc.test(MODELCONDITION_REACT_3) || mc.test(MODELCONDITION_REACT_4) ||
			 mc.test(MODELCONDITION_REACT_5)) ? 1 : 0;
		std::fprintf(stderr,
			"TEST pkg16 click frame=%u object=%u template=%s hasMod=%d timer=%d countdown=%d level=%d reactFlag=%d\n",
			frame,
			static_cast<unsigned>(source->getID()),
			source->getTemplate()->getName().str(),
			click ? 1 : 0,
			click ? click->getTimer() : 0,
			click ? click->getReactionCountdown() : 0,
			click ? click->getActiveReactionLevel() : 0,
			reactAny);
		return;
	}

	if (action == "pkg16-auto-state")
	{
		const SpecialPowerTemplate* power = (TheSpecialPowerStore && argument && *argument)
			? TheSpecialPowerStore->findSpecialPowerTemplate(argument)
			: nullptr;
		const SpecialPowerModuleInterface* mod = power ? source->getSpecialPowerModule(power) : nullptr;
		const AutoAbilityBehavior* autoMod = power ? AutoAbilityBehavior::find(source, power) : nullptr;
		std::fprintf(stderr,
			"TEST pkg16 auto frame=%u object=%u template=%s power=%s hasAuto=%d active=%d ready=%d pct=%.3f\n",
			frame,
			static_cast<unsigned>(source->getID()),
			source->getTemplate()->getName().str(),
			argument ? argument : "-",
			autoMod ? 1 : 0,
			autoMod && autoMod->isAutoActive() ? 1 : 0,
			mod && mod->isReady() ? 1 : 0,
			mod ? mod->getPercentReady() : 0.0f);
		return;
	}

	if (action == "pkg16-condition")
	{
		if (argument && *argument)
		{
			std::string_view arg(argument);
			Bool setFlag = TRUE;
			if (arg.starts_with("!"))
			{
				setFlag = FALSE;
				arg.remove_prefix(1);
			}
			AsciiString name(arg.data(), static_cast<Int>(arg.size()));
			ModelConditionFlags flags;
			if (flags.setBitByName(name.str()))
			{
				if (setFlag)
					source->setModelConditionFlags(flags);
				else
					source->clearModelConditionFlags(flags);
			}
		}
		return;
	}
}
#endif
