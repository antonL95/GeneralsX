#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameLogic/Object.h"

namespace
{
void observeDifficulty(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object)
		return;
	const Player* owner = object->getControllingPlayer();
	const UpgradeTemplate* brutal = TheUpgradeCenter->findUpgrade("Upgrade_BrutalAIMultiPlayer");
	std::fprintf(stderr, "TEST pkg43 difficulty frame=%u level=%d brutal-upgrade=%d cost=%d frames=%d\n",
		ctx.frame, owner->getPlayerDifficulty(), brutal && object->hasUpgrade(brutal),
		object->getTemplate()->calcCostToBuild(owner), object->getTemplate()->calcTimeToBuild(owner));
}

void observeTemplatePrice(const RetailTestCommands::Context& ctx)
{
	const Player* player = ctx.findPlayer();
	const ThingTemplate* type = TheThingFactory->findTemplate("MordorPorter", FALSE);
	if (!player || !type)
		return;
	std::fprintf(stderr, "TEST pkg43 control-price frame=%u cost=%d frames=%d\n",
		ctx.frame, type->calcCostToBuild(player), type->calcTimeToBuild(player));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage43TestCommands()
{
#if !RTS_GENERALS
	registerAction("pkg43-difficulty", observeDifficulty);
	registerAction("pkg43-template-price", observeTemplatePrice);
#endif
}
}
