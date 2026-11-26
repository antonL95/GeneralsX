#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>
#include <utility>

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/Keyboard.h"
#include "GameClient/RetailEva.h"
#include "GameLogic/HeroRoster.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/SlaughterHordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
class IdleKeyboard final : public Keyboard
{
	Bool getCapsState() override { return FALSE; }
	void getKey(KeyboardIO* key) override { key->key = KEY_NONE; }
};

struct Chain
{
	Object* ring = nullptr;
	Object* holder = nullptr;
	Object* fortress = nullptr;
	Object* hero = nullptr;
	Int rings = 0;
	const ThingTemplate* heroType = nullptr;
};

Chain findChain(const Player* player)
{
	Chain chain;
	const PlayerTemplate* definition = player ? player->getPlayerTemplate() : nullptr;
	if (definition && !definition->getBuildableRingHeroesMP().empty())
		chain.heroType = TheThingFactory->findTemplate(definition->getBuildableRingHeroesMP().front(), FALSE);
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->isEffectivelyDead() || object->isDestroyed())
			continue;
		if (object->isKindOf(KINDOF_ONE_RING))
		{
			chain.ring = object;
			++chain.rings;
		}
		if (object->testStatus(OBJECT_STATUS_HOLDING_THE_RING))
			chain.holder = object;
		if (object->getControllingPlayer() != player)
			continue;
		if (object->findUpdateModule(NAMEKEY("CitadelSlaughterHordeContain")) &&
			(!chain.fortress || object->getID() < chain.fortress->getID()))
			chain.fortress = object;
		if (chain.heroType && object->getTemplate()->isEquivalentTo(chain.heroType))
			chain.hero = object;
	}
	return chain;
}

Coord3D openSpot(const Object& fortress, Real distance)
{
	Region3D extent;
	TheTerrainLogic->getExtent(&extent);
	Coord3D spot = *fortress.getPosition();
	Coord2D inward = {(extent.lo.x + extent.hi.x) * 0.5f - spot.x, (extent.lo.y + extent.hi.y) * 0.5f - spot.y};
	inward.normalize();
	spot.x += inward.x * distance;
	spot.y += inward.y * distance;
	spot.z = TheTerrainLogic->getGroundHeight(spot.x, spot.y);
	return spot;
}

Object* spawn(const char* name, Player* owner, const Coord3D& position)
{
	const ThingTemplate* what = TheThingFactory->findTemplate(name, FALSE);
	if (!what || !owner)
		return nullptr;
	Object* object = TheThingFactory->newObject(what, owner->getDefaultTeam());
	object->setPosition(&position);
	return object;
}

void order(const RetailTestCommands::Context& ctx, Object& actor, Object& target, bool enter)
{
	if (actor.isLocallyControlled())
	{
		ctx.selectObject(&actor);
		IdleKeyboard idleKeyboard;
		Keyboard* keyboard = std::exchange(TheKeyboard, TheKeyboard ? TheKeyboard : &idleKeyboard);
		const auto message = TheGameClient->evaluateContextCommand(target.getDrawable(), target.getPosition(),
			CommandTranslator::DO_COMMAND);
		TheKeyboard = keyboard;
		std::fprintf(stderr, "TEST ringchain-order frame=%u target=%s command=%s\n", ctx.frame,
			enter ? "fortress" : "ring", message == GameMessage::MSG_ENTER ? "enter"
				: message == GameMessage::MSG_DO_SALVAGE ? "pickup" : message == GameMessage::MSG_INVALID ? "none" : "other");
		return;
	}
	AIUpdateInterface* ai = actor.getAI();
	if (!ai)
		return;
	if (enter)
		ai->aiEnter(&target, CMD_FROM_PLAYER);
	else
		ai->aiMoveToPosition(target.getPosition(), CMD_FROM_PLAYER);
}

