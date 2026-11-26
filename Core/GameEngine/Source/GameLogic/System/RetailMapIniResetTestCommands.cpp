#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#include <cstdio>

#include "Common/GameEngine.h"
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/PlayerTemplate.h"
#include "Common/RandomValue.h"
#include "GameClient/MapUtil.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/RetailEva.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/RetailTerrainFire.h"
#include "GameNetwork/GameInfo.h"

namespace
{
void probe(const RetailTestCommands::Context& ctx)
{
	const PlayerTemplate* player = ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY("FactionNeutral"));
	const ParticleSystemTemplate* particle = TheParticleSystemManager->findTemplate("FXSplash02");
	RetailTerrainFire fire;
	fire.initialize(1, 1, {1});
	std::fprintf(stderr, "TEST map-ini %s player=%u particle=%.1f fire=%d\n", ctx.argument,
		player ? player->getMoney()->countMoney() : 0, particle ? particle->getWindStrength() : -1.0f,
		fire.cellAt(5, 5).fuel);
	std::fprintf(stderr, "TEST map-ini extras %s player=%d particle=%d\n", ctx.argument,
		ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY("MapIniResetPlayerFixture")) != nullptr,
		TheParticleSystemManager->findTemplate("MapIniResetParticleFixture") != nullptr);
}

void startNextMap(const RetailTestCommands::Context& ctx)
{
	const MapMetaData* next = TheMapCache->ensureRetailLaunchMap(ctx.argument);
	if (!next || !TheSkirmishGameInfo)
		return;

	if (TheGlobalData->m_mapName.compareNoCase(next->m_fileName) == 0)
	{
		std::fprintf(stderr, "TEST map-ini finished %s\n", next->m_fileName.str());
		TheGameEngine->setQuitting(TRUE);
		return;
	}

	std::fprintf(stderr, "TEST map-ini leaving %s\n", TheGlobalData->m_mapName.str());
	TheWritableGlobalData->m_initialFile.clear();
	TheGameLogic->exitGame();
	TheWritableGlobalData->m_pendingFile = next->m_fileName;
	TheSkirmishGameInfo->setMap(next->m_fileName);
	TheSkirmishGameInfo->setMapCRC(next->m_CRC);
	TheSkirmishGameInfo->setMapSize(next->m_filesize);
	GameMessage* start = TheMessageStream->appendMessage(GameMessage::MSG_NEW_GAME);
	start->appendIntegerArgument(GAME_SKIRMISH);
	start->appendIntegerArgument(DIFFICULTY_NORMAL);
	InitRandom(TheSkirmishGameInfo->getSeed());
}
}

namespace RetailTestCommands
{
void registerRetailMapIniResetTestCommands()
{
	registerAction("map-ini-probe", probe);
	registerAction("map-ini-eva-queue", [](const Context& ctx) {
		RetailEva::queue(ctx.player, "MapIniResetFixture");
	});
	registerAction("map-ini-eva-probe", [](const Context& ctx) {
		UnsignedInt frame = 0;
		std::fprintf(stderr, "TEST map-ini eva %s played=%d\n", ctx.argument,
			RetailEva::getLastPlayed("MapIniResetFixture", frame));
	});
	registerAction("map-ini-existing-queue", [](const Context& ctx) {
		RetailEva::queue(ctx.player, "BuildingCaptured");
	});
	registerAction("map-ini-existing-probe", [](const Context& ctx) {
		UnsignedInt frame = 0;
		std::fprintf(stderr, "TEST map-ini existing %s played=%d\n", ctx.argument,
			RetailEva::getLastPlayed("BuildingCaptured", frame));
	});
	registerAction("map-ini-next", startNextMap);
}
}
