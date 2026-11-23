#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include "Common/ActionManager.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingTemplate.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Mouse.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/SpecialPowerUpdateModule.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

#include <cstdio>

namespace
{
void handleLocationAbility(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;
	if (ctx.action == "pkg29-arrow-click")
	{
		const MouseIO* mouse = TheMouse->getMouseStatus();
		for (const auto type : {GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN, GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP})
		{
			GameMessage* message = TheMessageStream->appendMessage(type);
			message->appendPixelArgument(mouse->pos);
			message->appendIntegerArgument(0);
			message->appendIntegerArgument(mouse->time);
		}
		return;
	}
	if (ctx.action == "pkg29-arrow-health")
	{
		std::fprintf(stderr, "TEST pkg29 arrow health frame=%u owner=%.*s hp=%.3f\n", ctx.frame,
			static_cast<Int>(ctx.playerName.size()), ctx.playerName.data(), source->getBodyModule()->getHealth());
		return;
	}
	if (ctx.action == "pkg29-arrow-bow")
	{
		source->setWeaponSetFlag(WEAPONSET_TOGGLE_1);
		return;
	}
	const CommandButton* button = TheRetailCommandCatalog->findCommandButton(ctx.argument);
	const SpecialPowerTemplate* power = button ? button->getSpecialPowerTemplate() : nullptr;
	if (!power)
		return;
	const Bool poultry = ctx.action.starts_with("pkg29-poultry-");
	const Coord3D position = ctx.hasPosition ? ctx.groundLocation() : *source->getPosition();
	if (ctx.action.ends_with("-probe"))
	{
		ctx.selectObject(source);
		TheInGameUI->setGUICommand(button);
		const Bool valid = TheInGameUI->canSelectedObjectsDoSpecialPower(button, nullptr, &position,
			InGameUI::SELECTION_ANY, button->getOptions(), source);
		std::fprintf(stderr, "TEST pkg29 %s cursor frame=%u armed=%d valid=%d cursor=%s\n",
			poultry ? "poultry" : "arrow", ctx.frame, TheInGameUI->getGUICommand() == button, valid,
			(valid ? button->getCursorName() : button->getInvalidCursorName()).str());
	}
	else if (ctx.action.ends_with("-cast"))
	{
		ctx.selectObject(source);
		GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION);
		message->appendIntegerArgument(power->getID());
		message->appendLocationArgument(position);
		message->appendRealArgument(INVALID_ANGLE);
		message->appendObjectIDArgument(INVALID_ID);
		message->appendIntegerArgument(button->getOptions());
		message->appendObjectIDArgument(source->getID());
	}
	else if (ctx.action.ends_with("-script"))
	{
		source->doCommandButtonAtPosition(button, &position, CMD_FROM_SCRIPT);
	}
	else if (ctx.action == "pkg29-poultry-ai")
	{
		const UnsignedInt options = button->getOptions() | COMMAND_OPTION_INTERNAL_AUTO;
		if (TheActionManager->canDoSpecialPowerAtLocation(source, &position, CMD_FROM_AI, power, nullptr, options))
			source->doSpecialPowerAtLocation(power, &position, INVALID_ANGLE, options);
	}
	else if (ctx.action.ends_with("-state"))
	{
		const SpecialPowerModuleInterface* module = source->getSpecialPowerModule(power);
		const Bool casting = source->getLogicModelConditions().test(MODELCONDITION_PREPARING);
		if (poultry)
		{
			Bool active = FALSE;
			for (BehaviorModule** behavior = source->getBehaviorModules(); *behavior; ++behavior)
			{
				const auto* update = (*behavior)->getSpecialPowerUpdateInterface();
				active = active || (update && update->isActive());
			}
			std::fprintf(stderr, "TEST pkg29 poultry frame=%u ready=%d accepted=%d active=%d\n",
				ctx.frame, module && module->isReady(), module && module->wasLastCastAccepted(), active);
		}
		else
			std::fprintf(stderr, "TEST pkg29 arrow frame=%u ready=%d accepted=%d firing=%d paralyzed=%d\n",
				ctx.frame, module && module->isReady(), module && module->wasLastCastAccepted(), casting,
				source->isDisabledByType(DISABLED_PARALYZED));
	}
}

