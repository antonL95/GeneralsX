#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/Upgrade.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

namespace
{
void handleBuilderCaravan(const RetailTestCommands::Context& ctx)
{
	if (ctx.action == "bcaravan-buy" || ctx.action == "bcaravan-owned")
	{
		Object* buyer = ctx.requireObject();
		if (!buyer)
			return;
		if (ctx.action == "bcaravan-buy")
		{
			ctx.selectObject(buyer);
			GameMessage* order = TheMessageStream->appendMessage(GameMessage::MSG_QUEUE_UPGRADE);
			order->appendIntegerArgument(0);
			order->appendIntegerArgument(TheNameKeyGenerator->nameToKey(ctx.argument));
			return;
		}
		const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(ctx.argument);
		std::fprintf(stderr, "TEST bcaravan owned frame=%u upgrade=%s owned=%d\n", ctx.frame, ctx.argument,
			upgrade && buyer->hasUpgrade(upgrade));
		return;
	}
	if (ctx.action != "bcaravan-census")
		return;
	const Player* player = ctx.findPlayer(ctx.playerName);
	Int living = 0;
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->getControllingPlayer() != player || object->isEffectivelyDead() || object->isDestroyed() ||
			std::strcmp(ctx.targetName, object->getTemplate()->getName().str()) != 0)
			continue;
		++living;
		std::fprintf(stderr, "TEST bcaravan object frame=%u template=%s id=%u x=%.0f y=%.0f progress=%.0f\n", ctx.frame,
			object->getTemplate()->getName().str(), static_cast<unsigned>(object->getID()), object->getPosition()->x,
			object->getPosition()->y, object->getConstructionPercent());
	}
	std::fprintf(stderr, "TEST bcaravan census frame=%u template=%s living=%d\n", ctx.frame,
		ctx.targetName, living);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailBuilderCaravanTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("bcaravan-", handleBuilderCaravan);
#endif
}
}
