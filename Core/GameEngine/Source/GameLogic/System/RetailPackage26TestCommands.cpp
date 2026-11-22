#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/CastleBehavior.h"
#include "GameLogic/Module/RetailGate.h"
#include "GameLogic/Module/RotwkParseOnlyModules.h"
#include "GameLogic/Module/WallHubBehavior.h"
#include "GameLogic/Object.h"

namespace
{
void handlePackage26(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject(FALSE);
	if (!source)
		return;

	const std::string_view action = ctx.action;
	const std::string_view arg = ctx.argument ? std::string_view(ctx.argument) : std::string_view();
	if (action == "pkg26-place-footprint")
	{
		TheAI->pathfinder()->addObjectToPathfindMap(source);
		return;
	}

	if (action == "pkg26-name")
	{
		source->setName(AsciiString(ctx.argument));
		return;
	}

	if (action == "pkg26-move")
	{
		if (!ctx.hasPosition)
			return;
		const PlayerIndex player = source->getControllingPlayer()->getPlayerIndex();
		GameMessage* select = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
		select->friend_setPlayerIndex(player);
		select->appendBooleanArgument(TRUE);
		select->appendObjectIDArgument(source->getID());
		GameMessage* move = TheMessageStream->appendMessage(GameMessage::MSG_DO_MOVETO);
		move->friend_setPlayerIndex(player);
		move->appendLocationArgument(ctx.groundLocation());
		return;
	}

	if (action == "pkg26-order-upgrade")
	{
		ctx.selectObject(source);
		GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_QUEUE_UPGRADE);
		msg->appendIntegerArgument(0);
		msg->appendIntegerArgument(TheNameKeyGenerator->nameToKey(ctx.argument));
		return;
	}

	if (action == "pkg26-gate-msg")
	{
		Int cmd = GUI_COMMAND_TOGGLE_GATE;
		if (arg == "open")
			cmd = GUI_COMMAND_OPEN_GATE;
		else if (arg == "close")
			cmd = GUI_COMMAND_CLOSE_GATE;
		if (TheMessageStream)
		{
			GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_RETAIL_GATE);
			msg->friend_setPlayerIndex(source->getControllingPlayer()->getPlayerIndex());
			msg->appendObjectIDArgument(source->getID());
			msg->appendIntegerArgument(cmd);
		}
		return;
	}

	if (action == "pkg26-gate-state")
	{
		const GateOpenAndCloseBehavior* gate = GateOpenAndCloseBehavior::resolveForControl(source);
		const Object* resolvedObj = gate ? gate->getGateObject() : source;
		const Bool passNow = ctx.target ? GateOpenAndCloseBehavior::canPass(resolvedObj, ctx.target, FALSE) : FALSE;
		const Bool passPlan = ctx.target ? GateOpenAndCloseBehavior::canPass(resolvedObj, ctx.target, TRUE) : FALSE;
		std::fprintf(stderr, "TEST pkg26 gate frame=%u object=%s resolved=%s wantsOpen=%d canPassTarget=%d canPassPlan=%d\n",
			ctx.frame, source->getTemplate()->getName().str(), resolvedObj->getTemplate()->getName().str(),
			gate && gate->wantsOpen() ? 1 : 0, passNow ? 1 : 0, passPlan ? 1 : 0);
		return;
	}

	if (action == "pkg26-portal-state")
	{
		const DynamicPortalBehaviour* portal = DynamicPortalBehaviour::find(source);
		const Bool passNow = ctx.target ? GateOpenAndCloseBehavior::canPass(source, ctx.target, FALSE) : FALSE;
		const Bool passPlan = ctx.target ? GateOpenAndCloseBehavior::canPass(source, ctx.target, TRUE) : FALSE;
		std::fprintf(stderr, "TEST pkg26 portal frame=%u object=%s target=%s active=%d canPassTarget=%d canPassPlan=%d\n",
			ctx.frame, source->getTemplate()->getName().str(),
			ctx.target ? ctx.target->getTemplate()->getName().str() : "-",
			portal && portal->isActive() ? 1 : 0, passNow ? 1 : 0, passPlan ? 1 : 0);
		return;
	}

	if (action == "pkg26-castle-member")
	{
		const CastleMemberBehavior* cm = CastleMemberBehavior::find(source);
		const Player* owner = source->getControllingPlayer();
		std::fprintf(stderr, "TEST pkg26 castle-member frame=%u object=%s owner=%s hasModule=%d linked=%d isKeep=%d owned=%d storedPrice=%u\n",
			ctx.frame, source->getTemplate()->getName().str(),
			owner ? owner->getPlayerName().str() : "-",
			cm != nullptr ? 1 : 0,
			cm && cm->getCastleID() != INVALID_ID ? 1 : 0,
			cm && cm->isKeep() ? 1 : 0,
			cm && cm->isOwnedByCastle() ? 1 : 0,
			cm ? cm->getStoredUpgradePrice() : 0u);
		return;
	}

	if (action == "pkg26-castle-transfer")
	{
		Player* nextOwner = ctx.findPlayer(ctx.argument);
		if (nextOwner && nextOwner->getDefaultTeam())
			source->setTeam(nextOwner->getDefaultTeam());
		return;
	}

	if (action == "pkg26-rebuild-kill")
	{
		source->kill();
		return;
	}


	if (action == "pkg26-rebuild-msg")
	{
		GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_REPAIR);
		msg->friend_setPlayerIndex(source->getControllingPlayer()->getPlayerIndex());
		msg->appendObjectIDArgument(source->getID());
		msg->appendIntegerArgument(GUI_COMMAND_START_SELF_REPAIR);
		return;
	}

	if (action == "pkg26-rebuild-state")
	{
		const GettingBuiltBehavior* gb = GettingBuiltBehavior::find(*source);
		std::fprintf(stderr, "TEST pkg26 rebuild frame=%u object=%s dead=%d underConst=%d rebuildFrames=%u waiting=%d rebuilding=%d\n",
			ctx.frame, source->getTemplate()->getName().str(),
			source->isEffectivelyDead() ? 1 : 0,
			source->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) ? 1 : 0,
			gb ? gb->getRebuildFrames() : 0u,
			gb && gb->isWaitingToRebuild() ? 1 : 0,
			gb && gb->isRebuildingFromRubble() ? 1 : 0);
		return;
	}

	if (action == "pkg26-wall-upgrade-state")
	{
		const WallUpgradeUpdate* wu = WallUpgradeUpdate::find(source);
		const CastleMemberBehavior* cm = CastleMemberBehavior::find(source);
		std::fprintf(stderr, "TEST pkg26 wall-upgrade frame=%u object=%s hasModule=%d hasWall=%d waiting=%d storedPrice=%u\n",
			ctx.frame, source->getTemplate()->getName().str(),
			wu != nullptr ? 1 : 0,
			wu && wu->getSupportingWallID() != INVALID_ID ? 1 : 0,
			wu && wu->isWaitingForWall() ? 1 : 0,
			cm ? cm->getStoredUpgradePrice() : 0u);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage26TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg26-", handlePackage26);
#endif
}
}
