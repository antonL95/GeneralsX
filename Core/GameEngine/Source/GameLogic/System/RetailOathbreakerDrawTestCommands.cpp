#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/GameType.h"
#include "Common/MessageStream.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"

namespace
{
void castOathbreakerPower(const RetailTestCommands::Context& ctx)
{
	Object* caster = ctx.requireObject(true);
	const CommandButton* button = TheControlBar ? TheControlBar->findCommandButton(ctx.argument) : nullptr;
	if (!caster || !button || !button->getSpecialPowerTemplate() || !ctx.hasPosition)
		return;

	ctx.selectObject(caster);
	const Coord3D point = ctx.groundLocation();
	GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION);
	message->appendIntegerArgument(button->getSpecialPowerTemplate()->getID());
	message->appendLocationArgument(point);
	message->appendRealArgument(INVALID_ANGLE);
	message->appendObjectIDArgument(INVALID_ID);
	message->appendIntegerArgument(button->getOptions());
	message->appendObjectIDArgument(INVALID_ID);
	std::fprintf(stderr, "TEST frame %u: cast %s at point (%.1f, %.1f)\n", ctx.frame,
		button->getName().str(), point.x, point.y);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailOathbreakerDrawTestCommands()
{
#if !RTS_GENERALS
	registerAction("oathbreaker-cast", castOathbreakerPower);
#endif
}
}
