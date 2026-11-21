#include "PreRTS.h"

#include <cstdio>
#include <cstdlib>

#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/RadiusDecal.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/LifetimeUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/RetailTestCommands.h"
#include "GameLogic/ScriptActions.h"
#include "GameLogic/ScriptEngine.h"

bool RetailTestCommands::runPowerCommand(std::string_view action, Player* player, Object* object, const char* argument, unsigned int frame)
{
#if !RTS_GENERALS
	if (!player)
		return true;
	if (action == "pw-button")
	{
		GameWindow* window = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY(argument));
		const CommandButton* button = window ? static_cast<const CommandButton*>(GadgetButtonGetData(window)) : nullptr;
		if (button && !window->winIsHidden())
		{
			const CommandAvailability availability = TheControlBar->getRetailButtonAvailability(window);
			std::fprintf(stderr, "PW button frame=%u name=%s passive=%d restricted=%d label=%s tooltip=%s enabled=%d\n",
				frame, button->getName().str(), availability == COMMAND_PASSIVE, availability == COMMAND_RESTRICTED,
				button->getTextLabel().str(), button->getDescriptionLabel().str(),
				BitIsSet(window->winGetStatus(), WIN_STATUS_ENABLED));
		}
		return true;
	}
	if (action == "pw-rank")
	{
		if (object)
		{
			auto* tracker = object->getExperienceTracker();
			tracker->setMinRetailRank(std::atoi(argument));
			std::fprintf(stderr, "PW rank frame=%u object=%s rank=%d\n",
				frame, object->getTemplate()->getName().str(), tracker->getRank());
		}
		return true;
	}
	if (action == "pw-purchase")
	{
		const ScienceType science = TheScienceStore->getScienceFromInternalName(argument);
		if (science != SCIENCE_INVALID && player == ThePlayerList->getLocalPlayer())
			TheMessageStream->appendMessage(GameMessage::MSG_PURCHASE_SCIENCE)->appendIntegerArgument(science);
		return true;
	}
	if (action == "pw-cp")
	{
		Int maximum = 0;
		const Int total = player->getCommandPointsTotal(&maximum);
		std::fprintf(stderr, "PW cp frame=%u used=%d total=%d maximum=%d\n",
			frame, player->getCommandPointsUsed(), total, maximum);
		return true;
	}
	if (action == "pw-counters")
	{
		for (const auto type : { ScriptAction::SET_PLAYER_COMMAND_POINTS_AVAILABLE_TO_COUNTER,
			ScriptAction::SET_PLAYER_COMMAND_POINTS_TOTAL_TO_COUNTER })
		{
			const AsciiString name = type == ScriptAction::SET_PLAYER_COMMAND_POINTS_AVAILABLE_TO_COUNTER
				? "PwAvailableCommandPoints" : "PwTotalCommandPoints";
			ScriptAction* script = newInstance(ScriptAction)(type);
			script->setNextAction(nullptr);
			script->getParameter(0)->friend_setString("<Local Player>");
			script->getParameter(1)->friend_setString(name);
			TheScriptActions->executeAction(script);
			deleteInstance(script);
			if (const auto* counter = TheScriptEngine->getCounter(name))
				std::fprintf(stderr, "PW counter frame=%u name=%s value=%d\n", frame, name.str(), counter->value);
		}
		return true;
	}
	if (action == "pw-book")
	{
		Object* book = player->getRetailSpellBook();
		const CommandSet* set = book && TheControlBar ? TheControlBar->findCommandSet(book->getCommandSetString()) : nullptr;
		for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
		{
			const CommandButton* button = set->getCommandButton(slot);
			const SpecialPowerTemplate* power = button ? button->getSpecialPowerTemplate() : nullptr;
			if (!power)
				continue;
			const auto* module = book->getSpecialPowerModule(power);
			const Int cursor = button->getRadiusCursorType();
			std::fprintf(stderr, "PW book frame=%u slot=%d button=%s science=%d ready=%d readyFrame=%u reload=%u radius=%.2f cursor=%d namedDecal=%d\n",
				frame, slot + 1, button->getName().str(), power->isUnlockedFor(player),
				module && module->isReady(), module ? module->getReadyFrame() : 0, power->getReloadTime(), power->getRadiusCursorRadius(),
				cursor, RadiusDecalTemplate::findNamedCursor(cursor) != nullptr);
		}
		return true;
	}
	if (action == "pw-summons")
	{
		for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		{
			if (object->getControllingPlayer() != player || object->isEffectivelyDead() ||
				!(object->isKindOf(KINDOF_SUMMONED) || object->isKindOf(KINDOF_HERO)))
				continue;
			const auto* lifetime = static_cast<const LifetimeUpdate*>(object->findUpdateModule(NAMEKEY("LifetimeUpdate")));
			std::fprintf(stderr, "PW summon frame=%u object=%s id=%u hero=%d summoned=%d selectable=%d lifetime=%u\n",
				frame, object->getTemplate()->getName().str(), static_cast<UnsignedInt>(object->getID()),
				object->isKindOf(KINDOF_HERO), object->isKindOf(KINDOF_SUMMONED), object->isSelectable(),
				lifetime ? lifetime->getLifetimeFrames() : 0);
		}
		return true;
	}
	if (action == "pw-created")
	{
		for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		{
			if (object->getControllingPlayer() != player || object->isEffectivelyDead() ||
				object->getTemplate()->getName() != argument)
				continue;
			const auto* drawable = object->getDrawable();
			std::fprintf(stderr, "PW creation frame=%u name=%s built=%d ai=%s\n", frame, argument,
				drawable && drawable->getModelConditionFlags().test(MODELCONDITION_JUST_BUILT),
				object->getAI() ? object->getAI()->getCurrentStateName().str() : "-");
		}
		return true;
	}
#endif
	return false;
}
