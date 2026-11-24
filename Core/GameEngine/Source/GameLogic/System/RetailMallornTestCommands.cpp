#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cmath>
#include <cstdio>

#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/CastleBehavior.h"
#include "GameLogic/Module/FoundationAIUpdate.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object.h"

namespace
{
Object* findAt(const RetailTestCommands::Context& ctx)
{
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		const Coord3D& position = *object->getPosition();
		if (object->getControllingPlayer() == ctx.player && !object->isEffectivelyDead() &&
			object->getTemplate()->getName() == ctx.targetName &&
			std::abs(position.x - ctx.position.x) < 0.1f && std::abs(position.y - ctx.position.y) < 0.1f)
			return object;
	}
	return nullptr;
}

void construct(const RetailTestCommands::Context& ctx)
{
	Object* plot = ctx.requireObject(TRUE);
	auto* foundation = plot ? FoundationAIUpdate::find(*plot) : nullptr;
	const auto* type = TheThingFactory->findTemplate(ctx.argument, FALSE);
	const Object* source = nullptr;
	if (foundation && CastleBehavior::findForMember(*plot))
	{
		const Coord3D ground = ctx.groundLocation();
		plot->setPosition(&ground);
		plot->setOrientation(0.0f);
		source = foundation->construct(type, ctx.player);
	}
	std::fprintf(stderr, "MALLORN_BUILD %s %u %u %d\n", ctx.argument,
		source ? static_cast<unsigned>(source->getID()) : 0, plot ? static_cast<unsigned>(plot->getID()) : 0,
		ctx.player && ctx.player->isSkirmishAIPlayer());
}

void order(const RetailTestCommands::Context& ctx)
{
	Object* source = findAt(ctx);
	const CommandButton* button = TheRetailCommandCatalog->findCommandButton(ctx.argument);
	const UpgradeTemplate* upgrade = button ? button->getUpgradeTemplate() : nullptr;
	auto* production = source ? source->getProductionUpdateInterface() : nullptr;
	if (production && upgrade)
		source->doCommandButton(button, CMD_FROM_AI);
	std::fprintf(stderr, "MALLORN_ORDER %d %s %u %d\n", ctx.ordinal, ctx.argument,
		source ? static_cast<unsigned>(source->getID()) : 0,
		production && upgrade && production->isUpgradeInQueue(upgrade));
}

void sample(const RetailTestCommands::Context& ctx)
{
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		const Coord3D& position = *object->getPosition();
		const Real dx = position.x - ctx.position.x;
		const Real dy = position.y - ctx.position.y;
		if (object->getControllingPlayer() != ctx.player || !object->isKindOf(KINDOF_STRUCTURE) ||
			object->isDestroyed() || object->isEffectivelyDead() || object->isKindOf(KINDOF_SUMMONED) ||
			dx * dx + dy * dy > 110.0f * 110.0f)
			continue;
		const auto* foundation = FoundationAIUpdate::find(*object);
		const Object* occupant = foundation ? foundation->getStructure() : nullptr;
		const auto* castle = CastleBehavior::findForMember(*object);
		const Object* keep = castle ? castle->getKeep() : nullptr;
		const auto* production = object->getProductionUpdateInterface();
		const auto* entry = production ? production->firstProduction() : nullptr;
		const auto* upgrade = entry && entry->getProductionType() == PRODUCTION_UPGRADE ? entry->getProductionUpgrade() : nullptr;
		if (object->getTemplate()->getName() == ctx.targetName && ctx.argument[0] != '-')
		{
			const auto* requested = TheUpgradeCenter->findUpgrade(ctx.argument);
			const auto* grant = TheUpgradeCenter->findUpgrade("Upgrade_SwitchToRockThrowing");
			std::fprintf(stderr, "MALLORN_ELIGIBILITY %d grant=%d eligible=%d\n", ctx.ordinal,
				grant && object->hasUpgrade(grant), requested && object->affectedByUpgrade(requested));
		}
		std::fprintf(stderr, "MALLORN_STATE %u %d %s %u %.2f %.2f %.3f %u %u %u %u %d %s\n",
			ctx.frame, ctx.ordinal, object->getTemplate()->getName().str(), static_cast<unsigned>(object->getID()),
			position.x, position.y, object->getOrientation(), static_cast<unsigned>(object->getProducerID()),
			occupant ? static_cast<unsigned>(occupant->getID()) : 0,
			castle ? static_cast<unsigned>(castle->getCastleObjectID()) : 0,
			keep ? static_cast<unsigned>(keep->getID()) : 0,
			object->testStatus(foundation ? OBJECT_STATUS_UNSELECTABLE : OBJECT_STATUS_UNDER_CONSTRUCTION),
			upgrade ? upgrade->getUpgradeName().str() : "-");
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailMallornTestCommands()
{
#if !RTS_GENERALS
	registerAction("mallorn-construct", construct);
	registerAction("mallorn-order", order);
	registerAction("mallorn-state", sample);
#endif
}
}
