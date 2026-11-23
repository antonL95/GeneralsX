#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#include <cstdio>

#include "GameClient/Drawable.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Object.h"
#include "GameLogic/Pathfinder/PathfindCell.h"
#include "GameLogic/WeaponSet.h"

namespace
{
void traceCollapseSite(const RetailTestCommands::Context& ctx)
{
	const Object* structure = ctx.source;
	const Object* attacker = ctx.target;
	if (!ctx.hasPosition)
		return;
	const PathfindCell* cell = TheAI->pathfinder()->getCell(LAYER_GROUND,
		REAL_TO_INT_FLOOR(ctx.position.x / PATHFIND_CELL_SIZE_F),
		REAL_TO_INT_FLOOR(ctx.position.y / PATHFIND_CELL_SIZE_F));
	const Bool owned = structure && cell && cell->getType() == PathfindCell::CELL_OBSTACLE &&
		cell->getObstacleID() == structure->getID();
	const char* tile = "other";
	if (cell)
	{
		switch (cell->getType())
		{
			case PathfindCell::CELL_CLEAR: tile = "clear"; break;
			case PathfindCell::CELL_OBSTACLE: tile = "obstacle"; break;
			case PathfindCell::CELL_RUBBLE: tile = "rubble"; break;
			default: break;
		}
	}
	const Bool attackable = attacker && structure && attacker->getAbleToAttackSpecificObject(
		ATTACK_NEW_TARGET_FORCED, structure, CMD_FROM_PLAYER) != ATTACKRESULT_NOT_POSSIBLE;
	const Drawable* draw = structure ? structure->getDrawable() : nullptr;
	std::fprintf(stderr, "TEST collapse-site frame=%u present=%d registered=%d owned=%d tile=%s attackable=%d rubble=%d post=%d\n",
		ctx.frame, structure != nullptr, structure && structure->friend_getPartitionData(), owned, tile, attackable,
		draw && draw->getModelConditionFlags().test(MODELCONDITION_RUBBLE),
		draw && draw->getModelConditionFlags().test(MODELCONDITION_POST_COLLAPSE));
}
}

namespace RetailTestCommands
{
void registerRetailBuildingCollapseTestCommands()
{
	registerAction("collapse-site", traceCollapseSite);
}
}
