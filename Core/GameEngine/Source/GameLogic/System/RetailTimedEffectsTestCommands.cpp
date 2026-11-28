#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ActionManager.h"
#include "Common/MessageStream.h"
#include "Common/ModelState.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/CommandXlat.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
const CommandButton* findCommandButton(const Object* object, const char* name)
{
	if (!object || !TheControlBar)
		return nullptr;
	const CommandSet* set = TheControlBar->findCommandSet(object->getCommandSetString());
	for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
	{
		const CommandButton* button = set->getCommandButton(slot);
		if (button && button->getName().compare(name) == 0)
			return button;
	}
	return nullptr;
}

void logObjectState(const char* role, const Object* object, UnsignedInt frame, const char* arg)
{
	AsciiString lists;
	AttributeModifiers::describe(object, lists);
	const AIUpdateInterface* ai = object->getAI();
	const Locomotor* loco = ai ? ai->getCurLocomotor() : nullptr;
	const Drawable* draw = object->getDrawable();
	const Int dissidentLogic = object->getLogicModelConditions().test(MODELCONDITION_EMOTION_DISSIDENT) ? 1 : 0;
	const Int dissidentDraw = draw && draw->getModelConditionFlags().test(MODELCONDITION_EMOTION_DISSIDENT) ? 1 : 0;
	Int customCond = -1;
	if (arg && *arg && std::strcmp(arg, "-") != 0)
	{
		const Int bit = ModelConditionFlags::getSingleBitFromName(arg);
		if (bit >= 0)
			customCond = object->getLogicModelConditions().test(bit) ? 1 : 0;
	}
	const UpgradeTemplate* upgrade = arg && *arg && TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(arg) : nullptr;
	const Int hasUpgrade = upgrade ? (object->hasUpgrade(upgrade) ? 1 : 0) : -1;
	const char* owner = object->getControllingPlayer() ? object->getControllingPlayer()->getPlayerName().str() : "-";
	std::fprintf(stderr,
		"TIMED probe frame=%u role=%s owner=%s template=%s id=%u damage=%.3f armor=%.3f speedMult=%.3f locoSpeed=%.2f dissident=%d/%d cond=%d upgrade=%d disabled=%d using=%d lists=[%s]\n",
		frame, role, owner, object->getTemplate()->getName().str(), static_cast<UnsignedInt>(object->getID()),
		AttributeModifiers::damageDealtFactor(object), AttributeModifiers::damageTakenFactor(object, DAMAGE_SLASH),
		AttributeModifiers::speedFactor(object),
		loco ? loco->getMaxSpeedForCondition(BODY_PRISTINE, object) : 0.0f,
		dissidentLogic, dissidentDraw, customCond, hasUpgrade,
		object->isDisabled() ? 1 : 0,
		object->testStatus(OBJECT_STATUS_IS_USING_ABILITY) ? 1 : 0,
		lists.str());
}

