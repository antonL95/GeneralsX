#include "PreRTS.h"

#include "GameLogic/Module/RotwkModifierModules.h"

#include <cstdio>

#include "Common/AbilityRules.h"
#include "Common/AttributeModifierRules.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/AttributeModifierStore.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/RetailTestCommands.h"
#include "GameLogic/WeaponNugget.h"

namespace
{
void parseCategoryMask(INI* ini, void*, void* store, const void*)
{
	UnsignedInt mask = 0;
	for (const char* token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
	{
		const Int category = AttributeModifierRules::indexOf(AttributeModifierRules::categoryNames(), token);
		if (category < 0)
			throw INI_INVALID_DATA;
		mask |= AttributeModifierRules::categoryBit(category);
	}
	*static_cast<UnsignedInt*>(store) = mask;
}

void parseAuraFilter(INI* ini, void* instance, void*, const void*)
{
	auto* data = static_cast<AttributeModifierAuraUpdateModuleData*>(instance);
	SpecialPowerObjectFilter::parse(ini, nullptr, &data->m_objectFilter, nullptr);
	data->m_hasObjectFilter = TRUE;
}

void parseAllowFilter(INI* ini, void* instance, void*, const void*)
{
	auto* data = static_cast<PassiveAreaEffectBehaviorModuleData*>(instance);
	SpecialPowerObjectFilter::parse(ini, nullptr, &data->m_allowFilter, nullptr);
	data->m_hasAllowFilter = TRUE;
}

void parseNameAppend(INI* ini, void*, void* store, const void*)
{
	for (const char* token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
		static_cast<std::vector<AsciiString>*>(store)->emplace_back(token);
}

UnsignedInt pulseDuration(Int list, UnsignedInt refreshFrames)
{
	const UnsignedInt listFrames = TheAttributeModifierStore ? TheAttributeModifierStore->def(list).durationFrames : 0;
	return listFrames > 0 ? listFrames : refreshFrames + 2;
}

Bool hasUpgradeNamed(const Object* obj, const AsciiString& name)
{
	const UpgradeTemplate* upgrade = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(name) : nullptr;
	if (!upgrade)
		return FALSE;
	if (obj->hasUpgrade(upgrade))
		return TRUE;
	const Player* player = obj->getControllingPlayer();
	return player && player->hasUpgradeComplete(upgrade);
}

Bool traces()
{
	return RetailTestCommands::tracesCombat();
}
}

void AttributeModifierAuraUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const char* const abilityConditions[] = { "MOUNTED", "WEAPON_TOGGLE", nullptr };
	static const FieldParse fields[] =
	{
		{ "StartsActive", INI::parseBool, nullptr, offsetof(AttributeModifierAuraUpdateModuleData, m_startsActive) },
		{ "BonusName", INI::parseAsciiString, nullptr, offsetof(AttributeModifierAuraUpdateModuleData, m_bonusName) },
		{ "RefreshDelay", INI::parseDurationUnsignedInt, nullptr, offsetof(AttributeModifierAuraUpdateModuleData, m_refreshDelay) },
		{ "Range", INI::parseReal, nullptr, offsetof(AttributeModifierAuraUpdateModuleData, m_range) },
		{ "ObjectFilter", parseAuraFilter, nullptr, 0 },
		{ "TargetEnemy", INI::parseBool, nullptr, offsetof(AttributeModifierAuraUpdateModuleData, m_targetEnemy) },
		{ "AllowPowerWhenAttacking", INI::parseBool, nullptr, offsetof(AttributeModifierAuraUpdateModuleData, m_allowPowerWhenAttacking) },
		{ "RequiredConditions", INI::parseBitString32, abilityConditions, offsetof(AttributeModifierAuraUpdateModuleData, m_requiredConditions) },
		{ "AntiCategory", parseCategoryMask, nullptr, offsetof(AttributeModifierAuraUpdateModuleData, m_antiCategories) },
		{ "AntiFX", INI::parseAsciiString, nullptr, offsetof(AttributeModifierAuraUpdateModuleData, m_antiFX) },
		{ "AffectGood", INI::parseBool, nullptr, offsetof(AttributeModifierAuraUpdateModuleData, m_affectGood) },
		{ "AffectEvil", INI::parseBool, nullptr, offsetof(AttributeModifierAuraUpdateModuleData, m_affectEvil) },
		{ "RunWhileDead", INI::parseBool, nullptr, offsetof(AttributeModifierAuraUpdateModuleData, m_runWhileDead) },
		{ "AllowSelf", INI::parseBool, nullptr, offsetof(AttributeModifierAuraUpdateModuleData, m_allowSelf) },
		{ "AffectContainedOnly", INI::parseBool, nullptr, offsetof(AttributeModifierAuraUpdateModuleData, m_affectContainedOnly) },
		{ "MaxActiveRank", INI::parseInt, nullptr, offsetof(AttributeModifierAuraUpdateModuleData, m_maxActiveRank) },
		{ "RequiresAllConflictingTriggers", INI::parseBool, nullptr, offsetof(AttributeModifierAuraUpdateModuleData, m_requiresAllConflictingTriggers) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse(p);
	p.add(fields);
	p.add(UpgradeMuxData::getFieldParse(), offsetof(AttributeModifierAuraUpdateModuleData, m_upgradeMuxData));
}

AttributeModifierAuraUpdate::AttributeModifierAuraUpdate( Thing *thing, const ModuleData *moduleData ) :
	UpdateModule( thing, moduleData ),
	m_targetsLastPulse( 0 )
{
	setWakeFrame(getObject(), getAttributeModifierAuraUpdateModuleData()->m_startsActive ? UPDATE_SLEEP_NONE : UPDATE_SLEEP_FOREVER);
}

AttributeModifierAuraUpdate::~AttributeModifierAuraUpdate()
{
}

void AttributeModifierAuraUpdate::onObjectCreated()
{
	if (getAttributeModifierAuraUpdateModuleData()->m_startsActive)
		setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
}

Bool AttributeModifierAuraUpdate::isAuraActive() const
{
	return getAttributeModifierAuraUpdateModuleData()->m_startsActive || isAlreadyUpgraded();
}

Bool AttributeModifierAuraUpdate::resetUpgrade( const UpgradeMaskType& keyMask )
{
	const Bool reset = UpgradeMux::resetUpgrade(keyMask);
	if (reset && !getAttributeModifierAuraUpdateModuleData()->m_upgradeMuxData.m_permanent)
		setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
	return reset;
}

void AttributeModifierAuraUpdate::pulse()
{
	const AttributeModifierAuraUpdateModuleData* data = getAttributeModifierAuraUpdateModuleData();
	Object* self = getObject();
	m_targetsLastPulse = 0;
	const Int list = AttributeModifiers::findList(data->m_bonusName);
	if (list < 0)
		return;
	if ((data->m_requiredConditions & AbilityRules::ABILITY_CONDITION_MOUNTED) && !self->testWeaponSetFlag(WEAPONSET_MOUNTED))
		return;
	const UnsignedInt duration = pulseDuration(list, data->m_refreshDelay);
	const SpecialPowerObjectFilter* filter = data->m_hasObjectFilter ? &data->m_objectFilter : nullptr;
	const AttributeModifiers::Sides sides = data->m_targetEnemy ? AttributeModifiers::SIDES_ENEMIES : AttributeModifiers::SIDES_ALLIES;
	const FXList* antiFX = data->m_antiCategories && data->m_antiFX.isNotEmpty() && TheFXListStore ?
		TheFXListStore->findFXList(data->m_antiFX.str()) : nullptr;
	const auto affect = [&](Object* target)
	{
		if (target->isEffectivelyDead())
			return;
		if (target == self && !data->m_allowSelf)
			return;
		if (filter && !objectFilterMatches(*filter, self, target))
			return;
		if (target != self && !AttributeModifiers::sidesMatch(self, target, filter, sides))
			return;
		if (AttributeModifiers::apply(target, list, duration, data->m_antiCategories))
		{
			++m_targetsLastPulse;
			if (antiFX)
				FXList::doFXObj(antiFX, target);
		}
	};

	if (data->m_affectContainedOnly)
	{
		ContainModuleInterface* contain = self->getContain();
		const ContainedItemsList* items = contain ? contain->getContainedItemsList() : nullptr;
		if (items)
		{
			for (Object* target : *items)
				affect(target);
		}
		return;
	}
	if (data->m_allowSelf)
		affect(self);
	if (data->m_range <= 0.0f)
		return;
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(self, data->m_range, FROM_CENTER_2D);
	MemoryPoolObjectHolder hold(iter);
	for (Object* target = iter->first(); target; target = iter->next())
	{
		if (target != self)
			affect(target);
	}
}

UpdateSleepTime AttributeModifierAuraUpdate::update()
{
	const AttributeModifierAuraUpdateModuleData* data = getAttributeModifierAuraUpdateModuleData();
	Object* self = getObject();
	if (!isAuraActive())
		return UPDATE_SLEEP_FOREVER;
	if (self->isEffectivelyDead() && !data->m_runWhileDead)
		return UPDATE_SLEEP_FOREVER;
	if (self->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) || self->testStatus(OBJECT_STATUS_SOLD))
		return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND);
	pulse();
	if (traces() && m_targetsLastPulse > 0)
		std::fprintf(stderr, "TEST aura frame=%u owner=%u(%s) bonus=%s targets=%d\n", TheGameLogic->getFrame(),
			static_cast<unsigned>(self->getID()), self->getTemplate()->getName().str(), data->m_bonusName.str(), m_targetsLastPulse);
	return UPDATE_SLEEP(data->m_refreshDelay > 0 ? data->m_refreshDelay : LOGICFRAMES_PER_SECOND);
}

void AttributeModifierAuraUpdate::crc( Xfer *xfer )
{
	UpdateModule::crc(xfer);
	upgradeMuxCRC(xfer);
}

void AttributeModifierAuraUpdate::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	UpdateModule::xfer(xfer);
	upgradeMuxXfer(xfer);
	xfer->xferInt(&m_targetsLastPulse);
}

void AttributeModifierAuraUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	upgradeMuxLoadPostProcess();
}

void PassiveAreaEffectBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] =
	{
		{ "EffectRadius", INI::parseReal, nullptr, offsetof(PassiveAreaEffectBehaviorModuleData, m_effectRadius) },
		{ "PingDelay", INI::parseDurationUnsignedInt, nullptr, offsetof(PassiveAreaEffectBehaviorModuleData, m_pingDelay) },
		{ "HealPercentPerSecond", INI::parsePercentToReal, nullptr, offsetof(PassiveAreaEffectBehaviorModuleData, m_healPercentPerSecond) },
		{ "ModifierName", parseNameAppend, nullptr, offsetof(PassiveAreaEffectBehaviorModuleData, m_modifierNames) },
		{ "AllowFilter", parseAllowFilter, nullptr, 0 },
		{ "UpgradeRequired", INI::parseAsciiString, nullptr, offsetof(PassiveAreaEffectBehaviorModuleData, m_upgradeRequired) },
		{ "NonStackable", INI::parseBool, nullptr, offsetof(PassiveAreaEffectBehaviorModuleData, m_nonStackable) },
		{ "AntiCategories", parseCategoryMask, nullptr, offsetof(PassiveAreaEffectBehaviorModuleData, m_antiCategories) },
		{ "AntiFX", INI::parseAsciiString, nullptr, offsetof(PassiveAreaEffectBehaviorModuleData, m_antiFX) },
		{ "HealFX", INI::parseAsciiString, nullptr, offsetof(PassiveAreaEffectBehaviorModuleData, m_healFX) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse(p);
	p.add(fields);
}

PassiveAreaEffectBehavior::PassiveAreaEffectBehavior( Thing *thing, const ModuleData *moduleData ) :
	UpdateModule( thing, moduleData )
{
	setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
}

PassiveAreaEffectBehavior::~PassiveAreaEffectBehavior()
{
}

UpdateSleepTime PassiveAreaEffectBehavior::update()
{
	const PassiveAreaEffectBehaviorModuleData* data = getPassiveAreaEffectBehaviorModuleData();
	Object* self = getObject();
	const UnsignedInt delay = data->m_pingDelay > 0 ? data->m_pingDelay : LOGICFRAMES_PER_SECOND;
	if (self->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	if (self->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		return UPDATE_SLEEP(delay);
	if (data->m_upgradeRequired.isNotEmpty() && !hasUpgradeNamed(self, data->m_upgradeRequired))
		return UPDATE_SLEEP(delay);

	std::vector<Int> lists;
	for (const AsciiString& name : data->m_modifierNames)
	{
		const Int list = AttributeModifiers::findList(name);
		if (list >= 0)
			lists.push_back(list);
	}
	const Real healFraction = data->m_healPercentPerSecond * INT_TO_REAL(delay) / INT_TO_REAL(LOGICFRAMES_PER_SECOND);
	const FXList* healFX = data->m_healFX.isNotEmpty() && TheFXListStore ? TheFXListStore->findFXList(data->m_healFX.str()) : nullptr;
	const SpecialPowerObjectFilter* filter = data->m_hasAllowFilter ? &data->m_allowFilter : nullptr;
	Int affected = 0;

	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(self, data->m_effectRadius, FROM_CENTER_2D);
	MemoryPoolObjectHolder hold(iter);
	for (Object* target = iter->first(); target; target = iter->next())
	{
		if (target->isEffectivelyDead())
			continue;
		if (filter && !objectFilterMatches(*filter, self, target))
			continue;
		if (!AttributeModifiers::sidesMatch(self, target, filter, AttributeModifiers::SIDES_ALLIES))
			continue;
		for (Int list : lists)
			AttributeModifiers::apply(target, list, pulseDuration(list, delay), data->m_antiCategories);
		BodyModuleInterface* body = target->getBodyModule();
		if (healFraction > 0.0f && body && body->getHealth() < body->getMaxHealth())
		{
			DamageInfo heal;
			heal.in.m_damageType = DAMAGE_HEALING;
			heal.in.m_deathType = DEATH_NONE;
			heal.in.m_sourceID = self->getID();
			heal.in.m_amount = body->getMaxHealth() * healFraction;
			body->attemptHealing(&heal);
			if (healFX)
				FXList::doFXObj(healFX, target);
		}
		++affected;
	}
	if (traces() && affected > 0 && !lists.empty())
		std::fprintf(stderr, "TEST passive frame=%u owner=%u(%s) lists=%d targets=%d\n", TheGameLogic->getFrame(),
			static_cast<unsigned>(self->getID()), self->getTemplate()->getName().str(), static_cast<int>(lists.size()), affected);
	return UPDATE_SLEEP(delay);
}

void PassiveAreaEffectBehavior::crc( Xfer *xfer )
{
	UpdateModule::crc(xfer);
}

void PassiveAreaEffectBehavior::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	UpdateModule::xfer(xfer);
}

void PassiveAreaEffectBehavior::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}

void AttributeModifierUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "AttributeModifier", INI::parseAsciiString, nullptr, offsetof(AttributeModifierUpgradeModuleData, m_attributeModifier) },
		{ "CustomAnimAndDuration", SpecialPowerAnimation::parse, nullptr, offsetof(AttributeModifierUpgradeModuleData, m_customAnimAndDuration) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

AttributeModifierUpgrade::AttributeModifierUpgrade( Thing *thing, const ModuleData *moduleData ) :
	UpgradeModule( thing, moduleData )
{
}

AttributeModifierUpgrade::~AttributeModifierUpgrade()
{
}

void AttributeModifierUpgrade::upgradeImplementation()
{
	const AttributeModifierUpgradeModuleData* data = getAttributeModifierUpgradeModuleData();
	Object* self = getObject();
	const Bool applied = AttributeModifiers::apply(self, data->m_attributeModifier);
	if (traces())
		std::fprintf(stderr, "TEST modifier upgrade frame=%u object=%u(%s) list=%s applied=%d\n", TheGameLogic->getFrame(),
			static_cast<unsigned>(self->getID()), self->getTemplate()->getName().str(), data->m_attributeModifier.str(), applied ? 1 : 0);
}

Bool AttributeModifierUpgrade::resetUpgrade( const UpgradeMaskType& keyMask )
{
	const Bool reset = UpgradeMux::resetUpgrade(keyMask);
	if (reset)
		AttributeModifiers::remove(getObject(), getAttributeModifierUpgradeModuleData()->m_attributeModifier);
	return reset;
}

void AttributeModifierUpgrade::crc( Xfer *xfer )
{
	UpgradeModule::crc(xfer);
}

void AttributeModifierUpgrade::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	UpgradeModule::xfer(xfer);
}

void AttributeModifierUpgrade::loadPostProcess()
{
	UpgradeModule::loadPostProcess();
}

namespace RotwkModifierModules
{
void applyHordeFormationModifiers( Object* horde )
{
	if (!horde || !horde->getAttributeModifierHelper())
		return;
	const HordeContain* contain = HordeContain::get(horde);
	if (!contain)
		return;
	for (const AsciiString& name : contain->getFormationModifiers())
		AttributeModifiers::apply(horde, name, 0);
}
}

namespace RotwkModifierModules
{
void removeHordeFormationModifiers( Object* horde )
{
	if (!horde || !horde->getAttributeModifierHelper())
		return;
	const HordeContain* contain = HordeContain::get(horde);
	if (!contain)
		return;
	for (const AsciiString& name : contain->getFormationModifiers())
		AttributeModifiers::remove(horde, name);
}
}
