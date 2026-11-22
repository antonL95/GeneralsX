#include "PreRTS.h"

#include "GameLogic/Module/RetailUpgradeModules.h"

#include <algorithm>

#include "Common/GameState.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "WWMath/wwmath.h"

void InheritUpgradeCreateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "Radius", INI::parseReal, nullptr, offsetof(InheritUpgradeCreateModuleData, m_radius) },
		{ "Upgrade", INI::parseAsciiStringVector, nullptr, offsetof(InheritUpgradeCreateModuleData, m_upgrades) },
		{ "ObjectFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(InheritUpgradeCreateModuleData, m_filter) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

InheritUpgradeCreate::InheritUpgradeCreate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data)
{
	setWakeFrame(getObject(), TheGameState && TheGameState->isInLoadGame() ? UPDATE_SLEEP_FOREVER : UPDATE_SLEEP_NONE);
}

InheritUpgradeCreate::~InheritUpgradeCreate() = default;

UpdateSleepTime InheritUpgradeCreate::update()
{
	Object* object = getObject();
	const auto* data = getInheritUpgradeCreateModuleData();
	UpgradeMaskType upgrades;
	for (const AsciiString& name : data->m_upgrades)
		if (const auto* upgrade = TheUpgradeCenter->findUpgrade(name))
			upgrades.set(upgrade->getUpgradeMask());
	if (!upgrades.any() || object->getObjectCompletedUpgradeMask().testForAll(upgrades) || data->m_radius < 0.0f)
		return UPDATE_SLEEP_FOREVER;

	const Real radiusSquared = data->m_radius * data->m_radius;
	for (const Object* source = TheGameLogic->getFirstObject(); source; source = source->getNextObject())
	{
		if (source != object && source->getControllingPlayer() == object->getControllingPlayer() &&
			!source->isEffectivelyDead() && !source->isDestroyed() && source->getObjectCompletedUpgradeMask().testForAll(upgrades) &&
			data->m_filter.matchesObject(object, source) &&
			ThePartitionManager->getDistanceSquared(object, source, FROM_CENTER_2D) <= radiusSquared)
		{
			object->giveUpgradeMask(upgrades);
			break;
		}
	}
	return UPDATE_SLEEP_FOREVER;
}

void InheritUpgradeCreate::crc(Xfer* xfer)
{
	UnsignedInt nextCall = friend_getNextCallFrame();
	xfer->xferUnsignedInt(&nextCall);
}

void InheritUpgradeCreate::xfer(Xfer* xfer) { UpdateModule::xfer(xfer); }
void InheritUpgradeCreate::loadPostProcess() { UpdateModule::loadPostProcess(); }

BuildableHeroListUpgrade::~BuildableHeroListUpgrade() = default;

void BuildableHeroListUpgrade::upgradeImplementation()
{
	Player* player = getObject()->getControllingPlayer();
	setRingHeroesBuildable(player, TRUE);
	if (TheControlBar && player && player->isLocalPlayer())
		TheControlBar->markUIDirty();
}

void BuildableHeroListUpgrade::setRingHeroesBuildable(Player* player, Bool buildable)
{
	const PlayerTemplate* definition = player ? player->getPlayerTemplate() : nullptr;
	if (!definition)
		return;

	for (const AsciiString& hero : definition->getBuildableRingHeroesMP())
		if (const ThingTemplate* type = TheThingFactory->findTemplate(hero, FALSE))
			player->setBuildingAllowed(type, buildable);
}

void BuildableHeroListUpgrade::crc(Xfer* xfer) { UpgradeModule::crc(xfer); }
void BuildableHeroListUpgrade::xfer(Xfer* xfer) { UpgradeModule::xfer(xfer); }
void BuildableHeroListUpgrade::loadPostProcess() { UpgradeModule::loadPostProcess(); }

void GiveUpgradeUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "DeliverUpgrade", INI::parseBool, nullptr, offsetof(GiveUpgradeUpdateModuleData, m_deliverUpgrade) },
		{ "FadeOutSpeed", INI::parseReal, nullptr, offsetof(GiveUpgradeUpdateModuleData, m_fadeOutSpeed) },
		{ "SpawnOutFX", INI::parseFXList, nullptr, offsetof(GiveUpgradeUpdateModuleData, m_spawnOutFX) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

GiveUpgradeUpdate::~GiveUpgradeUpdate() = default;

const UpgradeTemplate* GiveUpgradeUpdate::carriedUpgrade() const
{
	const UpgradeTemplate* carried = nullptr;
	for (const auto* upgrade = TheUpgradeCenter->firstUpgradeTemplate(); upgrade; upgrade = upgrade->friend_getNext())
		if (upgrade->getUpgradeType() == UPGRADE_TYPE_OBJECT && getObject()->hasUpgrade(upgrade))
			carried = upgrade;
	return carried;
}

Bool GiveUpgradeUpdate::canDeliverTo(const Object* target, const UpgradeTemplate* upgrade) const
{
	return target && upgrade && target != getObject() &&
		target->getControllingPlayer() == getObject()->getControllingPlayer() &&
		!target->isEffectivelyDead() && !target->isDestroyed() &&
		!target->getEnclosingContainedBy() &&
		!target->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) &&
		!target->hasUpgrade(upgrade) && target->affectedByUpgrade(upgrade);
}

