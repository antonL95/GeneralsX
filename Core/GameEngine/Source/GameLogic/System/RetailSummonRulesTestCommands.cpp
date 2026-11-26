#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>

#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Object.h"

namespace
{
void census(const RetailTestCommands::Context& ctx)
{
	const Player* player = ctx.findPlayer();
	const Real radius = std::strtof(ctx.argument, nullptr);
	if (!ctx.hasPosition || radius <= 0.0f)
		return;
	static const NameKeyType lifetimeKey = NAMEKEY("LifetimeUpdate");
	Int count = 0;
	Int timed = 0;
	Int rank = 0;
	Int contained = 0;
	Real health = 0.0f;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->getControllingPlayer() != player || object->isDestroyed() || object->isEffectivelyDead() ||
			object->getTemplate()->getName().compare(ctx.targetName) != 0)
			continue;
		const Coord3D* position = object->getPosition();
		const Real dx = position->x - ctx.position.x;
		const Real dy = position->y - ctx.position.y;
		if (dx * dx + dy * dy > radius * radius)
			continue;
		++count;
		if (object->findUpdateModule(lifetimeKey))
			++timed;
		if (const ExperienceTracker* tracker = object->getExperienceTracker())
			rank = std::max(rank, tracker->getRank());
		if (object->getContainedBy())
			++contained;
		if (const BodyModuleInterface* body = object->getBodyModule())
			health += body->getHealth();
	}
	std::fprintf(stderr, "TEST summon-rules census frame=%u object=%s count=%d contained=%d timed=%d rank=%d health=%.0f\n",
		ctx.frame, ctx.targetName, count, contained, timed, rank, health);
}

void cast(const RetailTestCommands::Context& ctx)
{
	Player* player = ctx.findPlayer();
	Object* book = player ? player->getRetailSpellBook() : nullptr;
	if (!book)
		return;
	book->setCommandSetStringOverride("SummonRulesSpellCommands");
	const CommandSet* set = TheRetailCommandCatalog->findCommandSet("SummonRulesSpellCommands");
	const Coord3D point = ctx.groundLocation();
	const CommandButton* button = set ? set->getCommandButton(ctx.ordinal) : nullptr;
	const Bool valid = button && button->canCastRetailSpell(book, nullptr, &point);
	const SpecialPowerModuleInterface* power = button ? book->getSpecialPowerModule(button->getSpecialPowerTemplate()) : nullptr;
	GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPELLBOOK_SPECIAL_POWER);
	message->friend_setPlayerIndex(player->getPlayerIndex());
	message->appendIntegerArgument(ctx.ordinal);
	message->appendLocationArgument(point);
	message->appendObjectIDArgument(INVALID_ID);
	std::fprintf(stderr, "TEST summon-rules cast frame=%u index=%d valid=%d ready=%.2f x=%.1f y=%.1f\n", ctx.frame, ctx.ordinal, valid, power ? power->getPercentReady() : -1.0f, point.x, point.y);
}

void handleSummonRules(const RetailTestCommands::Context& ctx)
{
	if (ctx.action == "summon-rules-census")
		census(ctx);
	else if (ctx.action == "summon-rules-cast")
		cast(ctx);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailSummonRulesTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("summon-rules-", handleSummonRules);
#endif
}
}
