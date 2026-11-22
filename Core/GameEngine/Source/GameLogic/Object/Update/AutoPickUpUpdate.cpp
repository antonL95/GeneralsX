#include "PreRTS.h"

#if !RTS_GENERALS
#include "GameLogic/Module/AutoPickUpUpdate.h"

#include <algorithm>
#include <utility>

#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/RetailFileLayers.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/GrabPassengerSpecialPower.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"

AutoPickUpUpdate::AutoPickUpUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data)
{
	const auto* pickup = getAutoPickUpUpdateModuleData();
	if (pickup->m_runFromButton)
		setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
	else if (pickup->m_delay)
		setWakeFrame(getObject(), UPDATE_SLEEP(pickup->m_delay));
}

AutoPickUpUpdate::~AutoPickUpUpdate() = default;

void AutoPickUpUpdateModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	UpdateModuleData::buildFieldParse(fields);
	static const FieldParse table[] =
	{
		{ "ScanDelayTime", INI::parseDurationUnsignedInt, nullptr, offsetof(AutoPickUpUpdateModuleData, m_delay) },
		{ "ScanDistance", INI::parseReal, nullptr, offsetof(AutoPickUpUpdateModuleData, m_range) },
		{ "PickUpFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(AutoPickUpUpdateModuleData, m_pickUpFilter) },
		{ "EatObjectEntry", parseEatEntry, nullptr, offsetof(AutoPickUpUpdateModuleData, m_eatEntries) },
		{ "RunFromButton", INI::parseBool, nullptr, offsetof(AutoPickUpUpdateModuleData, m_runFromButton) },
		{ "RunFromButtonNumber", INI::parseInt, nullptr, offsetof(AutoPickUpUpdateModuleData, m_buttonNumber) },
		{ "CanScanWhileAttackingOrMoving", INI::parseBool, nullptr, offsetof(AutoPickUpUpdateModuleData, m_scanWhileBusy) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(table);
}

void AutoPickUpUpdateModuleData::parseEatEntry(INI* ini, void*, void* store, const void*)
{
	EatEntry entry;
	entry.myHealth = INI::scanPercentToReal(ini->getNextSubToken("MyHealth"));
	entry.targetHealth = INI::scanPercentToReal(ini->getNextSubToken("TargetHealth"));
	entry.filter.addTokens(ini, ini->getNextSubToken("Filter"));
	static_cast<std::vector<EatEntry>*>(store)->push_back(std::move(entry));
}

AutoPickUpUpdate* AutoPickUpUpdate::findButtonModule(const Object* object, const CommandButton* button)
{
	if (!RetailFileLayers::enabled() || !object || !button || button->getCommandType() != GUI_COMMAND_WAKE_AUTO_PICKUP ||
		object->isEffectivelyDead() || object->isDisabled() || object->getEnclosingContainedBy())
		return nullptr;
	if (object->testWeaponSetFlag(WEAPONSET_TOGGLE_1) ||
		(object->getContain() && object->getContain()->getContainCount() > 0))
		return nullptr;

	const UnsignedInt options = button->getOptions();
	const Int number = options & OPTION_THREE ? 3 : options & OPTION_TWO ? 2 : 1;
	return findButtonNumber(object, number);
}

AutoPickUpUpdate* AutoPickUpUpdate::findButtonNumber(const Object* object, Int number)
{
	for (BehaviorModule** module = object->getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() != NAMEKEY("AutoPickUpUpdate"))
			continue;
		auto* pickup = static_cast<AutoPickUpUpdate*>(*module);
		const auto* data = pickup->getAutoPickUpUpdateModuleData();
		if (data->m_runFromButton && data->m_buttonNumber == number)
			return pickup;
	}
	return nullptr;
}

Bool AutoPickUpUpdate::canUseButton(const Object* object, const CommandButton* button)
{
	return findButtonModule(object, button) != nullptr;
}

Bool AutoPickUpUpdate::canScanFromButton(const Object* object, const CommandButton* button)
{
	auto* pickup = findButtonModule(object, button);
	return pickup && pickup->findEligibleGrab();
}

Bool AutoPickUpUpdate::hasEatingButton(const Object* object)
{
	const AutoPickUpUpdate* pickup = findButtonNumber(object, 3);
	return pickup && !pickup->getAutoPickUpUpdateModuleData()->m_eatEntries.empty() &&
		!object->testWeaponSetFlag(WEAPONSET_TOGGLE_1) &&
		object->getContain() && object->getContain()->getContainCount() == 0;
}

void AutoPickUpUpdate::issueButton(Object* object, const CommandButton* button)
{
	if (auto* pickup = findButtonModule(object, button))
		pickup->setWakeFrame(object, UPDATE_SLEEP_NONE);
}

UpdateSleepTime AutoPickUpUpdate::update()
{
	const auto* data = getAutoPickUpUpdateModuleData();
	scan();
	return data->m_runFromButton ? UPDATE_SLEEP_FOREVER : UPDATE_SLEEP(std::max(1u, data->m_delay));
}

GrabPassengerSpecialPower* AutoPickUpUpdate::findEligibleGrab()
{
	Object* self = getObject();
	const auto* data = getAutoPickUpUpdateModuleData();
	const AIUpdateInterface* ai = self->getAI();
	if (!ai || self->isDestroyed() || self->isEffectivelyDead() || self->isDisabled() || self->getEnclosingContainedBy() ||
		self->testWeaponSetFlag(WEAPONSET_TOGGLE_1) || data->m_range <= 0.0f ||
		!self->getContain() || self->getContain()->getContainCount() > 0 ||
		ai->isBusy() || (!data->m_scanWhileBusy && (ai->isMoving() || ai->isAttacking())))
		return nullptr;

	auto* grab = GrabPassengerSpecialPower::find(self);
	return grab && TheSpecialPowerStore->canUseSpecialPower(self, grab->getSpecialPowerTemplate()) ? grab : nullptr;
}

void AutoPickUpUpdate::scan()
{
	auto* grab = findEligibleGrab();
	if (!grab)
		return;

	Object* self = getObject();
	const auto* data = getAutoPickUpUpdateModuleData();
	SimpleObjectIterator* objects = ThePartitionManager->iterateObjectsInRange(self, data->m_range, FROM_CENTER_2D);
	MemoryPoolObjectHolder hold(objects);
	Real best = sqr(data->m_range);
	Object* target = nullptr;
	const Bool checksPickUpFilter = data->m_pickUpFilter.m_includeKinds.any() ||
		!data->m_pickUpFilter.m_includeObjects.empty() || data->m_pickUpFilter.m_rules != 0;
	const Real health = self->getBodyModule()->getHealth();
	const Real maxHealth = self->getBodyModule()->getMaxHealth();
	for (Object* candidate = objects->first(); candidate; candidate = objects->next())
	{
		if (candidate == self || candidate->isKindOf(KINDOF_HORDE) || !grab->isValidPowerTarget(candidate) ||
			(candidate->isKindOf(KINDOF_TREE) && !candidate->getTemplate()->getRotWKObjectData().isGrabbable))
			continue;
		const Real distance = ThePartitionManager->getDistanceSquared(self, candidate, FROM_CENTER_2D);
		if (distance > best || (distance == best && target && candidate->getID() >= target->getID()))
			continue;

		const auto* body = candidate->getBodyModule();
		const Bool shouldEat = body && std::any_of(data->m_eatEntries.begin(), data->m_eatEntries.end(),
			[&](const AutoPickUpUpdateModuleData::EatEntry& entry)
			{
				return health < maxHealth * entry.myHealth &&
					body->getHealth() <= body->getMaxHealth() * entry.targetHealth &&
					entry.filter.matchesObject(self, candidate);
			});
		if (!shouldEat && !(checksPickUpFilter && data->m_pickUpFilter.matchesObject(self, candidate)))
			continue;
		best = distance;
		target = candidate;
	}

	Coord3D treePosition;
	Real treeAngle;
	const ThingTemplate* tree = data->m_pickUpFilter.m_includeKinds.test(KINDOF_TREE)
		? TheTerrainLogic->findNearestGrabbableMapTree(*self->getPosition(), best, data->m_pickUpFilter, treePosition, treeAngle)
		: nullptr;
	if (tree)
	{
		Object* materialized = TheThingFactory->newObject(tree, ThePlayerList->getNeutralPlayer()->getDefaultTeam());
		if (!materialized)
			return;
		materialized->setPosition(&treePosition);
		materialized->setOrientation(treeAngle);
		if (grab->requestGrab(materialized, 0))
			TheTerrainLogic->removeMapTreeAt(treePosition);
		else
			TheGameLogic->destroyObject(materialized);
	}
	else if (target)
	{
		grab->requestGrab(target, 0);
	}
}

void AutoPickUpUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
}

void AutoPickUpUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
}

void AutoPickUpUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
#endif
