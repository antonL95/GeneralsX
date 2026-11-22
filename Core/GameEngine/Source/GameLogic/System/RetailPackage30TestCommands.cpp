#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include "Common/ActionManager.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/HotKey.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace
{
const CommandButton* findButton(const std::string& name)
{
	if (name.empty())
		return nullptr;
	if (TheRetailCommandCatalog)
	{
		if (const CommandButton* btn = TheRetailCommandCatalog->findCommandButton(AsciiString(name.c_str())))
			return btn;
	}
	if (TheControlBar)
		return TheControlBar->findCommandButton(AsciiString(name.c_str()));
	return nullptr;
}

void logProbe(const RetailTestCommands::Context& ctx, Object* source, const std::string& buttonName, Object* target)
{
	const CommandButton* button = findButton(buttonName);
	if (!button)
	{
		std::fprintf(stderr, "TEST pkg30 probe frame=%u button=%s missing=1\n", ctx.frame, buttonName.c_str());
		return;
	}
	const Bool condOk = button->isModelConditionAllowed(source);
	const Bool containerOk = button->isValidContainerForCommand(source);
	const Bool objTargetOk = target ? button->isValidObjectTarget(source, target) : FALSE;
	const Bool damagedOk = target && button->isDamagedTargetValid(target);
	const SpecialPowerTemplate* power = button->getSpecialPowerTemplate();
	const Bool validToUse = power ?
		button->isValidToUseOn(source, target, target ? target->getPosition() : source->getPosition(), CMD_FROM_SCRIPT) :
		condOk && containerOk;
	const Bool storeOk = power && TheSpecialPowerStore ? TheSpecialPowerStore->canUseSpecialPower(source, power) : FALSE;
	Bool actionOk = FALSE;
	if (power && TheActionManager)
	{
		if (BitIsSet(button->getOptions(), COMMAND_OPTION_NEED_OBJECT_TARGET))
			actionOk = target && TheActionManager->canDoSpecialPowerAtObject(source, target, CMD_FROM_PLAYER, power, button->getOptions());
		else if (BitIsSet(button->getOptions(), NEED_TARGET_POS))
			actionOk = TheActionManager->canDoSpecialPowerAtLocation(source, target ? target->getPosition() : source->getPosition(), CMD_FROM_PLAYER, power, target, button->getOptions());
		else
			actionOk = TheActionManager->canDoSpecialPower(source, power, CMD_FROM_PLAYER, button->getOptions());
	}
	const Bool previewValid = power && TheInGameUI
		? TheInGameUI->canSelectedObjectsDoSpecialPower(button, target, target ? target->getPosition() : source->getPosition(), InGameUI::SELECTION_ANY, button->getOptions(), source)
		: FALSE;
	const char* cursor = previewValid ? button->getCursorName().str() : button->getInvalidCursorName().str();
	const SpecialPowerModuleInterface* spMod = power ? source->getSpecialPowerModule(power) : nullptr;
	const Bool ready = spMod ? spMod->isReady() : FALSE;
	std::fprintf(stderr,
		"TEST pkg30 probe frame=%u object=%s button=%s cond=%d container=%d target=%d damaged=%d valid=%d store=%d action=%d preview=%d cursor=%s ready=%d\n",
		ctx.frame, source->getTemplate()->getName().str(), buttonName.c_str(),
		condOk ? 1 : 0, containerOk ? 1 : 0, objTargetOk ? 1 : 0, damagedOk ? 1 : 0,
		validToUse ? 1 : 0, storeOk ? 1 : 0, actionOk ? 1 : 0, previewValid ? 1 : 0,
		cursor && *cursor ? cursor : "-", ready ? 1 : 0);
}

void handlePackage30(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;
	const std::string_view arguments(ctx.argument);
	const auto separator = arguments.find(',');
	const std::string arg1(arguments.substr(0, separator));
	Object* target = separator == std::string_view::npos ? nullptr : ctx.findTarget(arguments.substr(separator + 1), true);
	if (separator != std::string_view::npos && !target)
	{
		std::fprintf(stderr, "TEST pkg30 frame=%u target missing: %s\n", ctx.frame, ctx.argument);
		return;
	}

	if (ctx.action == "pkg30-probe")
	{
		ctx.selectObject(source);
		logProbe(ctx, source, arg1, target);
	}
	else if (ctx.action == "pkg30-toggle-weapon")
	{
		if (arg1 == "on")
			source->setWeaponSetFlag(WEAPONSET_TOGGLE_1);
		else
			source->clearWeaponSetFlag(WEAPONSET_TOGGLE_1);
		std::fprintf(stderr, "TEST pkg30 toggle frame=%u object=%s toggle1=%d\n",
			ctx.frame, source->getTemplate()->getName().str(),
			source->testWeaponSetFlag(WEAPONSET_TOGGLE_1) ? 1 : 0);
	}
	else if (ctx.action == "pkg30-damage")
	{
		const Real amount = static_cast<Real>(std::atof(arg1.c_str()));
		DamageInfo dmg;
		dmg.in.m_damageType = DAMAGE_UNRESISTABLE;
		dmg.in.m_deathType = DEATH_NORMAL;
		dmg.in.m_sourceID = source->getID();
		dmg.in.m_amount = amount;
		source->attemptDamage(&dmg);
		const BodyModuleInterface* body = source->getBodyModule();
		std::fprintf(stderr, "TEST pkg30 damage frame=%u object=%s hp=%.1f max=%.1f\n",
			ctx.frame, source->getTemplate()->getName().str(),
			body ? body->getHealth() : 0.0f, body ? body->getMaxHealth() : 0.0f);
	}
	else if (ctx.action == "pkg30-dispatch")
	{
		const CommandButton* button = findButton(arg1);
		if (!button || !TheMessageStream)
			return;
		ctx.selectObject(source);
		if (button->getSpecialPowerTemplate())
		{
			const SpecialPowerTemplate* power = button->getSpecialPowerTemplate();
			if (BitIsSet(button->getOptions(), COMMAND_OPTION_NEED_OBJECT_TARGET))
			{
				if (!target)
					return;
				GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT);
				msg->appendIntegerArgument(power->getID());
				msg->appendObjectIDArgument(target->getID());
				msg->appendIntegerArgument(button->getOptions());
				msg->appendObjectIDArgument(source->getID());
			}
			else if (BitIsSet(button->getOptions(), NEED_TARGET_POS))
			{
				GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION);
				msg->appendIntegerArgument(power->getID());
				msg->appendLocationArgument(target ? *target->getPosition() : *source->getPosition());
				msg->appendRealArgument(INVALID_ANGLE);
				msg->appendObjectIDArgument(target ? target->getID() : INVALID_ID);
				msg->appendIntegerArgument(button->getOptions());
				msg->appendObjectIDArgument(source->getID());
			}
			else
			{
				GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER);
				msg->appendIntegerArgument(power->getID());
				msg->appendIntegerArgument(button->getOptions());
				msg->appendObjectIDArgument(source->getID());
			}
		}
		else if (button->getCommandType() == GUI_COMMAND_EVACUATE)
		{
			TheMessageStream->appendMessage(GameMessage::MSG_EVACUATE);
		}
		else if (button->getCommandType() == GUI_COMMAND_EXIT_CONTAINER && target)
		{
			GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_EXIT);
			msg->appendObjectIDArgument(target->getID());
		}
		std::fprintf(stderr, "TEST pkg30 dispatch frame=%u object=%s button=%s\n",
			ctx.frame, source->getTemplate()->getName().str(), arg1.c_str());
	}
	else if (ctx.action == "pkg30-script")
	{
		const CommandButton* button = findButton(arg1);
		if (!button)
			return;
		if (BitIsSet(button->getOptions(), COMMAND_OPTION_NEED_OBJECT_TARGET) && target)
			source->doCommandButtonAtObject(button, target, CMD_FROM_SCRIPT);
		else if (BitIsSet(button->getOptions(), NEED_TARGET_POS))
			source->doCommandButtonAtPosition(button, target ? target->getPosition() : source->getPosition(), CMD_FROM_SCRIPT);
		else
			source->doCommandButton(button, CMD_FROM_SCRIPT);
		const SpecialPowerTemplate* power = button->getSpecialPowerTemplate();
		const SpecialPowerModuleInterface* spMod = power ? source->getSpecialPowerModule(power) : nullptr;
		std::fprintf(stderr, "TEST pkg30 script frame=%u object=%s button=%s ready=%d\n",
			ctx.frame, source->getTemplate()->getName().str(), arg1.c_str(),
			spMod && spMod->isReady() ? 1 : 0);
	}
	else if (ctx.action == "pkg30-cancel")
	{
		const CommandButton* button = findButton(arg1);
		if (!button || !TheInGameUI)
			return;
		ctx.selectObject(source);
		TheInGameUI->setGUICommand(button);
		const Bool entered = TheInGameUI->getGUICommand() == button;
		TheInGameUI->setGUICommand(nullptr);
		const Bool cleared = TheInGameUI->getGUICommand() == nullptr;
		std::fprintf(stderr, "TEST pkg30 cancel frame=%u button=%s entered=%d cleared=%d\n",
			ctx.frame, arg1.c_str(), entered ? 1 : 0, cleared ? 1 : 0);
	}
	else if (ctx.action == "pkg30-hotkey")
	{
		const CommandButton* button = findButton(arg1);
		if (!button || !TheHotKeyManager)
			return;
		ctx.selectObject(source);
		const AsciiString key = TheHotKeyManager->searchHotKey(button->getTextLabel());
		const Bool fired = key.isNotEmpty() && TheHotKeyManager->executeHotKey(key);
		std::fprintf(stderr, "TEST pkg30 hotkey frame=%u object=%s button=%s key=%s fired=%d\n",
			ctx.frame, source->getTemplate()->getName().str(), arg1.c_str(),
			key.isNotEmpty() ? key.str() : "-", fired ? 1 : 0);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage30TestCommands()
{
#if RTS_ZEROHOUR
	registerPrefix("pkg30-", handlePackage30);
#endif
}
}
