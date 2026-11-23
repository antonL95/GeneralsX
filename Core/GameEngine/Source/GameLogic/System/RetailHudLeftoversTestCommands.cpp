#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Image.h"
#include "GameClient/Keyboard.h"
#include "GameClient/Mouse.h"
#include "GameClient/View.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Object.h"
#include "GameLogic/Squad.h"

namespace
{
void rankHeroCommand(const RetailTestCommands::Context& ctx)
{
	Object* hero = ctx.requireObject();
	if (hero)
		hero->getExperienceTracker()->addRetailExperience(std::strtof(ctx.argument, nullptr), FALSE);
}

void setRankCommand(const RetailTestCommands::Context& ctx)
{
	Object* hero = ctx.requireObject();
	if (hero)
		hero->getExperienceTracker()->setMinRetailRank(std::atoi(ctx.argument));
}

void probeHudCommand(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object)
		return;

	GameWindow* window = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("ControlBar.wnd:CameoWindow"));
	const Image* portrait = window && !window->winIsHidden() ? window->winGetEnabledImage(0) : nullptr;
	const Drawable* drawable = object->getDrawable();
	const Coord3D& camera = TheTacticalView->getPosition();
	std::fprintf(stderr, "TEST hud-state %u %s portrait=%s rank=%d selectable=%d hidden=%d selected=%d camera=%.2f,%.2f\n",
		ctx.frame, object->getTemplate()->getName().str(), portrait ? portrait->getName().str() : "none",
		object->getExperienceTracker()->getRank(), object->isSelectable(), drawable && drawable->isDrawableEffectivelyHidden(),
		drawable && drawable->isSelected(), camera.x, camera.y);
}

void probeBuilderCommand(const RetailTestCommands::Context& ctx)
{
	const Object* builder = ctx.requireObject();
	if (!builder)
		return;
	const Drawable* drawable = builder->getDrawable();
	std::fprintf(stderr, "TEST hud-builder %u selectable=%d masked=%d hidden=%d\n", ctx.frame,
		builder->isSelectable(), builder->testStatus(OBJECT_STATUS_MASKED), drawable && drawable->isDrawableEffectivelyHidden());
}

void clickHudCommand(const RetailTestCommands::Context& ctx)
{
	const ICoord2D point{ static_cast<Int>(ctx.position.x), static_cast<Int>(ctx.position.y) };
	TheMouse->setPosition(point.x, point.y);
	const GameMessage::Type down = std::strcmp(ctx.argument, "double") == 0 ? GameMessage::MSG_RAW_MOUSE_LEFT_DOUBLE_CLICK :
		GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN;
	for (GameMessage::Type type : {down, GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP})
	{
		GameMessage* event = TheMessageStream->appendMessage(type);
		event->appendPixelArgument(point);
		event->appendIntegerArgument(0);
		event->appendIntegerArgument(timeGetTime());
	}
}

void keyCommand(const RetailTestCommands::Context& ctx)
{
	const Bool assign = std::strcmp(ctx.argument, "assign") == 0;
	const Int modifiers = assign ? KEY_STATE_LCONTROL : 0;
	const auto send = [](Int key, Int state)
	{
		const GameMessage::Type type = (state & KEY_STATE_DOWN) ? GameMessage::MSG_RAW_KEY_DOWN : GameMessage::MSG_RAW_KEY_UP;
		GameMessage* event = TheMessageStream->appendMessage(type);
		event->appendIntegerArgument(key);
		event->appendIntegerArgument(state);
	};
	if (assign)
		send(KEY_LCTRL, KEY_STATE_DOWN | modifiers);
	const Int key = std::strcmp(ctx.argument, "builder") == 0 ? KEY_B : KEY_1;
	send(key, KEY_STATE_DOWN | modifiers);
	send(key, KEY_STATE_UP | modifiers);
	if (assign)
		send(KEY_LCTRL, KEY_STATE_UP);
}

void groupStateCommand(const RetailTestCommands::Context& ctx)
{
	Player* player = ctx.findPlayer();
	Squad* group = player ? player->getHotkeySquad(1) : nullptr;
	std::fprintf(stderr, "TEST hud-group %u size=%zu\n", ctx.frame, group ? group->getLiveObjects().size() : 0);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailHudLeftoversTestCommands()
{
#if !RTS_GENERALS
	registerAction("hud-xp", rankHeroCommand);
	registerAction("hud-rank", setRankCommand);
	registerAction("hud-state", probeHudCommand);
	registerAction("hud-builder", probeBuilderCommand);
	registerAction("hud-click", clickHudCommand);
	registerAction("hud-key", keyCommand);
	registerAction("hud-group-state", groupStateCommand);
#endif
}
}