Bool GiveUpgradeUpdate::acceptsIntent(const Object* target, const Coord3D* position) const
{
	return m_removeAt == 0 && canDeliverTo(target, carriedUpgrade()) && SpecialAbilityUpdate::acceptsIntent(target, position);
}

Bool GiveUpgradeUpdate::initiateIntentToDoSpecialPower(const SpecialPowerTemplate* power, const Object* target,
	const Coord3D* position, const Waypoint* waypoint, UnsignedInt options)
{
	if (power != getTemplate() || m_removeAt != 0)
		return FALSE;

	if (target && HordeContain::get(target->getContainedBy()))
		target = target->getContainedBy();

	const auto* data = getGiveUpgradeUpdateModuleData();
	if (!target && data->m_deliverUpgrade)
	{
		const Object* object = getObject();
		const auto* upgrade = carriedUpgrade();
		std::vector<ObjectID> reserved;
		static const NameKeyType moduleKey = NAMEKEY("GiveUpgradeUpdate");
		for (const Object* porter = TheGameLogic->getFirstObject(); porter; porter = porter->getNextObject())
		{
			if (porter == object || porter->getControllingPlayer() != object->getControllingPlayer() ||
				porter->isEffectivelyDead() || porter->isDestroyed())
				continue;
			for (BehaviorModule* const* module = porter->getBehaviorModules(); *module; ++module)
			{
				if ((*module)->getModuleNameKey() != moduleKey)
					continue;
				const auto* delivery = static_cast<const GiveUpgradeUpdate*>(*module);
				if (delivery->isActive() && !delivery->m_removeAt && delivery->carriedUpgrade() == upgrade)
					reserved.push_back(delivery->getAbilityTargetID());
			}
		}
		constexpr Real searchRadius = 100000.0f;
		Real bestDistance = searchRadius * searchRadius;
		for (const Object* candidate = TheGameLogic->getFirstObject(); candidate; candidate = candidate->getNextObject())
		{
			if (HordeContain::get(candidate->getContainedBy()) || !canDeliverTo(candidate, upgrade) ||
				std::find(reserved.begin(), reserved.end(), candidate->getID()) != reserved.end())
				continue;
			const Real distance = ThePartitionManager->getDistanceSquared(object, candidate, FROM_CENTER_2D);
			if (distance < bestDistance || (distance == bestDistance && (!target || candidate->getID() < target->getID())))
			{
				target = candidate;
				bestDistance = distance;
			}
		}
	}

	return SpecialAbilityUpdate::initiateIntentToDoSpecialPower(power, target, position, waypoint, options);
}

void GiveUpgradeUpdate::triggerAbilityEffect()
{
	Object* porter = getObject();
	Object* target = TheGameLogic->findObjectByID(getAbilityTargetID());
	const auto* upgrade = carriedUpgrade();
	if (m_removeAt || !canDeliverTo(target, upgrade))
	{
		finishTriggeredAbility();
		return;
	}

	SpecialAbilityUpdate::triggerAbilityEffect();
	target->giveUpgrade(upgrade);
	porter->removeUpgrade(upgrade);
	const auto* data = getGiveUpgradeUpdateModuleData();
	FXList::doFXObj(data->m_spawnOutFX, porter);
	porter->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNSELECTABLE));
	const UnsignedInt fadeFrames = static_cast<UnsignedInt>(WWMath::Ceil(1.0f / std::max(0.001f, data->m_fadeOutSpeed)));
	if (Drawable* drawable = porter->getDrawable())
		drawable->fadeOut(fadeFrames);
	m_removeAt = TheGameLogic->getFrame() + fadeFrames;
}

UpdateSleepTime GiveUpgradeUpdate::update()
{
	if (m_removeAt && TheGameLogic->getFrame() >= m_removeAt)
	{
		TheGameLogic->destroyObject(getObject());
		return UPDATE_SLEEP_FOREVER;
	}
	const UpdateSleepTime sleep = SpecialAbilityUpdate::update();
	return m_removeAt ? UPDATE_SLEEP_NONE : sleep;
}

void GiveUpgradeUpdate::crcRetailState(Xfer* xfer)
{
	crcRetailAbilityPhase(xfer);
	xfer->xferUnsignedInt(&m_removeAt);
}

void GiveUpgradeUpdate::crc(Xfer* xfer) { SpecialAbilityUpdate::crc(xfer); }
void GiveUpgradeUpdate::xfer(Xfer* xfer)
{
	SpecialAbilityUpdate::xfer(xfer);
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	xfer->xferUnsignedInt(&m_removeAt);
}
void GiveUpgradeUpdate::loadPostProcess() { SpecialAbilityUpdate::loadPostProcess(); }
