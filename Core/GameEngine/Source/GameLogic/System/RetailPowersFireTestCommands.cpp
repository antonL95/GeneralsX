#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstring>

#include "Common/MessageStream.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

namespace
{
const CommandButton* findButton(const Object* object, const char* name)
{
	const CommandSet* set = TheControlBar->findCommandSet(object->getCommandSetString());
	for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
	{
		const CommandButton* button = set->getCommandButton(slot);
		if (button && button->getName().compare(name) == 0)
			return button;
	}
	return nullptr;
}

void powersFireCast(const RetailTestCommands::Context& ctx)
{
	Object* caster = ctx.findObject(true);
	const CommandButton* button = caster ? findButton(caster, ctx.argument) : nullptr;
	if (!button || !ctx.hasPosition)
	{
		std::fprintf(stderr, "TEST powers-fire-cast frame=%u button=%s issued=0\n", ctx.frame, ctx.argument);
		return;
	}
	TheInGameUI->deselectAllDrawables();
	ctx.selectObject(caster);
	const Coord3D location = ctx.groundLocation();
	const SpecialPowerTemplate* power = button->getSpecialPowerTemplate();
	if (button->getCommandType() == GUI_COMMAND_FIRE_WEAPON)
	{
		GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_WEAPON_AT_LOCATION);
		message->appendIntegerArgument(button->getWeaponSlot());
		message->appendLocationArgument(location);
		message->appendIntegerArgument(button->getMaxShotsToFire());
		message->appendObjectIDArgument(INVALID_ID);
	}
	else if (BitIsSet(button->getOptions(), NEED_TARGET_POS))
	{
		GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION);
		message->appendIntegerArgument(power->getID());
		message->appendLocationArgument(location);
		message->appendRealArgument(INVALID_ANGLE);
		message->appendObjectIDArgument(INVALID_ID);
		message->appendIntegerArgument(button->getOptions());
		message->appendObjectIDArgument(INVALID_ID);
	}
	else
	{
		GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER);
		message->appendIntegerArgument(power->getID());
		message->appendIntegerArgument(button->getOptions());
		message->appendObjectIDArgument(INVALID_ID);
	}
	std::fprintf(stderr, "TEST powers-fire-cast frame=%u button=%s issued=1\n", ctx.frame, ctx.argument);
}

void powersFireState(const RetailTestCommands::Context& ctx)
{
	Int alive = 0;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		if (!object->isEffectivelyDead() && object->getTemplate()->getName().compare(ctx.targetName) == 0)
			++alive;
	std::fprintf(stderr, "TEST powers-fire-state frame=%u template=%s alive=%d", ctx.frame, ctx.targetName, alive);
	const Object* subject = ctx.findObject(true);
	if (const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(ctx.argument))
		std::fprintf(stderr, " upgrade=%d", subject && subject->hasUpgrade(upgrade) ? 1 : 0);
	if (const SpecialPowerTemplate* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument))
	{
		const SpecialPowerModuleInterface* module = subject ? subject->getSpecialPowerModule(power) : nullptr;
		std::fprintf(stderr, " ready=%d", module && module->isReady() ? 1 : 0);
	}
	std::fputc('\n', stderr);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPowersFireTestCommands()
{
#if !RTS_GENERALS
	registerAction("powers-fire-cast", powersFireCast);
	registerAction("powers-fire-state", powersFireState);
#endif
}
}
