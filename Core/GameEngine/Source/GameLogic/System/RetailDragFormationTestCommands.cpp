#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>

#include "Common/MessageStream.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Object.h"

namespace
{
void dragFormation(const RetailTestCommands::Context& ctx)
{
	GameMessage* move = TheMessageStream->appendMessage(GameMessage::MSG_DO_MOVETO_FORMATION);
	move->appendLocationArgument(ctx.groundLocation());
	move->appendRealArgument(ctx.angle);
	move->appendIntegerArgument(std::atoi(ctx.argument));
	move->appendBooleanArgument(FALSE);
}

void dragFormationState(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (!object)
		return;
	std::fprintf(stderr, "TEST drag-formation-state %u %s %d %.1f %.1f %.2f\n", ctx.frame,
		object->getTemplate()->getName().str(), ctx.ordinal, object->getPosition()->x, object->getPosition()->y,
		object->getOrientation());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailDragFormationTestCommands()
{
#if !RTS_GENERALS
	registerAction("drag-formation", dragFormation);
	registerAction("drag-formation-state", dragFormationState);
#endif
}
}
