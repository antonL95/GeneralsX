#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "GameClient/ControlBar.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Mouse.h"

namespace
{
GameWindow* findProductionButton(std::string_view command)
{
	for (Int slot = 1; slot <= MAX_COMMANDS_PER_SET; ++slot)
	{
		AsciiString name;
		name.format("ControlBar.wnd:RadialButton%02d", slot);
		GameWindow* window = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(name));
		if (!window || window->winIsHidden())
			continue;
		const auto* button = static_cast<const CommandButton*>(GadgetButtonGetData(window));
		if (button && command == button->getName().str())
			return window;
	}
	return nullptr;
}

void productionButton(const RetailTestCommands::Context& ctx)
{
	GameWindow* window = findProductionButton(ctx.argument);
	if (!window)
	{
		std::fprintf(stderr, "TEST followup frame=%u button=%s missing\n", ctx.frame, ctx.argument);
		return;
	}

	ICoord2D point;
	Int width, height;
	window->winGetScreenPosition(&point.x, &point.y);
	window->winGetSize(&width, &height);
	if (ctx.action == "followup-button-state")
	{
		Int count;
		Real progress;
		TheControlBar->getRetailProduction(window, count, progress);
		const Player* player = ctx.findPlayer();
		std::fprintf(stderr, "TEST followup frame=%u button=%s availability=%d enabled=%d count=%d progress=%.2f cash=%u x=%d y=%d width=%d height=%d\n",
			ctx.frame, ctx.argument, TheControlBar->getRetailButtonAvailability(window), window->winGetEnabled(),
			count, progress, player ? player->getMoney()->countMoney() : 0, point.x, point.y, width, height);
		return;
	}

	point.x += width / 2;
	point.y += height / 2;
	TheMouse->setPosition(point.x, point.y);
	GameMessage* move = TheMessageStream->appendMessage(GameMessage::MSG_RAW_MOUSE_POSITION);
	move->appendPixelArgument(point);
	move->appendIntegerArgument(0);
	const bool right = ctx.action == "followup-right";
	const auto down = right ? GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN : GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN;
	const auto up = right ? GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP : GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP;
	for (const auto type : {down, up})
	{
		GameMessage* button = TheMessageStream->appendMessage(type);
		button->appendPixelArgument(point);
		button->appendIntegerArgument(0);
		button->appendIntegerArgument(TheMouse->getMouseStatus()->time);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailFollowupTestCommands()
{
#if !RTS_GENERALS
	registerAction("followup-button-state", productionButton);
	registerAction("followup-left", productionButton);
	registerAction("followup-right", productionButton);
#endif
}
}
