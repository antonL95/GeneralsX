#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/FlammableUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/TerrainLogic.h"
#include <cstdio>

namespace
{
void ignite(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject(true);
	if (!source)
		return;
	auto* fire = static_cast<FlammableUpdate*>(source->findUpdateModule(NAMEKEY("FlammableUpdate")));
	if (fire)
		fire->tryToIgnite();
}

void probeMapTree(const RetailTestCommands::Context& ctx)
{
	const Coord3D position = ctx.groundLocation();
	std::fprintf(stderr, "TEST ai-data tree frame=%u present=%d\n",
		ctx.frame, TheTerrainLogic->isNearMapTree(position, 5.0f));
}

void probeAttackPriority(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject(true);
	const Object* target = ctx.findTarget(ctx.argument, true);
	if (!source || !source->getAI() || !target)
		return;
	const AttackPriorityInfo* info = source->getAI()->getAttackInfo();
	if (!info)
		return;
	std::fprintf(stderr, "TEST ai-data priority frame=%u profile=%s default=%d target=%d\n",
		ctx.frame, info->getName().str(), info->getPriority(nullptr), info->getPriority(target->getTemplate()));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailAiDataTestCommands()
{
#if !RTS_GENERALS
	registerAction("ai-data-priority", probeAttackPriority);
	registerAction("ai-data-ignite", ignite);
	registerAction("ai-data-map-tree", probeMapTree);
#endif
}
}
