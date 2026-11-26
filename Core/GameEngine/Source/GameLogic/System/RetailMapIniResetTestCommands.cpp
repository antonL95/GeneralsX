#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#include "Common/INI.h"
#include "Common/PlayerTemplate.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/RetailEva.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/RetailTerrainFire.h"

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
	const auto* existing = TheParticleSystemManager->findTemplate("MapIniResetExistingParticleFixture");
	if (existing)
		std::fprintf(stderr, "TEST map-ini existing-particle %s wind=%.1f\n", ctx.argument, existing->getWindStrength());
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
	registerAction("map-ini-existing-particle", [](const Context&) {
		TheParticleSystemManager->newTemplate("MapIniResetExistingParticleFixture");
		const char* commands = std::getenv("GENERALSX_TEST_COMMANDS");
		if (!commands)
			return;
		std::string path(commands);
		const auto slash = path.find_last_of('/');
		if (slash == std::string::npos)
			return;
		path.resize(slash + 1);
		path += "map-ini-existing.ini";
		INI ini;
		ini.load(path.c_str(), INI_LOAD_CREATE_OVERRIDES, nullptr);
	});
	registerAction("map-ini-reset", [](const Context& ctx) {
		ThePlayerTemplateStore->reset();
		TheParticleSystemManager->reset();
		RetailEva::reset();
		TheGameLogic->loadMapINI("map-ini-reset-empty/empty.map");
		probe(ctx);
		RetailEva::queue(ctx.player, "MapIniResetFixture");
		RetailEva::queue(ctx.player, "BuildingCaptured");
	});
}
}