void handleTimedEffects(const RetailTestCommands::Context& ctx)
{
	if (ctx.action == "timed-count")
	{
		const Player* player = ctx.playerName == "-" ? nullptr : ctx.findPlayer();
		Int alive = 0;
		Int total = 0;
		for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		{
			if (player && object->getControllingPlayer() != player)
				continue;
			if (object->getTemplate()->getName().compare(ctx.argument) != 0)
				continue;
			++total;
			if (!object->isEffectivelyDead())
				++alive;
		}
		const char* prefix = ctx.frame >= 244 ? "TIMED" : "TIMED_PRE";
		std::fprintf(stderr, "%s count frame=%u template=%s alive=%d total=%d\n",
			prefix, ctx.frame, ctx.argument, alive, total);
		return;
	}

	Object* source = ctx.requireObject(true);
	if (!source)
		return;

	if (ctx.action == "timed-cast")
	{
		const char* prefix = ctx.frame >= 244 ? "TIMED" : "TIMED_PRE";
		const CommandButton* button = findCommandButton(source, ctx.argument);
		const SpecialPowerTemplate* power = button ? button->getSpecialPowerTemplate() : nullptr;
		const SpecialPowerModuleInterface* spm = power ? source->getSpecialPowerModule(power) : nullptr;
		if (!button || !power)
		{
			std::fprintf(stderr, "%s cast frame=%u template=%s button=%s issued=0 reason=no_button\n",
				prefix, ctx.frame, source->getTemplate()->getName().str(), ctx.argument);
			return;
		}
		TheInGameUI->deselectAllDrawables();
		ctx.selectObject(source);
		const Bool ready = spm && spm->isReady();
		Bool canDo = FALSE;
		Bool accepted = FALSE;
		Int shroud = -1;
		Int water = -1;
		if (BitIsSet(button->getOptions(), NEED_TARGET_POS))
		{
			const Coord3D location = ctx.groundLocation();
			shroud = ThePartitionManager && source->getControllingPlayer()
				? static_cast<Int>(ThePartitionManager->getShroudStatusForPlayer(source->getControllingPlayer()->getPlayerIndex(), &location))
				: -1;
			water = TheTerrainLogic && TheTerrainLogic->isUnderwater(location.x, location.y) ? 1 : 0;
			canDo = TheActionManager->canDoSpecialPowerAtLocation(source, &location, CMD_FROM_PLAYER, power, nullptr, button->getOptions());
			TheInGameUI->setGUICommand(button);
			CommandTranslator translator;
			accepted = translator.evaluateContextCommand(nullptr, &location, CommandTranslator::DO_COMMAND) ==
				GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION;
			TheInGameUI->setGUICommand(nullptr);
		}
		else if (BitIsSet(button->getOptions(), COMMAND_OPTION_NEED_OBJECT_TARGET))
		{
			Object* target = ctx.findTarget(true);
			canDo = target && TheActionManager->canDoSpecialPowerAtObject(source, target, CMD_FROM_PLAYER, power, button->getOptions());
			TheInGameUI->setGUICommand(button);
			CommandTranslator translator;
			accepted = target && translator.evaluateContextCommand(target->getDrawable(), target->getPosition(), CommandTranslator::DO_COMMAND) ==
				GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT;
			TheInGameUI->setGUICommand(nullptr);
		}
		else
		{
			canDo = TheActionManager->canDoSpecialPower(source, power, CMD_FROM_PLAYER, button->getOptions());
			GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER);
			msg->appendIntegerArgument(power->getID());
			msg->appendIntegerArgument(button->getOptions());
			msg->appendObjectIDArgument(INVALID_ID);
			accepted = TRUE;
		}
		std::fprintf(stderr, "%s cast frame=%u template=%s button=%s issued=1 ready=%d canDo=%d accepted=%d shroud=%d water=%d draw=%d\n",
			prefix, ctx.frame, source->getTemplate()->getName().str(), ctx.argument,
			ready ? 1 : 0, canDo ? 1 : 0, accepted ? 1 : 0, shroud, water, source->getDrawable() ? 1 : 0);
		return;
	}

	if (ctx.action == "timed-probe")
	{
		logObjectState("object", source, ctx.frame, ctx.argument);
		const ContainModuleInterface* contain = HordeContain::get(source) ? source->getContain() : nullptr;
		const ContainedItemsList* members = contain ? contain->getContainedItemsList() : nullptr;
		if (members && !members->empty())
			logObjectState("member", members->front(), ctx.frame, ctx.argument);
		return;
	}

	if (ctx.action == "timed-disable")
	{
		const UnsignedInt duration = static_cast<UnsignedInt>(std::atoi(ctx.argument));
		source->setDisabledUntil(DISABLED_PARALYZED, ctx.frame + duration);
		std::fprintf(stderr, "TIMED disable frame=%u template=%s until=%u\n",
			ctx.frame, source->getTemplate()->getName().str(), ctx.frame + duration);
		return;
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailTimedEffectsTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("timed-", handleTimedEffects);
#endif
}
}
