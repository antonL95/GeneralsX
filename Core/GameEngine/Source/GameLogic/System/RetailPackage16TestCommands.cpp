#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "Common/MessageStream.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/CommandButtonRules.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/GameLogic.h"
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
		const ThingTemplate* resolvedObj = btn ? btn->resolveUpgradeTargetTemplate(source) : nullptr;
		std::string triggersJoined;
		if (btn)
		{
			for (const AsciiString& trig : btn->getCommandTriggers())
			{
				if (!triggersJoined.empty())
					triggersJoined.push_back(',');
				triggersJoined.append(trig.str());
			}
		}
		const CommandButton* resolvedToggle = (btn && source) ? btn->resolveRetailToggleButton(source) : btn;
		std::fprintf(stderr,
			"TEST pkg16 button frame=%u name=%s found=%d clickable=%d show=%d passive=%d doubleClick=%d triggerReady=%d autoDelay=%.2f triggerCount=%zu trigger=%s toggle=%s resolvedToggle=%s obj=%s resolvedObj=%s objName=%s avail=%d\n",
			frame,
			argument ? argument : "-",
			btn ? 1 : 0,
			btn && btn->isClickable() ? 1 : 0,
			btn && btn->isShowButton() ? 1 : 0,
			btn && btn->isRetailPassive() ? 1 : 0,
			btn && btn->isDoubleClick() ? 1 : 0,
			btn && btn->isTriggerWhenReady() ? 1 : 0,
			btn ? btn->getAutoDelay() : 0.0f,
			btn ? btn->getCommandTriggers().size() : 0u,
			triggersJoined.empty() ? "-" : triggersJoined.c_str(),
			btn && !btn->getToggleButtonName().isEmpty() ? btn->getToggleButtonName().str() : "-",
			resolvedToggle ? resolvedToggle->getName().str() : "-",
			targetObj ? targetObj->getName().str() : "-",
			resolvedObj ? resolvedObj->getName().str() : "-",
			btn && !btn->getThingTemplateName().isEmpty() ? btn->getThingTemplateName().str() : "-",
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
		msg->appendBooleanArgument(TRUE);
		return;
	}

	if (action == "pkg16-click-drag")
	{
		GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
		msg->appendBooleanArgument(TRUE);
		msg->appendObjectIDArgument(source->getID());
		return;
	}

	if (action == "pkg16-auto-toggle")
	{
		const Int slot = (argument && *argument) ? std::atoi(argument) : 0;
		GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_RETAIL_AUTO_ABILITY);
		msg->appendObjectIDArgument(source->getID());
		msg->appendIntegerArgument(slot);
		return;
	}

	if (action == "pkg16-queue-upgrade")
	{
		if (!TheUpgradeCenter || !argument || !*argument)
			return;
		const UpgradeTemplate* upg = TheUpgradeCenter->findUpgrade(argument);
		if (!upg)
			return;
		if (Player* p = source->getControllingPlayer())
		{
			if (p->getMoney()->countMoney() < 5000)
				p->getMoney()->deposit(5000, FALSE, FALSE);
		}
		GameMessage* selMsg = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
		selMsg->appendBooleanArgument(TRUE);
		selMsg->appendObjectIDArgument(source->getID());
		GameMessage* upgMsg = TheMessageStream->appendMessage(GameMessage::MSG_QUEUE_UPGRADE);
		upgMsg->appendObjectIDArgument(source->getID());
		upgMsg->appendIntegerArgument(upg->getUpgradeNameKey());
		return;
	}

	if (action == "pkg16-count-template")
	{
		if (!argument || !*argument || !TheGameLogic)
			return;
		const Player* owner = source->getControllingPlayer();
		Int count = 0;
		for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		{
			if (obj->isDestroyed() || obj->isEffectivelyDead() || !obj->getTemplate())
				continue;
			if (owner && obj->getControllingPlayer() != owner)
				continue;
			if (obj->getTemplate()->getName().compareNoCase(argument) == 0)
				++count;
		}
		std::fprintf(stderr,
			"TEST pkg16 count frame=%u template=%s count=%d\n",
			frame,
			argument,
			count);
		return;
	}

	if (action == "pkg16-group-special-power")
	{
		if (!TheSpecialPowerStore || !argument || !*argument || !source->getControllingPlayer())
			return;
		const SpecialPowerTemplate* power = TheSpecialPowerStore->findSpecialPowerTemplate(argument);
		if (!power)
			return;
		const CommandButton* btn = TheRetailCommandCatalog ? TheRetailCommandCatalog->findPowerButton(source, power) : nullptr;
		const UnsignedInt options = btn ? btn->getOptions() : 0;
		GameMessage* sel = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
		sel->appendBooleanArgument(TRUE);
		for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		{
			if (!obj->isEffectivelyDead() && obj->getTemplate() == source->getTemplate() &&
				obj->getControllingPlayer() == source->getControllingPlayer())
			{
				sel->appendObjectIDArgument(obj->getID());
			}
		}
		GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER);
		msg->appendIntegerArgument(power->getID());
		msg->appendIntegerArgument(options);
		msg->appendObjectIDArgument(INVALID_ID);
		return;
	}

	if (action == "pkg16-ai-special-power")
	{
		if (!TheSpecialPowerStore || !argument || !*argument)
			return;
		const SpecialPowerTemplate* power = TheSpecialPowerStore->findSpecialPowerTemplate(argument);
		if (!power)
			return;
		const CommandButton* btn = TheRetailCommandCatalog ? TheRetailCommandCatalog->findPowerButton(source, power) : nullptr;
		const UnsignedInt options = btn ? btn->getOptions() : 0;
		source->doSpecialPower(power, options);
		return;
	}

	if (action == "pkg16-special-power")
	{
		if (!TheSpecialPowerStore || !argument || !*argument)
			return;
		std::string_view spec(argument);
		const size_t at = spec.find('@');
		std::string powerName(at == std::string_view::npos ? spec : spec.substr(0, at));
		const SpecialPowerTemplate* power = TheSpecialPowerStore->findSpecialPowerTemplate(powerName.c_str());
		if (!power)
			return;
		const CommandButton* btn = TheRetailCommandCatalog ? TheRetailCommandCatalog->findPowerButton(source, power) : nullptr;
		const UnsignedInt options = btn ? btn->getOptions() : 0;
		if (at != std::string_view::npos)
		{
			Coord3D pos = *source->getPosition();
			float x = 0.0f;
			float y = 0.0f;
			if (std::sscanf(argument + at + 1, "%f,%f", &x, &y) == 2)
			{
				pos.x = x;
				pos.y = y;
			}
			GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION);
			msg->appendIntegerArgument(power->getID());
			msg->appendLocationArgument(pos);
			msg->appendRealArgument(INVALID_ANGLE);
			msg->appendObjectIDArgument(INVALID_ID);
			msg->appendIntegerArgument(options);
			msg->appendObjectIDArgument(source->getID());
		}
		else
		{
			GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER);
			msg->appendIntegerArgument(power->getID());
			msg->appendIntegerArgument(options);
			msg->appendObjectIDArgument(source->getID());
		}
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
		const Int firingFlag = source->getLogicModelConditions().test(MODELCONDITION_FIRING_OR_PREATTACK_A) ? 1 : 0;
		std::fprintf(stderr,
			"TEST pkg16 auto frame=%u object=%u template=%s power=%s hasAuto=%d active=%d ready=%d pct=%.3f firingFlag=%d\n",
			frame,
			static_cast<unsigned>(source->getID()),
			source->getTemplate()->getName().str(),
			argument ? argument : "-",
			autoMod ? 1 : 0,
			autoMod && autoMod->isAutoActive() ? 1 : 0,
			mod && mod->isReady() ? 1 : 0,
			mod ? mod->getPercentReady() : 0.0f,
			firingFlag);
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

namespace RetailTestCommands
{
void registerRetailPackage16TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg16-", [](const Context& ctx) {
		Object* source = ctx.requireObject();
		if (!source)
			return;
		runRetailPackage16TestCommand(ctx.action, source, ctx.argument, ctx.frame);
	});
#endif
}
}