void recruit(const RetailTestCommands::Context& ctx, Player& player, const ThingTemplate& hero)
{
	bool queued = false;
	for (Object* object = TheGameLogic->getFirstObject(); object && !queued; object = object->getNextObject())
	{
		if (object->getControllingPlayer() != &player)
			continue;
		ProductionUpdateInterface* production = object->getProductionUpdateInterface();
		const CommandSet* set = TheControlBar->findCommandSet(object->getCommandSetString());
		if (!production || !set || !HeroRoster::isReviveTemplateInSet(set, &player, object, &hero))
			continue;
		if (player.isLocalPlayer())
		{
			ctx.selectObject(object);
			GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_QUEUE_UNIT_CREATE);
			message->appendIntegerArgument(hero.getTemplateID());
			message->appendIntegerArgument(production->requestUniqueUnitID());
			queued = true;
		}
		else
			queued = production->queueCreateUnit(&hero, production->requestUniqueUnitID());
	}
	std::fprintf(stderr, "TEST ringchain-recruit frame=%u queued=%d\n", ctx.frame, queued);
}

bool recruitable(const Player& player, const ThingTemplate& hero)
{
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->getControllingPlayer() != &player || !object->getProductionUpdateInterface())
			continue;
		const CommandSet* set = TheControlBar->findCommandSet(object->getCommandSetString());
		if (set && HeroRoster::isReviveTemplateInSet(set, &player, object, &hero))
			return true;
	}
	return false;
}

void handleRingChain(const RetailTestCommands::Context& ctx)
{
	const std::string_view action(ctx.action);
	if (action == "ringchain-eva")
	{
		UnsignedInt played = 0;
		std::fprintf(stderr, "TEST ringchain-eva frame=%u event=%s played=%d\n", ctx.frame, ctx.targetName,
			RetailEva::getLastPlayed(ctx.targetName, played) ? 1 : 0);
		return;
	}

	Player* player = ctx.findPlayer();
	if (!player)
		return;
	const Chain chain = findChain(player);
	if (action == "ringchain-state")
	{
		const UpgradeTemplate* ringUpgrade = TheUpgradeCenter->findUpgrade("Upgrade_RingHero");
		const UpgradeTemplate* fortressUpgrade = TheUpgradeCenter->findUpgrade("Upgrade_FortressRingHero");
		const Drawable* draw = chain.fortress ? chain.fortress->getDrawable() : nullptr;
		const Int flag = ModelConditionFlags::getSingleBitFromName("ONE_RING");
		std::fprintf(stderr,
			"TEST ringchain frame=%u rings=%d holder=%s upgrade=%d fortress-upgrade=%d fortress-flag=%d recruitable=%d hero=%d\n",
			ctx.frame, chain.rings, chain.holder ? chain.holder->getTemplate()->getName().str() : "-",
			player->hasUpgradeComplete(ringUpgrade), chain.fortress && chain.fortress->hasUpgrade(fortressUpgrade),
			draw && flag >= 0 && draw->getModelConditionFlags().test(flag),
			chain.heroType && recruitable(*player, *chain.heroType), chain.hero != nullptr);
		return;
	}

	if (action == "ringchain-gollum" && chain.fortress)
		spawn(ctx.targetName, ThePlayerList->findPlayerWithNameKey(NAMEKEY("PlyrCreeps")), openSpot(*chain.fortress, 450.0f));
	else if (action == "ringchain-kill-gollum")
	{
		for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
			if (!object->isEffectivelyDead() && object->getTemplate()->getName() == ctx.targetName)
				object->kill();
	}
	else if (action == "ringchain-carrier" && chain.ring)
	{
		Coord3D position = *chain.ring->getPosition();
		position.x += 40.0f;
		position.z = TheTerrainLogic->getGroundHeight(position.x, position.y);
		spawn(ctx.targetName, player, position);
	}
	else if (action == "ringchain-pickup" && chain.ring)
	{
		if (Object* carrier = ctx.requireObject(TRUE))
			order(ctx, *carrier, *chain.ring, false);
	}
	else if (action == "ringchain-deliver" && chain.fortress && chain.holder)
		order(ctx, *chain.holder, *chain.fortress, true);
	else if (action == "ringchain-kill-carrier" && chain.holder)
		chain.holder->kill();
	else if (action == "ringchain-recruit" && chain.heroType)
		recruit(ctx, *player, *chain.heroType);
	else if (action == "ringchain-kill-hero" && chain.hero)
		chain.hero->kill();
}
}
#endif

namespace RetailTestCommands
{
void registerRetailRingChainTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("ringchain-", handleRingChain);
#endif
}
}
