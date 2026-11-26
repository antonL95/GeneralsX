#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/AttributeModifierRules.h"
#include "Common/MessageStream.h"
#include "GameClient/InGameUI.h"
#include "GameClient/KeyDefs.h"
#include "GameLogic/Module/RotwkParseOnlyModules.h"
#include "GameLogic/Object.h"
#include <cstdio>

namespace
{
void selectUnit(const RetailTestCommands::Context& ctx)
{
	ctx.selectObject(ctx.requireObject(true));
}

void pressKey(GameMessage::Type type, Int key, Int state)
{
	GameMessage* message = TheMessageStream->appendMessage(type);
	message->appendIntegerArgument(key);
	message->appendIntegerArgument(state);
}

void attack(const RetailTestCommands::Context&)
{
	pressKey(GameMessage::MSG_RAW_KEY_DOWN, KEY_A, KEY_STATE_DOWN);
}

void aggressive(const RetailTestCommands::Context&)
{
	pressKey(GameMessage::MSG_RAW_KEY_DOWN, KEY_D, KEY_STATE_DOWN);
}

void battle(const RetailTestCommands::Context&)
{
	pressKey(GameMessage::MSG_RAW_KEY_DOWN, KEY_F, KEY_STATE_DOWN);
}

void holdGround(const RetailTestCommands::Context&)
{
	pressKey(GameMessage::MSG_RAW_KEY_DOWN, KEY_G, KEY_STATE_DOWN);
}

void waypoint(const RetailTestCommands::Context&)
{
	pressKey(GameMessage::MSG_RAW_KEY_DOWN, KEY_LALT, KEY_STATE_DOWN | KEY_STATE_LALT);
}

void immediate(const RetailTestCommands::Context&)
{
	pressKey(GameMessage::MSG_RAW_KEY_UP, KEY_LALT, KEY_STATE_UP);
}

void sell(const RetailTestCommands::Context&)
{
	pressKey(GameMessage::MSG_RAW_KEY_DOWN, KEY_DEL, KEY_STATE_DOWN);
}

void checkSale(const RetailTestCommands::Context& ctx)
{
	if (const Object* object = ctx.requireObject())
		std::fprintf(stderr, "TEST rotwk-hotkey-sale frame=%u sold=%d\n", ctx.frame,
			object->testStatus(OBJECT_STATUS_SOLD));
}

void check(const RetailTestCommands::Context& ctx)
{
	const auto* stances = StancesBehavior::find(ctx.requireObject(true));
	const char* stance = "missing";
	if (stances)
	{
		switch (stances->getStance())
		{
		case AttributeModifierRules::STANCE_AGGRESSIVE: stance = "aggressive"; break;
		case AttributeModifierRules::STANCE_BATTLE: stance = "battle"; break;
		case AttributeModifierRules::STANCE_HOLD_GROUND: stance = "hold-ground"; break;
		default: stance = "other"; break;
		}
	}
	std::fprintf(stderr, "TEST rotwk-hotkeys frame=%u attack-move=%d waypoint=%d stance=%s\n",
		ctx.frame, TheInGameUI->isInAttackMoveToMode(), TheInGameUI->isInWaypointMode(), stance);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailRotwkHotkeyTestCommands()
{
#if !RTS_GENERALS
	registerAction("rotwk-hotkey-select", selectUnit);
	registerAction("rotwk-hotkey-attack", attack);
	registerAction("rotwk-hotkey-aggressive", aggressive);
	registerAction("rotwk-hotkey-battle", battle);
	registerAction("rotwk-hotkey-hold-ground", holdGround);
	registerAction("rotwk-hotkey-waypoint", waypoint);
	registerAction("rotwk-hotkey-immediate", immediate);
	registerAction("rotwk-hotkey-sell", sell);
	registerAction("rotwk-hotkey-sold", checkSale);
	registerAction("rotwk-hotkey-check", check);
#endif
}
}