void handlePoultry(const RetailTestCommands::Context& ctx)
{
	if (ctx.action != "pkg29-poultry-count")
	{
		handleLocationAbility(ctx);
		return;
	}
	Int living = 0;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->isEffectivelyDead() || object->isDestroyed())
			continue;
		const std::string_view name = object->getTemplate()->getName().str();
		if (name != ctx.argument && (std::string_view(ctx.argument) != "animals" || !name.ends_with("_Replacement")))
			continue;
		const Player* player = object->getControllingPlayer();
		if (ctx.playerName != "*" && player != ctx.findPlayer())
			continue;
		++living;
		std::fprintf(stderr, "TEST pkg29 poultry object frame=%u name=%s neutral=%d x=%.1f y=%.1f angle=%.3f hp=%.1f/%.1f xp=%.1f upgrades=%d\n",
			ctx.frame, name.data(), player == ThePlayerList->getNeutralPlayer(),
			object->getPosition()->x, object->getPosition()->y, object->getOrientation(), object->getBodyModule()->getHealth(),
			object->getBodyModule()->getMaxHealth(), object->getExperienceTracker()->getRetailExperience(),
			object->getObjectCompletedUpgradeMask().any());
	}
	std::fprintf(stderr, "TEST pkg29 poultry count frame=%u object=%s player=%.*s living=%d\n",
		ctx.frame, ctx.argument, static_cast<Int>(ctx.playerName.size()), ctx.playerName.data(), living);
}

void handlePackage29(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;

	const CommandButton* button = TheRetailCommandCatalog->findCommandButton("Command_Repair");
	const SpecialPowerTemplate* power = button ? button->getSpecialPowerTemplate() : nullptr;
	if (!power)
		return;

	Object* target = ctx.findTarget(ctx.argument, TRUE);
	const SpecialPowerModuleInterface* module = source->getSpecialPowerModule(power);
	if (ctx.action == "pkg29-state")
	{
		AIUpdateInterface* ai = source->getAI();
		DozerAIInterface* dozer = ai ? ai->getDozerAIInterface() : nullptr;
		const BodyModuleInterface* body = target ? target->getBodyModule() : nullptr;
		const Bool repair = dozer && dozer->isTaskPending(DOZER_TASK_REPAIR);
		std::fprintf(stderr, "TEST pkg29 repair frame=%u module=%d ready=%d accepted=%d task=%d repairing=%d health=%.3f\n",
			ctx.frame, module != nullptr, module && module->isReady(), module && module->wasLastCastAccepted(),
			repair, target && target->getSoleHealingBenefactor() == source->getID(), body ? body->getHealth() : 0.0f);
		return;
	}
	if (!target)
		return;

	if (ctx.action == "pkg29-probe")
	{
		ctx.selectObject(source);
		const Bool valid = TheInGameUI->canSelectedObjectsDoSpecialPower(button, target, target->getPosition(),
			InGameUI::SELECTION_ANY, button->getOptions(), source);
		std::fprintf(stderr, "TEST pkg29 cursor frame=%u valid=%d cursor=%s generic=%d\n", ctx.frame, valid,
			(valid ? button->getCursorName() : button->getInvalidCursorName()).str(),
			TheActionManager->canRepairObject(source, target, CMD_FROM_PLAYER));
	}
	else if (ctx.action == "pkg29-cast")
	{
		std::fprintf(stderr, "TEST pkg29 cast frame=%u armed=%d\n", ctx.frame, TheInGameUI->getGUICommand() == button);
		ctx.selectObject(source);
		GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT);
		message->appendIntegerArgument(power->getID());
		message->appendObjectIDArgument(target->getID());
		message->appendIntegerArgument(button->getOptions());
		message->appendObjectIDArgument(source->getID());
	}
	else if (ctx.action == "pkg29-script")
	{
		source->doCommandButtonAtObject(button, target, CMD_FROM_SCRIPT);
	}
	else if (ctx.action == "pkg29-ai")
	{
		const UnsignedInt options = button->getOptions() | COMMAND_OPTION_INTERNAL_AUTO;
		if (TheActionManager->canDoSpecialPowerAtObject(source, target, CMD_FROM_AI, power, options))
			source->doSpecialPowerAtObject(power, target, options);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage29TestCommands()
{
#if RTS_ZEROHOUR
	registerPrefix("pkg29-", handlePackage29);
	registerPrefix("pkg29-arrow-", handleLocationAbility);
	registerPrefix("pkg29-poultry-", handlePoultry);
#endif
}
}
