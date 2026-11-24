#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>
#include <cstdlib>

#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "GameClient/Display.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/RetailHudExtras.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameNetwork/NetCommandMsg.h"

namespace
{
using RetailTestCommands::Context;

void objectiveAction(const Context& ctx)
{
	ScriptAction::ScriptActionType type;
	if (ctx.action == "hud-objective-show")
		type = ScriptAction::SHOW_MISSION_OBJECTIVE;
	else if (ctx.action == "hud-objective-hide")
		type = ScriptAction::HIDE_MISSION_OBJECTIVE;
	else if (ctx.action == "hud-objective-complete")
		type = ScriptAction::MARK_MISSION_OBJECTIVE_COMPLETED;
	else if (ctx.action == "hud-objective-incomplete")
		type = ScriptAction::MARK_MISSION_OBJECTIVE_NOT_COMPLETED;
	else if (ctx.action == "hud-objective-disable")
		type = ScriptAction::DISABLE_OBJECTIVES_SCREEN;
	else
		type = ScriptAction::ENABLE_OBJECTIVES_SCREEN;

	auto* action = newInstance(ScriptAction)(type);
	action->setNextAction(nullptr);
	if (auto* param = action->getParameter(0))
		param->friend_setInt(ctx.ordinal);
	TheScriptEngine->friend_executeAction(action);
	deleteInstance(action);
}

void objectiveState(const Context& ctx)
{
	std::fprintf(stderr, "TEST hud-objective %u %d %d %d %d\n", ctx.frame, ctx.ordinal,
		TheGameLogic->isObjectiveVisible(ctx.ordinal), TheGameLogic->isObjectiveCompleted(ctx.ordinal),
		TheGameLogic->getObjectivesEnabled());
}

void objectivePanel(const Context& ctx)
{
	const GameMode mode = TheGameLogic->getGameMode();
	TheGameLogic->setGameMode(GAME_SINGLE_PLAYER);
	GameWindow* previous = TheWindowManager->winGetFocus();
	RetailObjectives::show();
	GameWindow* panel = TheWindowManager->winGetFocus();
	const Bool opened = panel && panel != previous && TheGameLogic->isGamePaused();
	objectiveAction(ctx);
	std::fprintf(stderr, "TEST hud-objective-panel %u %d %d\n", ctx.frame, opened,
		opened && TheWindowManager->winGetFocus() != panel && !TheGameLogic->isGamePaused());
	RetailObjectives::close();
	TheGameLogic->setGameMode(mode);
}

void tributeSetup(const Context& ctx)
{
	Player* sender = ctx.player;
	Player* recipient = ctx.findPlayer(ctx.targetName);
	if (!sender || !recipient)
		return;
	sender->setPlayerRelationship(recipient, ENEMIES);
	recipient->setPlayerRelationship(sender, ENEMIES);
	sender->getMoney()->setStartingCash(800);
	recipient->getMoney()->setStartingCash(40);
}

void tributeAlly(const Context& ctx)
{
	Player* recipient = ctx.findPlayer(ctx.targetName);
	if (ctx.player && recipient)
	{
		ctx.player->setPlayerRelationship(recipient, ALLIES);
		recipient->setPlayerRelationship(ctx.player, ALLIES);
	}
}

void tributeBalances(const Context& ctx)
{
	const Player* sender = ctx.player;
	const Player* recipient = ctx.findPlayer(ctx.targetName);
	if (sender && recipient)
		std::fprintf(stderr, "TEST hud-tribute %u %u %u\n", ctx.frame,
			sender->getMoney()->countMoney(), recipient->getMoney()->countMoney());
}

void tributeMessage(const Context& ctx)
{
	const Player* recipient = ctx.findPlayer(ctx.targetName);
	if (!ctx.player || !recipient)
		return;
	const Int senderSlot = ThePlayerList->getSlotIndex(ctx.player->getPlayerIndex());
	if (senderSlot < 0 || senderSlot >= MAX_SLOTS)
		return;
	auto* message = newInstance(GameMessage)(GameMessage::MSG_RETAIL_TRIBUTE);
	message->appendIntegerArgument(ThePlayerList->getSlotIndex(recipient->getPlayerIndex()));
	message->appendIntegerArgument(std::atoi(ctx.argument));
	auto* network = newInstance(NetGameCommandMsg)(message);
	network->setPlayerID(senderSlot);
	TheCommandList->appendMessage(network->constructGameMessage());
	deleteInstance(network);
	deleteInstance(message);
}

void tributePanel(const Context& ctx)
{
	const Bool testReset = ctx.action == "hud-tribute-panel-reset";
	const UnsignedInt width = TheDisplay->getWidth();
	const UnsignedInt height = TheDisplay->getHeight();
	TheDisplay->Display::setWidth(1024);
	TheDisplay->Display::setHeight(768);
	GameWindow* previous = TheWindowManager->winGetFocus();
	RetailObjectives::show();
	GameWindow* panel = TheWindowManager->winGetFocus();
	const Bool opened = panel && panel != previous;
	if (opened)
	{
		const auto pointer = [](GameWindowMessage message, Int x, Int y) {
			ICoord2D point{x, y};
			ICoord2D delta{0, 0};
			TheWindowManager->winProcessMouseEvent(message, &point, &delta);
		};
		const auto click = [&pointer](Int x, Int y) {
			pointer(GWM_LEFT_DOWN, x, y);
			pointer(GWM_LEFT_UP, x, y);
		};
		click(485, 167);
		pointer(GWM_LEFT_DOWN, 554, 220);
		pointer(GWM_LEFT_DRAG, 717, 220);
		pointer(GWM_LEFT_UP, 717, 220);
		if (testReset)
			click(473, 392);
		click(665, 392);
		RetailObjectives::close();
	}
	TheDisplay->Display::setWidth(width);
	TheDisplay->Display::setHeight(height);
	std::fprintf(stderr, "TEST hud-tribute-panel %u %d\n", ctx.frame, opened);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailObjectivesTributeTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("hud-objective-show", objectiveAction);
	registerAction("hud-objective-hide", objectiveAction);
	registerAction("hud-objective-complete", objectiveAction);
	registerAction("hud-objective-incomplete", objectiveAction);
	registerAction("hud-objective-disable", objectivePanel);
	registerAction("hud-objective-enable", objectiveAction);
	registerAction("hud-objective-state", objectiveState);
	registerAction("hud-tribute-setup", tributeSetup);
	registerAction("hud-tribute-ally", tributeAlly);
	registerAction("hud-tribute-panel-reset", tributePanel);
	registerAction("hud-tribute-panel-send", tributePanel);
	registerAction("hud-tribute-message", tributeMessage);
	registerAction("hud-tribute-balances", tributeBalances);
#endif
}
}
