#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include "Common/ActionManager.h"
#include "Common/MessageStream.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Mouse.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

#include <cstdio>

namespace
{
void handleArrowStorm(const RetailTestCommands::Context& ctx)
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
	const Coord3D position = ctx.hasPosition ? ctx.groundLocation() : *source->getPosition();
	if (ctx.action == "pkg29-arrow-probe")
	{
		ctx.selectObject(source);
		TheInGameUI->setGUICommand(button);
		const Bool valid = TheInGameUI->canSelectedObjectsDoSpecialPower(button, nullptr, &position,
			InGameUI::SELECTION_ANY, button->getOptions(), source);
		std::fprintf(stderr, "TEST pkg29 arrow cursor frame=%u armed=%d valid=%d cursor=%s\n",
			ctx.frame, TheInGameUI->getGUICommand() == button, valid,
			(valid ? button->getCursorName() : button->getInvalidCursorName()).str());
	}
	else if (ctx.action == "pkg29-arrow-cast")
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
	else if (ctx.action == "pkg29-arrow-script")
	{
		source->doCommandButtonAtPosition(button, &position, CMD_FROM_SCRIPT);
	}
	else if (ctx.action == "pkg29-arrow-state")
	{
		const SpecialPowerModuleInterface* module = source->getSpecialPowerModule(power);
		const Bool casting = source->getLogicModelConditions().test(MODELCONDITION_PREPARING);
		std::fprintf(stderr, "TEST pkg29 arrow frame=%u ready=%d accepted=%d firing=%d paralyzed=%d\n",
			ctx.frame, module && module->isReady(), module && module->wasLastCastAccepted(), casting,
			source->isDisabledByType(DISABLED_PARALYZED));
	}
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
	registerPrefix("pkg29-arrow-", handleArrowStorm);
#endif
}
}
