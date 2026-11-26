#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ActionManager.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/CommandButtonRules.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/GrabPassengerSpecialPower.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"

namespace
{
void checkDismount(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject();
	const CommandButton* button = TheRetailCommandCatalog->findCommandButton(ctx.argument);
	if (!source || !button || !button->getSpecialPowerTemplate())
		return;
	const SpecialPowerTemplate* power = button->getSpecialPowerTemplate();
	const Coord3D point = ctx.groundLocation();
	const Int shroud = static_cast<Int>(ThePartitionManager->getShroudStatusForPlayer(
		source->getControllingPlayer()->getPlayerIndex(), &point));
	const Bool canDo = TheActionManager->canDoSpecialPowerAtLocation(
		source, &point, CMD_FROM_PLAYER, power, nullptr, button->getOptions());
	const Bool validTarget = power->isValidRetailTarget(source, nullptr, &point, CMD_FROM_PLAYER);
	std::fprintf(stderr, "TEST pt-dismount-check %u %s x=%.0f y=%.0f shroud=%d canDo=%d validTarget=%d\n",
		ctx.frame, source->getTemplate()->getName().str(), point.x, point.y, shroud, canDo ? 1 : 0, validTarget ? 1 : 0);
}

void castDismount(const RetailTestCommands::Context& ctx)
{
	Player* player = ctx.findPlayer();
	const Object* source = ctx.requireObject();
	const CommandButton* button = TheRetailCommandCatalog->findCommandButton(ctx.argument);
	if (!player || !source || !button || !button->getSpecialPowerTemplate())
		return;
	const SpecialPowerTemplate* power = button->getSpecialPowerTemplate();
	GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION);
	message->friend_setPlayerIndex(player->getPlayerIndex());
	message->appendIntegerArgument(power->getID());
	message->appendLocationArgument(ctx.groundLocation());
	message->appendRealArgument(INVALID_ANGLE);
	message->appendObjectIDArgument(INVALID_ID);
	message->appendIntegerArgument(button->getOptions());
	message->appendObjectIDArgument(source->getID());
}

void reportDismountState(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.findObject(TRUE);
	if (!object)
	{
		std::fprintf(stderr, "TEST pt-dismount-state %u %s absent\n", ctx.frame, ctx.targetName);
		return;
	}
	std::fprintf(stderr, "TEST pt-dismount-state %u %s present cmdset=%s\n",
		ctx.frame, object->getTemplate()->getName().str(), object->getCommandSetString().str());
}

void toggleWeapon(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;
	const CommandSet* set = TheRetailCommandCatalog->findCommandSet(source->getCommandSetString());
	for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
	{
		const CommandButton* button = set->getCommandButton(slot);
		if (button && button->getCommandType() == GUI_COMMAND_TOGGLE_WEAPONSET &&
			button->getName().compareNoCase(ctx.argument) == 0)
		{
			ctx.selectObject(source);
			GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_RETAIL_HORDE_COMMAND);
			message->appendObjectIDArgument(source->getID());
			message->appendIntegerArgument(slot);
			message->appendIntegerArgument(-1);
			return;
		}
	}
}

void attemptGrab(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	Object* target = ctx.findTarget(TRUE);
	if (!source || !target)
		return;
	const char* powerName = ctx.position.x > 0.5f ? "SpecialAbilityGrabPassenger2" : "SpecialAbilityGrabPassenger";
	const SpecialPowerTemplate* power = TheSpecialPowerStore->findSpecialPowerTemplate(powerName);
	auto* grab = power ? dynamic_cast<GrabPassengerSpecialPower*>(source->getSpecialPowerModule(power)) : nullptr;
	if (!grab)
		return;
	constexpr UnsignedInt options = NEED_TARGET_NEUTRAL_OBJECT | NEED_TARGET_ENEMY_OBJECT;
	const Bool toggle = source->testWeaponSetFlag(WEAPONSET_TOGGLE_1);
	const Bool valid = grab->isValidPowerTarget(target);
	const Bool accepted = grab->requestGrab(target, options);
	std::fprintf(stderr, "TEST pt-grab %u %s toggle=%d valid=%d accepted=%d\n",
		ctx.frame, source->getTemplate()->getName().str(), toggle ? 1 : 0, valid ? 1 : 0, accepted ? 1 : 0);
}

void reportHeld(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject();
	if (!source)
		return;
	const ContainModuleInterface* contain = source->getContain();
	const UnsignedInt count = contain ? contain->getContainCount() : 0;
	std::fprintf(stderr, "TEST pt-held %u %s count=%u\n", ctx.frame, source->getTemplate()->getName().str(), count);
}

void checkTrollButton(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject();
	const CommandButton* button = TheRetailCommandCatalog->findCommandButton(ctx.argument);
	if (!source || !button || !button->getSpecialPowerTemplate())
		return;
	const ContainModuleInterface* contain = source->getContain();
	const UnsignedInt count = contain ? contain->getContainCount() : 0;
	const Bool ws1 = source->getLogicModelConditions().test(MODELCONDITION_WEAPONSTATE_ONE);
	const Bool modelAllowed = button->isModelConditionAllowed(source);
	const Bool containerAllowed = button->isValidContainerForCommand(source);
	const Bool canDo = TheActionManager->canDoSpecialPower(source, button->getSpecialPowerTemplate(), CMD_FROM_PLAYER, button->getOptions());
	std::fprintf(stderr, "TEST pt-troll-check %u %s count=%u ws1=%d modelAllowed=%d containerAllowed=%d canDo=%d\n",
		ctx.frame, source->getTemplate()->getName().str(), count, ws1 ? 1 : 0,
		modelAllowed ? 1 : 0, containerAllowed ? 1 : 0, canDo ? 1 : 0);
}

void addContained(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	Object* target = ctx.findTarget(TRUE);
	if (!source || !target || !source->getContain())
		return;
	source->getContain()->addToContain(target);
}

void removeContained(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	Object* target = ctx.findTarget(TRUE);
	if (!source || !target || !source->getContain())
		return;
	source->getContain()->removeFromContain(target);
}

void reportCarrierState(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject();
	if (!source)
		return;
	const ContainModuleInterface* contain = source->getContain();
	const UnsignedInt count = contain ? contain->getContainCount() : 0;
	const auto& flags = source->getLogicModelConditions();
	std::fprintf(stderr, "TEST pt-carrier-state %u %s count=%u ws1=%d ws2=%d\n",
		ctx.frame, source->getTemplate()->getName().str(), count,
		flags.test(MODELCONDITION_WEAPONSTATE_ONE) ? 1 : 0,
		flags.test(MODELCONDITION_WEAPONSTATE_TWO) ? 1 : 0);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPowerTargetsTestCommands()
{
#if !RTS_GENERALS
	registerAction("pt-dismount-check", checkDismount);
	registerAction("pt-dismount-cast", castDismount);
	registerAction("pt-dismount-state", reportDismountState);
	registerAction("pt-toggle", toggleWeapon);
	registerAction("pt-grab", attemptGrab);
	registerAction("pt-held", reportHeld);
	registerAction("pt-troll-check", checkTrollButton);
	registerAction("pt-contain-add", addContained);
	registerAction("pt-contain-remove", removeContained);
	registerAction("pt-carrier-state", reportCarrierState);
#endif
}
}
