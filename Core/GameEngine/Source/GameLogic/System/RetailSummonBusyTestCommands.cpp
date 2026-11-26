#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/MessageStream.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/LifetimeUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void traceSummon(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.findObject(false);
	if (!object)
		return;
	const auto* lifetime = static_cast<const LifetimeUpdate*>(object->findUpdateModule(NAMEKEY("LifetimeUpdate")));
	if (!lifetime)
		return;
	if (ctx.action == "summon-busy-egg")
	{
		std::fprintf(stderr, "TEST summon-busy egg %s death=%u\n", ctx.argument, lifetime->getDieFrame());
		return;
	}
	const auto* ai = object->getAI();
	if (!ai || !lifetime->getLifetimeFrames())
		return;
	const UnsignedInt birth = lifetime->getDieFrame() - lifetime->getLifetimeFrames();
	const UnsignedInt age = ctx.frame - birth;
	if (age != 1 && age != 6 && age != 24 && age != 70 && age != 75)
		return;
	const Coord3D& position = *object->getPosition();
	std::fprintf(stderr, "TEST summon-busy sample %s frame=%u birth=%u age=%u busy=%d idle=%d x=%.3f y=%.3f\n",
		ctx.argument, ctx.frame, birth, age, ai->isBusy(), ai->isIdle(), position.x, position.y);
	if (age == 1 && ctx.hasPosition && object->isSelectable())
	{
		ctx.selectObject(object);
		GameMessage* move = TheMessageStream->appendMessage(GameMessage::MSG_DO_MOVETO);
		move->appendLocationArgument(ctx.groundLocation());
		std::fprintf(stderr, "TEST summon-busy move %s frame=%u\n", ctx.argument, ctx.frame);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailSummonBusyTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("summon-busy-", traceSummon);
#endif
}
}
