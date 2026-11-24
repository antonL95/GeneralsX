#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Module/FoundationAIUpdate.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

namespace
{
void logGrant(const RetailTestCommands::Context& ctx)
{
	const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(ctx.argument);
	const Object* object = ctx.findObject(true);
	if (object && object->isDestroyed())
		object = nullptr;
	const Player* player = ctx.findPlayer();
	Int pending = 0;
	if (object)
		for (BehaviorModule* const* module = object->getBehaviorModules(); *module; ++module)
			if ((*module)->getModuleNameKey() == NAMEKEY("GrantUpgradeCreate"))
				pending += (*module)->getCreate()->shouldDoOnBuildComplete();
	const Bool granted = upgrade && (upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER
		? player && player->hasUpgradeComplete(upgrade) : object && object->hasUpgrade(upgrade));
	const Weapon* weapon = object ? object->getCurrentWeapon() : nullptr;
	std::fprintf(stderr,
		"TEST pkg65 grant frame=%u object=%s index=%d upgrade=%s present=%d building=%d pending=%d granted=%d rider5=%d toggle=%d weapon=%s set=%s\n",
		ctx.frame, ctx.targetName, ctx.ordinal, ctx.argument, object != nullptr,
		object && object->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION), pending, granted,
		object && object->getLogicModelConditions().test(MODELCONDITION_RIDER5),
		object && object->testWeaponSetFlag(WEAPONSET_TOGGLE_1),
		weapon ? weapon->getTemplate()->getName().str() : "-", object ? object->getCommandSetString().str() : "-");
}

void logChildren(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject(true);
	if (!source)
		return;
	Int count = 0;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		if (object->getProducerID() == source->getID() && !object->isDestroyed() && !object->isEffectivelyDead() &&
			object->getTemplate()->getName() == ctx.argument)
			++count;
	std::fprintf(stderr, "TEST pkg65 children frame=%u object=%s child=%s count=%d\n",
		ctx.frame, ctx.targetName, ctx.argument, count);
}

void prebuild(const RetailTestCommands::Context& ctx)
{
	Object* plot = ctx.requireObject(true);
	if (plot)
		if (FoundationAIUpdate* foundation = FoundationAIUpdate::find(*plot))
			foundation->construct(TheThingFactory->findTemplate(ctx.argument, FALSE), plot->getControllingPlayer(), TRUE);
}

void cancelProduction(const RetailTestCommands::Context& ctx)
{
	Object* producer = ctx.requireObject(true);
	if (!producer)
		return;
	const auto* production = producer->getProductionUpdateInterface();
	const ProductionEntry* entry = production ? production->firstProduction() : nullptr;
	if (!entry)
		return;
	auto* select = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
	select->appendBooleanArgument(TRUE);
	select->appendObjectIDArgument(producer->getID());
	TheMessageStream->appendMessage(GameMessage::MSG_CANCEL_UNIT_CREATE)->appendIntegerArgument(entry->getProductionID());
	std::fprintf(stderr, "TEST pkg65 cancel production frame=%u object=%s\n", ctx.frame, ctx.targetName);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage65TestCommands()
{
#if !RTS_GENERALS
	registerAction("pkg65-grant", logGrant);
	registerAction("pkg65-children", logChildren);
	registerAction("pkg65-prebuild", prebuild);
	registerAction("pkg65-cancel-production", cancelProduction);
#endif
}
}
