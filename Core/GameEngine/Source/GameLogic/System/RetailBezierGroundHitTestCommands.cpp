#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/MessageStream.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BezierProjectileBehavior.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

namespace
{
void handleBezierGroundHit(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject(ctx.action != "bezier-ground-hit-state");
	if (!source)
		return;

	if (ctx.action == "bezier-ground-hit-enter")
	{
		const Object* target = ctx.findTarget();
		if (!target)
			return;
		ctx.selectObject(source);
		auto* message = TheMessageStream->appendMessage(GameMessage::MSG_ENTER);
		message->appendObjectIDArgument(source->getID());
		message->appendObjectIDArgument(target->getID());
		return;
	}

	if (ctx.action == "bezier-ground-hit-cast")
	{
		const CommandButton* button = TheControlBar ? TheControlBar->findCommandButton(ctx.argument) : nullptr;
		if (!button || !button->getSpecialPowerTemplate() || !ctx.hasPosition)
			return;
		auto* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION);
		message->appendIntegerArgument(button->getSpecialPowerTemplate()->getID());
		message->appendLocationArgument(ctx.groundLocation());
		message->appendRealArgument(INVALID_ANGLE);
		message->appendObjectIDArgument(INVALID_ID);
		message->appendIntegerArgument(button->getOptions());
		message->appendObjectIDArgument(source->getID());
		return;
	}

	if (ctx.action != "bezier-ground-hit-state")
		return;

	const auto* body = source->getBodyModule();
	const auto* ai = source->getAI();
	const auto* draw = source->getDrawable();
	const auto* bezier = dynamic_cast<const BezierProjectileBehavior*>(source->getProjectileUpdateInterface());
	const Coord3D* pos = source->getPosition();
	const Bool busy = ai && ai->isBusy();
	const Bool splatted = source->getLogicModelConditions().test(MODELCONDITION_SPLATTED);
	const Bool thrownCond = source->getLogicModelConditions().test(MODELCONDITION_THROWN_PROJECTILE);
	const Bool inFlight = bezier && (bezier->isSelfFlight() || bezier->isPassengerFlight());
	const Int bounce = bezier ? bezier->getBounceIndex() : 0;
	std::fprintf(stderr,
		"TEST bezier-ground-hit state frame=%u name=%s id=%u flight=%d bounce=%d thrown=%d dead=%d splatted=%d busy=%d noAttack=%d hidden=%d pos=%.2f,%.2f,%.2f hp=%.1f/%.1f\n",
		ctx.frame, source->getTemplate()->getName().str(), static_cast<UnsignedInt>(source->getID()),
		inFlight ? 1 : 0, bounce, thrownCond ? 1 : 0, source->isEffectivelyDead() ? 1 : 0, splatted ? 1 : 0,
		busy ? 1 : 0, source->testStatus(OBJECT_STATUS_NO_ATTACK) ? 1 : 0,
		draw && draw->isDrawableEffectivelyHidden() ? 1 : 0, pos->x, pos->y, pos->z,
		body ? body->getHealth() : 0.0f, body ? body->getMaxHealth() : 0.0f);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailBezierGroundHitTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("bezier-ground-hit-", handleBezierGroundHit);
#endif
}
}
