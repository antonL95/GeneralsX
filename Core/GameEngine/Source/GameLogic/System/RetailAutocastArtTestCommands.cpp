#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>

#include "Common/NameKeyGenerator.h"
#include "GameClient/ControlBar.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"

namespace
{
GameWindow* ringButton(const char* argument)
{
	if (!TheWindowManager)
		return nullptr;
	AsciiString name;
	name.format("ControlBar.wnd:RingButton%d", std::atoi(argument) + 1);
	GameWindow* window = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey(name));
	return window && !window->winIsHidden() ? window : nullptr;
}

void handleAutocastArt(const RetailTestCommands::Context& ctx)
{
	GameWindow* window = ringButton(ctx.argument);
	if (!window)
	{
		std::fprintf(stderr, "TEST autocast-art frame=%u slot=%s missing\n", ctx.frame, ctx.argument);
		return;
	}
	if (ctx.action == "autocast-art-right")
	{
		TheWindowManager->winSendSystemMsg(window->winGetOwner(), GBM_SELECTED_RIGHT, (WindowMsgData)window, window->winGetWindowId());
		return;
	}
	ICoord2D point;
	Int width, height;
	window->winGetScreenPosition(&point.x, &point.y);
	window->winGetSize(&width, &height);
	std::fprintf(stderr, "TEST autocast-art frame=%u slot=%s availability=%d auto=%d x=%d y=%d width=%d height=%d\n",
		ctx.frame, ctx.argument, static_cast<int>(TheControlBar->getRetailButtonAvailability(window)),
		TheControlBar->isRetailAutoAbilityOn(window), point.x, point.y, width, height);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailAutocastArtTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("autocast-art-", handleAutocastArt);
#endif
}
}
