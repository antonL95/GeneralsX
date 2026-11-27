#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "Common/Dict.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
void setPowerEconomy(const RetailTestCommands::Context& context)
{
	Player* player = context.findPlayer();
	Int cash = 0;
	Int capacity = 0;
	if (!player || std::sscanf(context.argument, "%d,%d", &cash, &capacity) != 2 || cash < 0 || capacity < 0)
		return;
	player->getMoney()->setStartingCash(cash);
	player->overrideCommandPoints(capacity, capacity);
}

void addFixtureTreeGroup(const RetailTestCommands::Context& context)
{
	const ThingTemplate* tree = TheThingFactory->findTemplate(context.targetName, FALSE);
	Coord3D center{};
	if (!tree || std::sscanf(context.argument, "%f,%f", &center.x, &center.y) != 2)
		return;
	constexpr Int side = 16;
	constexpr Real spacing = 4.0f;
	for (Int i = 0; i < side; ++i)
		for (Int j = 0; j < side; ++j)
		{
			Coord3D position{center.x + (i - side / 2) * spacing, center.y + (j - side / 2) * spacing, 0.0f};
			position.z = TheTerrainLogic->getGroundHeight(position.x, position.y);
			TheTerrainLogic->registerMapTree(*tree, position);
		}
}

void assignMountTeam(const RetailTestCommands::Context& context)
{
	Object* member = context.requireObject(true);
	if (!member)
		return;
	Player* owner = member->getControllingPlayer();
	if (!owner)
		return;
	const AsciiString name(context.argument);
	if (!TheTeamFactory->findTeamPrototype(name))
	{
		Dict teamData;
		TheTeamFactory->initTeam(name, owner->getPlayerName(), TRUE, &teamData);
	}
	Team* team = TheTeamFactory->findTeam(name);
	if (team && team->getControllingPlayer() == owner)
		member->setTeam(team);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailAIPowersTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("ai-powers-economy", setPowerEconomy);
	registerAction("ai-powers-grove", addFixtureTreeGroup);
	registerAction("ai-powers-mount-team", assignMountTeam);
#endif
}
}
