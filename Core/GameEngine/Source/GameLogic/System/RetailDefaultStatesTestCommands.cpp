#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/RetailDetectionModules.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

namespace
{
void probeDefaultState(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.findObject(TRUE);
	if (!object)
	{
		std::fprintf(stderr, "TEST default-states %u %d absent\n", ctx.frame, ctx.ordinal);
		return;
	}
	const auto& conditions = object->getLogicModelConditions();
	const AIUpdateInterface* ai = object->getAI();
	std::fprintf(stderr,
		"TEST default-states %u %d %s stealthed=%d camo=%d trees=%d selectable=%d idle=%d\n",
		ctx.frame, ctx.ordinal, object->getTemplate()->getName().str(), object->testStatus(OBJECT_STATUS_STEALTHED),
		conditions.test(MODELCONDITION_INVISIBLE_CAMOUFLAGE), RetailDetection::nearTrees(object),
		object->isSelectable(), ai && ai->isIdle());
}

void castDefaultPower(const RetailTestCommands::Context& ctx)
{
	Player* player = ctx.findPlayer();
	const CommandButton* button = TheRetailCommandCatalog->findCommandButton(ctx.argument);
	if (!player || !button || !button->getSpecialPowerTemplate())
		return;
	const Object* source = player->getRetailSpellBook();
	if (!source)
		return;
	const SpecialPowerTemplate* power = button->getSpecialPowerTemplate();
	const SpecialPowerModuleInterface* module = source->getSpecialPowerModule(power);
	std::fprintf(stderr, "TEST default-cast %u %s ready=%d\n", ctx.frame, ctx.argument, module && module->isReady());
	GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION);
	message->friend_setPlayerIndex(player->getPlayerIndex());
	message->appendIntegerArgument(power->getID());
	message->appendLocationArgument(ctx.groundLocation());
	message->appendRealArgument(INVALID_ANGLE);
	message->appendObjectIDArgument(INVALID_ID);
	message->appendIntegerArgument(button->getOptions());
	message->appendObjectIDArgument(source->getID());
}

void orderDefaultMove(const RetailTestCommands::Context& ctx)
{
	Player* player = ctx.findPlayer();
	const Object* object = ctx.findObject(TRUE);
	if (!player || !object)
		return;
	std::fprintf(stderr, "TEST default-order %u selectable=%d\n", ctx.frame, object->isSelectable());
	GameMessage* select = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
	select->friend_setPlayerIndex(player->getPlayerIndex());
	select->appendBooleanArgument(TRUE);
	select->appendObjectIDArgument(object->getID());
	GameMessage* move = TheMessageStream->appendMessage(GameMessage::MSG_DO_MOVETO);
	move->friend_setPlayerIndex(player->getPlayerIndex());
	move->appendLocationArgument(ctx.groundLocation());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailDefaultStatesTestCommands()
{
#if !RTS_GENERALS
	registerAction("default-state", probeDefaultState);
	registerAction("default-cast", castDefaultPower);
	registerAction("default-order", orderDefaultMove);
#endif
}
}
