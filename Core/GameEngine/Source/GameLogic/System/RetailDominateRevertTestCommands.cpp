#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

namespace
{
void cast(const RetailTestCommands::Context& ctx)
{
	Object* caster = ctx.requireObject(true);
	Object* victim = ctx.target;
	const auto* power = TheSpecialPowerStore ? TheSpecialPowerStore->findSpecialPowerTemplate("DominateRevertTestPower") : nullptr;
	if (!caster || !victim || !power)
		return;
	const auto* button = TheRetailCommandCatalog ? TheRetailCommandCatalog->findPowerButton(caster, power) : nullptr;
	GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT);
	msg->appendIntegerArgument(power->getID());
	msg->appendObjectIDArgument(victim->getID());
	msg->appendIntegerArgument(button ? button->getOptions() : 0);
	msg->appendObjectIDArgument(caster->getID());
}

void remove(const RetailTestCommands::Context& ctx)
{
	if (Object* caster = ctx.requireObject(true))
		TheGameLogic->destroyObject(caster);
}

void probe(const RetailTestCommands::Context& ctx)
{
	const Player* owner = nullptr;
	const char* state = "absent";
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->isDestroyed() || object->getTemplate()->getName().compareNoCase(ctx.targetName) != 0)
			continue;
		owner = object->getControllingPlayer();
		state = object->isEffectivelyDead() ? "dead" : "alive";
		break;
	}
	const AsciiString ownerName = owner
		? TheNameKeyGenerator->keyToName(owner->getPlayerNameKey())
		: AsciiString("-");
	std::fprintf(stderr, "TEST dominate-revert frame=%u object=%s state=%s owner=%s\n",
		ctx.frame, ctx.targetName, state, ownerName.str());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailDominateRevertTestCommands()
{
#if !RTS_GENERALS
	registerAction("dominate-revert-cast", cast);
	registerAction("dominate-revert-remove", remove);
	registerAction("dominate-revert-probe", probe);
#endif
}
}
