#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Object.h"
#include <cstdio>

namespace
{
void probeCapture(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject(true);
	const Object* target = ctx.findTarget(ctx.argument, true);
	const SpecialAbilityUpdate* ability = source
		? source->findSpecialAbilityUpdate(SPECIAL_INFANTRY_CAPTURE_BUILDING) : nullptr;
	const AIUpdateInterface* ai = source ? source->getAI() : nullptr;
	if (!ai || !target)
		return;
	const auto* horde = static_cast<const HordeAIUpdate*>(source->findUpdateModule(NAMEKEY("HordeAIUpdate")));
	const Coord3D* goal = horde ? horde->getGoalPosition() : ai->getGoalPosition();
	const bool active = ability && ability->isActive();
	const bool targeted = active && (ability->getCaptureTargetID() == target->getID() ||
		(goal && (*goal - *target->getPosition()).asCoord2D().lengthSqr() < 100.0f * 100.0f));
	std::fprintf(stderr, "TEST skirmish-capture frame=%u index=%d active=%d target=%d\n",
		ctx.frame, ctx.ordinal, active, targeted);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailTacticalRemainderTestCommands()
{
#if !RTS_GENERALS
	registerAction("skirmish-tactic-capture", probeCapture);
#endif
}
}
