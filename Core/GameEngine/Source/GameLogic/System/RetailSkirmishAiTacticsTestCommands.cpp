#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include <cstdio>

namespace
{
void probeExpansion(const RetailTestCommands::Context& ctx)
{
	Object* builder = ctx.requireObject(true);
	if (!builder)
		return;
	AIUpdateInterface* ai = builder->getAI();
	DozerAIInterface* dozer = ai ? ai->getDozerAIInterface() : nullptr;
	const Object* site = dozer ? TheGameLogic->findObjectByID(dozer->getTaskTarget(DOZER_TASK_BUILD)) : nullptr;
	const bool building = site && (site->isKindOf(KINDOF_COMMANDCENTER) || site->isKindOf(KINDOF_BASE_SITE)) &&
		site->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION);
	const Waypoint* start = TheTerrainLogic->getWaypointByName(ctx.argument);
	bool atStart = false;
	if (building && start)
	{
		const Coord3D delta = *site->getPosition() - *start->getLocation();
		atStart = delta.x * delta.x + delta.y * delta.y < 200.0f * 200.0f;
	}
	std::fprintf(stderr, "TEST skirmish-expansion frame=%u building=%d at-start=%d\n", ctx.frame, building, atStart);
}

void probeReachable(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject(true);
	const Object* target = ctx.findTarget(ctx.argument, true);
	if (!source || !target)
		return;
	const bool reachable = TheAI->pathfinder()->quickDoesPathExistToObject(source, source->getPosition(), target) != FALSE;
	std::fprintf(stderr, "TEST skirmish-probe frame=%u reachable=%d\n", ctx.frame, reachable);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailSkirmishAiTacticsTestCommands()
{
#if !RTS_GENERALS
	registerAction("skirmish-tactic-expansion", probeExpansion);
	registerAction("skirmish-tactic-probe", probeReachable);
#endif
}
}
