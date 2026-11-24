#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/ActionManager.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/TunnelContain.h"
#include "GameLogic/Object.h"

namespace
{
void selectForTunnelOrder(Object* object)
{
	GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
	message->friend_setPlayerIndex(object->getControllingPlayer()->getPlayerIndex());
	message->appendBooleanArgument(TRUE);
	message->appendObjectIDArgument(object->getID());
}

void handleTunnelTest(const RetailTestCommands::Context& ctx)
{
	const std::string_view action(ctx.action);
	if (action == "tunnel-state")
	{
		const Object* object = ctx.findObject();
		const Coord3D pos = object ? *object->getPosition() : Coord3D{};
		std::fprintf(stderr, "TEST tunnel frame=%u %s alive=%d contained=%d enclosed=%d unselectable=%d x=%.1f y=%.1f\n",
			ctx.frame, ctx.argument, object && !object->isEffectivelyDead(),
			object && object->getContainedBy(), object && object->testStatus(OBJECT_STATUS_ENCLOSED),
			object && object->testStatus(OBJECT_STATUS_UNSELECTABLE), pos.x, pos.y);
		std::fflush(stderr);
		return;
	}

	Object* object = ctx.requireObject();
	if (!object)
		return;

	if (action == "tunnel-count")
	{
		if (ContainModuleInterface* contain = object->getContain())
		{
			Int total = 0;
			Int full = 0;
			const Bool show = contain->getContainerPipsToShow(total, full);
			std::fprintf(stderr, "TEST tunnel frame=%u %s count=%u pips=%d total=%d full=%d\n",
				ctx.frame, ctx.argument, contain->getContainCount(), show, total, full);
			std::fflush(stderr);
		}
	}
	else if (action == "tunnel-check" && ctx.target)
	{
		const Bool allowed = TheActionManager->canEnterObject(object, ctx.target, CMD_FROM_AI, CHECK_CAPACITY);
		const auto* tunnel = TunnelContain::find(ctx.target);
		std::fprintf(stderr, "TEST tunnel frame=%u %.*s %s allowed=%d reachable=%d\n", ctx.frame,
			static_cast<Int>(ctx.playerName.size()), ctx.playerName.data(), ctx.targetName,
			allowed, tunnel && tunnel->canReachEntrance(object));
		std::fflush(stderr);
	}
	else if (action == "tunnel-geometry")
	{
		if (const auto* tunnel = TunnelContain::find(object))
		{
			const Coord3D entry = tunnel->getEntryPosition();
			const Coord3D approach = tunnel->getEntryOffset();
			Coord3D exit;
			tunnel->getNaturalRallyPoint(exit, TRUE);
			std::fprintf(stderr, "TEST tunnel frame=%u %s entry=%.1f,%.1f approach=%.1f,%.1f exit=%.1f,%.1f\n",
				ctx.frame, ctx.argument, entry.x, entry.y, approach.x, approach.y, exit.x, exit.y);
			std::fflush(stderr);
		}
	}
	else if (action == "tunnel-bone" && ctx.target)
	{
		const auto* tunnel = TunnelContain::find(ctx.target);
		Coord3D bone{};
		const Bool assigned = tunnel && tunnel->getPassengerBonePosition(object->getID(), bone);
		const Coord3D& position = *object->getPosition();
		const Bool positioned = assigned && position.x == bone.x && position.y == bone.y && position.z == bone.z;
		std::fprintf(stderr, "TEST tunnel frame=%u passenger-bone assigned=%d positioned=%d\n",
			ctx.frame, assigned, positioned);
		std::fflush(stderr);
	}
	else if (action == "tunnel-enter" && ctx.target)
	{
		selectForTunnelOrder(object);
		GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_ENTER);
		message->friend_setPlayerIndex(object->getControllingPlayer()->getPlayerIndex());
		message->appendObjectIDArgument(object->getID());
		message->appendObjectIDArgument(ctx.target->getID());
	}
	else if (action == "tunnel-evacuate")
	{
		selectForTunnelOrder(object);
		GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_EVACUATE);
		message->friend_setPlayerIndex(object->getControllingPlayer()->getPlayerIndex());
	}
	else if (action == "tunnel-kill")
	{
		object->kill();
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage48TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("tunnel-", handleTunnelTest);
#endif
}
}
