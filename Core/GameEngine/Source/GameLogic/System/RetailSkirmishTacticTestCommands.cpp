#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Object.h"
#include <cstdio>

namespace
{
void probeGuard(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject(true);
	const Object* target = ctx.findTarget(ctx.argument, true);
	const AIUpdateInterface* ai = source ? source->getAI() : nullptr;
	if (!ai || !target)
		return;
	const auto* horde = static_cast<const HordeAIUpdate*>(source->findUpdateModule(NAMEKEY("HordeAIUpdate")));
	const bool guarding = ai->getGuardObject() == target->getID() && (horde || ai->getGuardTargetType() == GUARDTARGET_OBJECT);
	std::fprintf(stderr, "TEST skirmish-guard frame=%u index=%d guarding=%d\n", ctx.frame, ctx.ordinal, guarding);
}

void probeAttack(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject(true);
	const Object* target = ctx.findTarget(ctx.argument, true);
	const AIUpdateInterface* ai = source ? source->getAI() : nullptr;
	if (!ai || !target)
		return;
	const auto* horde = static_cast<const HordeAIUpdate*>(source->findUpdateModule(NAMEKEY("HordeAIUpdate")));
	const Object* victim = horde ? horde->getAttackTarget() : ai->getCurrentVictim();
	std::fprintf(stderr, "TEST skirmish-raid frame=%u index=%d attacking=%d\n", ctx.frame, ctx.ordinal, victim == target);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailSkirmishTacticTestCommands()
{
#if !RTS_GENERALS
	registerAction("skirmish-tactic-guard", probeGuard);
	registerAction("skirmish-tactic-attack", probeAttack);
#endif
}
}
