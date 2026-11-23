#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/MessageStream.h"
#include "Common/SpecialPower.h"
#include "GameClient/CommandXlat.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

namespace
{
void castHall(const RetailTestCommands::Context& ctx)
{
	const Object* book = ctx.requireObject();
	const CommandButton* button = TheRetailCommandCatalog->findCommandButton("Command_SpellBookHallOfFire");
	if (!book || !button || !ctx.hasPosition)
		return;
	GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION);
	message->appendIntegerArgument(button->getSpecialPowerTemplate()->getID());
	message->appendLocationArgument(ctx.groundLocation());
	message->appendRealArgument(INVALID_ANGLE);
	message->appendObjectIDArgument(INVALID_ID);
	message->appendIntegerArgument(button->getOptions());
	message->appendObjectIDArgument(book->getID());
}

void castSummon(const RetailTestCommands::Context& ctx)
{
	Object* hall = ctx.requireObject();
	const CommandButton* button = TheRetailCommandCatalog->findCommandButton(ctx.argument);
	if (!hall || !button || !ctx.hasPosition)
		return;
	ctx.selectObject(hall);
	TheInGameUI->setGUICommand(button);
	CommandTranslator translator;
	const Coord3D point = ctx.groundLocation();
	const Bool accepted = translator.evaluateContextCommand(nullptr, &point, CommandTranslator::DO_COMMAND) ==
		GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION;
	TheInGameUI->setGUICommand(nullptr);
	std::fprintf(stderr, "TEST hall-cast %u %s %d\n", ctx.frame, ctx.argument, accepted);
}

void probeHallObject(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.findObject(true);
	if (!object)
	{
		std::fprintf(stderr, "TEST hall-object %u %s absent\n", ctx.frame, ctx.argument);
		return;
	}
	const Coord3D& pos = *object->getPosition();
	const PhysicsBehavior* physics = object->getPhysics();
	const Drawable* draw = object->getDrawable();
	std::fprintf(stderr, "TEST hall-object %u %s %.2f %.2f %.2f %.3f %d\n",
		ctx.frame, ctx.argument, pos.x - ctx.position.x, pos.y - ctx.position.y,
		object->getHeightAboveTerrain(), physics ? physics->getVelocity()->z : 0.0f,
		draw && draw->getModelConditionFlags().test(MODELCONDITION_WEAPONSET_PLAYER_UPGRADE));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailHallOfFireTestCommands()
{
#if !RTS_GENERALS
	registerAction("hall-cast", castHall);
	registerAction("hall-summon", castSummon);
	registerAction("hall-probe", probeHallObject);
#endif
}
}
