#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/ScoreKeeper.h"
#include "Common/ThingFactory.h"
#include "GameLogic/ExperienceLevelStore.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace
{
void loadLevels(const RetailTestCommands::Context& ctx)
{
	std::ifstream input(ctx.argument);
	if (!input)
		return;
	std::ostringstream text;
	text << input.rdbuf();
	INI ini;
	ini.loadText(ctx.argument, text.str().c_str(), INI_LOAD_OVERWRITE, "ExperienceLevel", ExperienceLevelStore::parseDefinition);
}

void placeTree(const RetailTestCommands::Context& ctx)
{
	if (const ThingTemplate* tree = TheThingFactory->findTemplate(ctx.targetName, FALSE))
		TheTerrainLogic->registerMapTree(*tree, ctx.groundLocation());
}

void probeDepot(const RetailTestCommands::Context& ctx)
{
	Object* depot = ctx.requireObject(true);
	if (!depot)
		return;
	const ExperienceTracker* tracker = depot->getExperienceTracker();
	std::fprintf(stderr, "TEST worker-depot frame=%u rank=%d xp=%.1f earned=%d\n",
		ctx.frame, tracker->getRank(), tracker->getRetailExperience(),
		depot->getControllingPlayer()->getScoreKeeper()->getTotalMoneyEarned());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailWorkerDepotTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("worker-depot-load-levels", loadLevels);
	registerAction("worker-depot-place-tree", placeTree);
	registerAction("worker-depot-probe", probeDepot);
#endif
}
}
