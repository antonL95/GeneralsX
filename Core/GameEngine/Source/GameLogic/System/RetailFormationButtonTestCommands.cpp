#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/HotKey.h"
#include "GameClient/Image.h"
#include "GameClient/Mouse.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"

namespace
{
GameWindow* findFormationWindow()
{
	for (Int i = 1; i <= 6; ++i)
	{
		AsciiString name;
		name.format("ControlBar.wnd:RingButton%d", i);
		GameWindow* window = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(name));
		if (!window || window->winIsHidden())
			continue;
		const auto* button = static_cast<const CommandButton*>(GadgetButtonGetData(window));
		if (button && button->getCommandType() == GUI_COMMAND_HORDE_TOGGLE_FORMATION)
			return window;
	}
	return nullptr;
}

void formationPointer(const RetailTestCommands::Context& ctx)
{
	const Bool stance = std::string_view(ctx.argument) == "stance";
	GameWindow* window = stance ? TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("ControlBar.wnd:RingButton1"))
		: findFormationWindow();
	if (!window || window->winIsHidden())
	{
		std::fprintf(stderr, "TEST formation-pointer %u %s missing\n", ctx.frame, ctx.argument);
		return;
	}

	ICoord2D point;
	Int width, height;
	window->winGetScreenPosition(&point.x, &point.y);
	window->winGetSize(&width, &height);
	point.x += width / 2;
	point.y += height / 2;
	TheMouse->setPosition(point.x, point.y);
	GameMessage* move = TheMessageStream->appendMessage(GameMessage::MSG_RAW_MOUSE_POSITION);
	move->appendPixelArgument(point);
	move->appendIntegerArgument(0);
	if (ctx.action == "formation-click")
	{
		for (const auto type : {GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN, GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP})
		{
			GameMessage* button = TheMessageStream->appendMessage(type);
			button->appendPixelArgument(point);
			button->appendIntegerArgument(0);
			button->appendIntegerArgument(TheMouse->getMouseStatus()->time);
		}
	}
	std::fprintf(stderr, "TEST formation-pointer %u %s x=%d y=%d enabled=%d\n", ctx.frame, ctx.argument, point.x, point.y,
		BitIsSet(window->winGetStatus(), WIN_STATUS_ENABLED));
}

void formationHud(const RetailTestCommands::Context& ctx)
{
	GameWindow* ring = findFormationWindow();
	GameWindow* tray = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("ControlBar.wnd:TrayButton1"));
	const auto* button = ring ? static_cast<const CommandButton*>(GadgetButtonGetData(ring)) : nullptr;
	const Image* image = ring ? TheControlBar->getRetailButtonImage(ring) : nullptr;
	std::fprintf(stderr, "TEST formation-hud %u ring=%s tray=%d image=%s key=%s choice0=%s choice1=%s double=%d\n", ctx.frame,
		button ? button->getName().str() : "-", tray && !tray->winIsHidden(), image ? image->getName().str() : "-",
		button ? TheHotKeyManager->searchHotKey(button->getTextLabel()).str() : "-",
		button ? TheHotKeyManager->searchHotKey(button->getRetailChoiceLabel(0)).str() : "-",
		button ? TheHotKeyManager->searchHotKey(button->getRetailChoiceLabel(1)).str() : "-",
		button && button->isDoubleClick());
}

void formationState(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	const HordeContain* horde = HordeContain::get(object);
	if (!horde)
		return;

	Int members = 0;
	Int primary = 0;
	Int alternate = 0;
	for (const Object* member : *horde->getContainedItemsList())
	{
		if (member->isEffectivelyDead() || horde->isBannerCarrier(member))
			continue;
		++members;
		primary += member->getLogicModelConditions().test(MODELCONDITION_PRIMARY_FORMATION);
		alternate += member->getLogicModelConditions().test(MODELCONDITION_ALTERNATE_FORMATION);
	}
	std::fprintf(stderr, "TEST formation-state %u %s %d members=%d primary=%d alternate=%d active=%d\n",
		ctx.frame, object->getTemplate()->getName().str(), ctx.ordinal, members, primary, alternate, horde->isAlternateFormation());
}

void formationMessage(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject();
	const CommandSet* set = source ? TheRetailCommandCatalog->findCommandSet(source->getCommandSetString()) : nullptr;
	for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
	{
		const CommandButton* button = set->getCommandButton(slot);
		if (!button || button->getCommandType() != GUI_COMMAND_HORDE_TOGGLE_FORMATION)
			continue;
		GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_RETAIL_HORDE_COMMAND);
		message->appendObjectIDArgument(source->getID());
		message->appendIntegerArgument(slot);
		message->appendIntegerArgument(std::string_view(ctx.argument) == "on" ? 1 : 0);
		return;
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailFormationButtonTestCommands()
{
#if !RTS_GENERALS
	registerAction("formation-click", formationPointer);
	registerAction("formation-hover", formationPointer);
	registerAction("formation-hud", formationHud);
	registerAction("formation-message", formationMessage);
	registerAction("formation-state", formationState);
#endif
}
}
