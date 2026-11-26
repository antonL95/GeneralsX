#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/Player.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"

namespace
{
void sampleCombatCoverage(const RetailTestCommands::Context& ctx)
{
	if (ctx.action == "combat-coverage-cash")
	{
		if (const Player* player = ctx.findPlayer())
			std::fprintf(stderr, "TEST combat-coverage cash %s %u\n", ctx.argument, player->getMoney()->countMoney());
		return;
	}

	const Object* object = ctx.requireObject();
	const HordeContain* horde = HordeContain::get(object);
	if (!horde)
		return;
	Real health = 0.0f;
	for (const Object* member : *horde->getContainedItemsList())
		if (!member->isEffectivelyDead())
			health += member->getBodyModule()->getHealth();
	std::fprintf(stderr, "TEST combat-coverage horde %s %.0f\n", ctx.argument, health);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailCombatFlankingTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("combat-coverage-", sampleCombatCoverage);
#endif
}
}
