#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "GameClient/MapUtil.h"
#include "GameClient/RetailMatchScreens.h"
#include "GameLogic/GameLogic.h"
#include "GameNetwork/GameInfo.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace
{
Int catalogueSlots(const std::vector<RetailSkirmishMap> &maps, const char *path)
{
	const MapMetaData *map = TheMapCache->findMap(path);
	if (!map)
		return 0;
	const auto entry = std::find_if(maps.begin(), maps.end(), [map](const RetailSkirmishMap &item) {
		return item.path.compareNoCase(map->m_fileName) == 0;
	});
	return entry != maps.end() ? entry->players : 0;
}

void inspectCatalogueAndRoster(const RetailTestCommands::Context &ctx)
{
	const char *path = std::getenv("PKGS1_RETAIL_MAP");
	const auto maps = collectRetailSkirmishMaps();
	const GameSlot *human = TheGameInfo ? TheGameInfo->getConstSlot(0) : nullptr;
	const GameSlot *second = TheGameInfo ? TheGameInfo->getConstSlot(1) : nullptr;
	const Player *local = ThePlayerList->getLocalPlayer();
	std::fprintf(stderr,
		"TEST skirmish-catalogue frame=%u slots=%d high-pass=%d skirmish=%d local=%s start=%d second=%d roster=%d\n",
		ctx.frame, path ? catalogueSlots(maps, path) : 0,
		catalogueSlots(maps, "maps/map mp high pass/map mp high pass.map"),
		TheGameLogic->isSimulatingSkirmishGame(), local ? local->getPlayerName().str() : "",
		human && human->isHuman() ? human->getStartPos() : -1,
		second && second->isOccupied(), ThePlayerList->getPlayerCount());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailSkirmishCatalogueTestCommands()
{
#if !RTS_GENERALS
	registerAction("skirmish-catalogue", inspectCatalogueAndRoster);
#endif
}
}
