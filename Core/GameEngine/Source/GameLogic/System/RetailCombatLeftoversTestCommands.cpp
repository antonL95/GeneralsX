#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/WeaponSetFlags.h"

namespace
{
const char* availabilityLabel(CommandAvailability availability)
{
	switch (availability)
	{
	case COMMAND_AVAILABLE: return "available";
	case COMMAND_ACTIVE: return "active";
	case COMMAND_HIDDEN: return "hidden";
	case COMMAND_NOT_READY: return "not_ready";
	case COMMAND_CANT_AFFORD: return "cant_afford";
	case COMMAND_RESTRICTED: return "restricted";
	case COMMAND_PASSIVE: return "passive";
	default: return "other";
	}
}

void countTemplateForPlayer(const RetailTestCommands::Context& ctx, const char* templateName, const char* label)
{
	const Player* player = ctx.findPlayer();
	Int count = 0;
	if (player)
	{
		for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		{
			if (obj->isDestroyed() || obj->isEffectivelyDead())
				continue;
			if (obj->getControllingPlayer() == player && obj->getTemplate() &&
				obj->getTemplate()->getName().compareNoCase(templateName) == 0)
				++count;
		}
	}
	std::fprintf(stderr, "TEST combat-leftovers count frame=%u label=%s player=%.*s template=%s count=%d\n",
		ctx.frame, label, static_cast<Int>(ctx.playerName.size()), ctx.playerName.data(), templateName, count);
}

void probeGothmog(const RetailTestCommands::Context& ctx)
{
	Object* gothmog = ctx.requireObject();
	if (!gothmog)
		return;
	const CommandButton* button = TheControlBar ? TheControlBar->findCommandButton("Command_SpecialAbilityGothmogOrcSummon") : nullptr;
	const SpecialPowerTemplate* power = TheSpecialPowerStore ? TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityGothmogSummon") : nullptr;
	const SpecialPowerModuleInterface* module = power ? gothmog->getSpecialPowerModule(power) : nullptr;
	const CommandAvailability availability = (button && TheControlBar)
		? TheControlBar->queryRetailCommandAvailability(button, gothmog)
		: COMMAND_HIDDEN;
	Int summoned = 0;
	for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (!obj->isDestroyed() && !obj->isEffectivelyDead() && obj->getTemplate() &&
			obj->getTemplate()->getName().compareNoCase("MordorMorgulOrcPikeHorde_Summoned") == 0)
			++summoned;
	}
	std::fprintf(stderr, "TEST combat-leftovers gothmog frame=%u label=%s mounted=%d avail=%s ready=%d summoned=%d\n",
		ctx.frame, ctx.argument, gothmog->testWeaponSetFlag(WEAPONSET_MOUNTED) ? 1 : 0,
		availabilityLabel(availability), module && module->isReady() ? 1 : 0, summoned);
}

void castPowerThroughMessage(const RetailTestCommands::Context& ctx)
{
	Object* caster = ctx.requireObject();
	if (!caster || !TheSpecialPowerStore || !TheMessageStream)
		return;
	const SpecialPowerTemplate* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
	if (!power)
		return;
	UnsignedInt options = COMMAND_OPTION_NONE;
	if (TheRetailCommandCatalog)
		if (const CommandButton* button = TheRetailCommandCatalog->findPowerButton(caster, power))
			options = button->getOptions();
	GameMessage* powerMsg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER);
	powerMsg->appendIntegerArgument(power->getID());
	powerMsg->appendIntegerArgument(options);
	powerMsg->appendObjectIDArgument(caster->getID());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailCombatLeftoversTestCommands()
{
#if !RTS_GENERALS
	registerAction("combat-leftovers-converted", [](const Context& ctx) {
		countTemplateForPlayer(ctx, "CombatLeftoverConverted", ctx.argument);
	});
	registerAction("combat-leftovers-gothmog", probeGothmog);
	registerAction("combat-leftovers-cast", castPowerThroughMessage);
#endif
}
}
