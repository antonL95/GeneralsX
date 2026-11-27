#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>
#include <optional>
#include <string_view>
#include <vector>
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

namespace
{
void deleteWalls(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;
	std::vector<Object*> walls;
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		if (!object->isDestroyed() && object->getTemplate() == source->getTemplate())
			walls.push_back(object);
	{
		std::optional<Pathfinder::WallRemovalBatch> batch;
		if (std::string_view(ctx.argument) == "batch")
			batch.emplace(*TheAI->pathfinder());
		for (Object* wall : walls)
			TheGameLogic->destroyObject(wall);
	}
	std::fprintf(stderr, "TEST wall-batch-delete frame=%u pieces=%zu\n", ctx.frame, walls.size());
}

void sampleWalls(const RetailTestCommands::Context& ctx)
{
	if (!ctx.hasPosition)
		return;
	Pathfinder* pathfinder = TheAI->pathfinder();
	const Int centerX = static_cast<Int>(ctx.position.x / PATHFIND_CELL_SIZE_F);
	const Int centerY = static_cast<Int>(ctx.position.y / PATHFIND_CELL_SIZE_F);
	UnsignedInt hash = 0;
	Int clear = 0;
	for (Int x = centerX - 14; x <= centerX + 14; ++x)
		for (Int y = centerY - 7; y <= centerY + 7; ++y)
		{
			const PathfindCell* wall = pathfinder->getCell(LAYER_WALL, x, y);
			const Bool hasWall = wall && wall->getLayer() == LAYER_WALL;
			clear += hasWall && wall->getType() == PathfindCell::CELL_CLEAR;
			hash = hash * 31 + (hasWall ? 1 + wall->getType() : 0);
			hash = hash * 31 + (hasWall ? wall->getConnectLayer() : LAYER_INVALID);
		}
	std::fprintf(stderr, "TEST wall-batch-state frame=%u clear=%d hash=%08x\n", ctx.frame, clear, hash);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailWallBatchTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("wall-batch-delete", deleteWalls);
	registerAction("wall-batch-state", sampleWalls);
#endif
}
}
