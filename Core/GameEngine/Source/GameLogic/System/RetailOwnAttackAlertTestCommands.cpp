#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include "Common/MessageStream.h"
#include "Common/GameType.h"

#include "GameClient/InGameUI.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"

namespace
{
void castFire(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject(true);
	if (!source || !ctx.hasPosition)
		return;
	TheInGameUI->deselectAllDrawables();
	ctx.selectObject(source);
	GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_WEAPON_AT_LOCATION);
	message->appendIntegerArgument(PRIMARY_WEAPON);
	message->appendLocationArgument(ctx.groundLocation());
	message->appendIntegerArgument(1);
	message->appendObjectIDArgument(INVALID_ID);
	std::fprintf(stderr, "TEST own-attack-cast frame=%u\n", ctx.frame);
}

void probe(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	if (!object || !object->getBodyModule())
		return;
	std::fprintf(stderr, "TEST own-attack-probe frame=%u case=%s health=%.1f wet=%d\n",
		ctx.frame, ctx.argument, object->getBodyModule()->getHealth(), object->testStatus(OBJECT_STATUS_WET));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailOwnAttackAlertTestCommands()
{
#if !RTS_GENERALS
	registerAction("own-attack-cast", castFire);
	registerAction("own-attack-probe", probe);
#endif
}
}
