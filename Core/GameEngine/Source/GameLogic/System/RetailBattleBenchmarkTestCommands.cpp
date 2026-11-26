#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <algorithm>
#include <cstdio>
#include <vector>

#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/RetailLogicBenchmark.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
void handleBenchStart(const RetailTestCommands::Context& ctx)
{
	RetailLogicBenchmark::start(ctx.frame);
}

void handleBenchReport(const RetailTestCommands::Context& ctx)
{
	const UnsignedInt crc = TheGameLogic ? TheGameLogic->getCRC(CRC_RECALC) : 0;
	RetailLogicBenchmark::report(ctx.frame, crc);
}

void handleBenchSpawnGrid(const RetailTestCommands::Context& ctx)
{
	Player* player = ctx.findPlayer();
	const ThingTemplate* what = TheThingFactory ? TheThingFactory->findTemplate(ctx.targetName, FALSE) : nullptr;
	float x0 = 0.0f;
	float y0 = 0.0f;
	int cols = 1;
	float dx = 0.0f;
	float dy = 0.0f;
	float angleDeg = 0.0f;
	if (!player || !what || ctx.ordinal <= 0 ||
		std::sscanf(ctx.argument, "%f,%f,%d,%f,%f,%f", &x0, &y0, &cols, &dx, &dy, &angleDeg) < 5 ||
		cols <= 0)
	{
		std::fprintf(stderr, "TEST frame %u: bench-spawn-grid invalid %.*s %s\n",
			ctx.frame, static_cast<int>(ctx.playerName.size()), ctx.playerName.data(), ctx.targetName);
		return;
	}

	const Real angleRad = angleDeg * PI / 180.0f;
	int spawned = 0;
	for (int i = 0; i < ctx.ordinal; ++i)
	{
		const int col = i % cols;
		const int row = i / cols;
		const Real x = x0 + static_cast<Real>(col) * dx;
		const Real y = y0 + static_cast<Real>(row) * dy;
		Object* obj = TheThingFactory->newObject(what, player->getDefaultTeam());
		if (!obj)
			continue;
		Coord3D pos;
		pos.set(x, y, TheTerrainLogic->getGroundHeight(x, y));
		obj->setPosition(&pos);
		obj->setOrientation(angleRad);
		if (obj->isKindOf(KINDOF_STRUCTURE) && !obj->isContained())
			TheAI->pathfinder()->addObjectToPathfindMap(obj);
		++spawned;
	}
	std::fprintf(stderr, "TEST frame %u: bench-spawn-grid %s %s count=%d\n",
		ctx.frame, player->getPlayerName().str(), ctx.targetName, spawned);
}

void handleBenchAttackMove(const RetailTestCommands::Context& ctx)
{
	Player* player = ctx.findPlayer();
	if (!player || !ctx.hasPosition || !TheGameLogic || !TheMessageStream)
	{
		std::fprintf(stderr, "TEST frame %u: bench-attackmove invalid\n", ctx.frame);
		return;
	}

	const Coord3D destination = ctx.groundLocation();
	std::vector<ObjectID> unitIds;
	for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj->getControllingPlayer() != player)
			continue;
		if (obj->isEffectivelyDead() || obj->isContained() || obj->isKindOf(KINDOF_STRUCTURE))
			continue;
		if (!obj->getAIUpdateInterface())
			continue;
		if (obj->isKindOf(KINDOF_DOZER))
			continue;
		unitIds.push_back(obj->getID());
	}

	constexpr std::size_t kBatchSize = 50;
	const PlayerIndex playerIndex = player->getPlayerIndex();
	for (std::size_t offset = 0; offset < unitIds.size(); offset += kBatchSize)
	{
		const std::size_t end = std::min(offset + kBatchSize, unitIds.size());
		GameMessage* select = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
		select->friend_setPlayerIndex(playerIndex);
		select->appendBooleanArgument(TRUE);
		for (std::size_t i = offset; i < end; ++i)
			select->appendObjectIDArgument(unitIds[i]);

		GameMessage* order = TheMessageStream->appendMessage(GameMessage::MSG_DO_ATTACKMOVETO);
		order->friend_setPlayerIndex(playerIndex);
		order->appendLocationArgument(destination);
	}
	std::fprintf(stderr, "TEST frame %u: bench-attackmove %s units=%zu to (%.1f, %.1f)\n",
		ctx.frame, player->getPlayerName().str(), unitIds.size(), destination.x, destination.y);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailBattleBenchmarkTestCommands()
{
#if !RTS_GENERALS
	registerAction("bench-start", handleBenchStart);
	registerAction("bench-report", handleBenchReport);
	registerAction("bench-spawn-grid", handleBenchSpawnGrid);
	registerAction("bench-attackmove", handleBenchAttackMove);
#endif
}
}
