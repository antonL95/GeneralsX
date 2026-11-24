#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/MessageStream.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "WWMath/wwmath.h"

namespace
{
void tracePayload(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.findObject(true);
	if (!object)
	{
		std::fprintf(stderr, "TEST pkg64 frame=%u label=%s missing\n", ctx.frame, ctx.argument);
		return;
	}

	const Coord3D& position = *object->getPosition();
	const auto* ai = object->getAI();
	const auto* drawable = object->getDrawable();
	std::fprintf(stderr, "TEST pkg64 frame=%u label=%s x=%.3f y=%.3f angle=%.3f busy=%d moving=%d built=%d\n",
		ctx.frame, ctx.argument, position.x, position.y, object->getOrientation(),
		ai && ai->isBusy(), ai && ai->isMoving(),
		drawable && drawable->getModelConditionFlags().test(MODELCONDITION_JUST_BUILT));
}

void traceBarrage(const RetailTestCommands::Context& ctx)
{
	const Object* keep = ctx.findTarget(true);
	if (!keep || !ctx.hasPosition)
		return;
	const Coord3D& keepPosition = *keep->getPosition();
	Int count = 0;
	Int inSpread = 0;
	Int facedKeep = 0;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->isEffectivelyDead() || object->getTemplate()->getName() != "SpellBookFieryBarrageSeed")
			continue;

		++count;
		const Coord3D& pos = *object->getPosition();
		const Real spreadX = pos.x - ctx.position.x;
		const Real spreadY = pos.y - ctx.position.y;
		const Real distanceSquared = spreadX * spreadX + spreadY * spreadY;
		if (distanceSquared >= 9.9f * 9.9f && distanceSquared <= 150.1f * 150.1f)
			++inSpread;

		const Real expected = WWMath::Atan2Origin(keepPosition.y - pos.y, keepPosition.x - pos.x);
		if (WWMath::CosTrig(object->getOrientation() - expected) > 0.9999f)
			++facedKeep;
	}
	std::fprintf(stderr, "TEST pkg64 frame=%u label=barrage seeds=%d spread=%d facing=%d\n",
		ctx.frame, count, inSpread, facedKeep);
}

void castPayload(const RetailTestCommands::Context& ctx)
{
	const Object* caster = ctx.requireObject();
	const CommandButton* button = TheControlBar->findCommandButton(ctx.argument);
	const SpecialPowerTemplate* power = button ? button->getSpecialPowerTemplate() : nullptr;
	if (!caster || !power || !ctx.hasPosition)
		return;

	GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION);
	message->appendIntegerArgument(power->getID());
	message->appendLocationArgument(ctx.groundLocation());
	message->appendRealArgument(INVALID_ANGLE);
	message->appendObjectIDArgument(INVALID_ID);
	message->appendIntegerArgument(button->getOptions());
	message->appendObjectIDArgument(caster->getID());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage64TestCommands()
{
#if !RTS_GENERALS
	registerAction("pkg64-state", tracePayload);
	registerAction("pkg64-barrage", traceBarrage);
	registerAction("pkg64-cast", castPayload);
	registerAction("pkg64-overlay", [](const Context& ctx) {
		if (Object* object = ctx.requireObject(true))
			object->setPowerModelCondition(MODELCONDITION_SPECIAL_CHEERING, LOGICFRAMES_PER_SECOND);
	});
#endif
}
}
