#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
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

void probeAdvance(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject(true);
	const Object* target = ctx.findTarget(ctx.argument, true);
	const auto* horde = source ? static_cast<const HordeAIUpdate*>(source->findUpdateModule(NAMEKEY("HordeAIUpdate"))) : nullptr;
	if (!horde || !target)
		return;
	const Coord3D delta = *horde->getGoalPosition() - *target->getPosition();
	const bool advancing = horde->hasAttackOrder() && !horde->getAttackTarget() && delta.asCoord2D().lengthSqr() < 100.0f * 100.0f;
	std::fprintf(stderr, "TEST skirmish-roam frame=%u advancing=%d\n", ctx.frame, advancing);
}

void probeMill(const RetailTestCommands::Context& ctx)
{
	Object* builder = ctx.requireObject(true);
	const Object* keep = ctx.findTarget(ctx.argument, true);
	if (!builder || !keep)
		return;
	AIUpdateInterface* ai = builder->getAI();
	DozerAIInterface* dozer = ai ? ai->getDozerAIInterface() : nullptr;
	const Object* site = dozer ? TheGameLogic->findObjectByID(dozer->getTaskTarget(DOZER_TASK_BUILD)) : nullptr;
	const bool pending = site && site->isKindOf(KINDOF_SUPPLY_GATHERING_CENTER) && site->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION);
	Coord3D tree{};
	const bool grove = pending && TheTerrainLogic->findNearestHarvestTree(*site->getPosition(), 300.0f, {}, tree);
	const bool outside = pending && (*site->getPosition() - *keep->getPosition()).asCoord2D().lengthSqr() >= 500.0f * 500.0f;
	std::fprintf(stderr, "TEST skirmish-mill frame=%u pending=%d grove=%d outside-base=%d\n", ctx.frame, pending, grove, outside);
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
	registerAction("skirmish-tactic-advance", probeAdvance);
	registerAction("skirmish-tactic-mill", probeMill);
#endif
}
}
