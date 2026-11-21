#include "PreRTS.h"

#define DEFINE_WEAPONSLOTTYPE_NAMES

#include "GameLogic/Module/RotwkAbilityModules.h"

#include <algorithm>
#include <memory>
#include <string>

#include "Common/AbilityRules.h"
#include "Common/ActionManager.h"
#include "Common/GameAudio.h"
#include "Common/INIAudit.h"
#include "Common/INIException.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/AIStateMachine.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/HordeContain.h"

namespace
{
enum FilterRuleBit
{
	RULE_SELF = 0,
	RULE_ALLIES = 1,
	RULE_ENEMIES = 2,
	RULE_NEUTRALS = 3,
	RULE_ALL = 10,
	RULE_NONE = 12,
	RULE_SAME_PLAYER = 13,
};

constexpr UnsignedInt AUTO_ABILITY_SCAN_FRAMES = LOGICFRAMES_PER_SECOND / 2;

Bool hasRule(const SpecialPowerObjectFilter& filter, Int bit)
{
	return (filter.m_rules & (1u << bit)) != 0;
}

Bool namedIn(const std::vector<AsciiString>& names, const Object* obj)
{
	const char* name = obj->getTemplate()->getName().str();
	return std::any_of(names.begin(), names.end(), [name](const AsciiString& entry) { return stricmp(entry.str(), name) == 0; });
}

SpecialPowerModule* powerModuleOf(Object* obj, const SpecialPowerTemplate* power)
{
	return power ? static_cast<SpecialPowerModule*>(obj->getSpecialPowerModule(power)) : nullptr;
}

template<class Data>
ModuleData* parsePowerData(INI* ini, const char* missingTemplate)
{
	auto data = std::unique_ptr<Data>(MSGNEW("AllModuleData") Data);
	if (ini)
	{
		ini->initFromINIMultiProc(data.get(), Data::buildFieldParse);
		if (!data->m_specialPowerTemplate)
			throw INIException(missingTemplate);
	}
	return data.release();
}

}

void WeaponFireSpecialAbilityUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "SpecialWeapon", INI::parseWeaponTemplate, nullptr, offsetof(WeaponFireSpecialAbilityUpdateModuleData, m_specialWeapon) },
		{ "WhichSpecialWeapon", INI::parseInt, nullptr, offsetof(WeaponFireSpecialAbilityUpdateModuleData, m_whichSpecialWeapon) },
		{ "SkipContinue", INI::parseBool, nullptr, offsetof(WeaponFireSpecialAbilityUpdateModuleData, m_skipContinue) },
		{ "BusyForDuration", INI::parseDurationUnsignedInt, nullptr, offsetof(WeaponFireSpecialAbilityUpdateModuleData, m_busyForDuration) },
		{ "PlayWeaponPreFireFX", INI::parseBool, nullptr, offsetof(WeaponFireSpecialAbilityUpdateModuleData, m_playWeaponPreFireFX) },
		{ "NeedLivingTargets", INI::parseBool, nullptr, offsetof(WeaponFireSpecialAbilityUpdateModuleData, m_needLivingTargets) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

WeaponFireSpecialAbilityUpdate::WeaponFireSpecialAbilityUpdate(Thing* thing, const ModuleData* moduleData)
	: SpecialAbilityUpdate(thing, moduleData)
{
}

void WeaponFireSpecialAbilityUpdate::triggerAbilityEffect()
{
	SpecialAbilityUpdate::triggerAbilityEffect();
	const WeaponFireSpecialAbilityUpdateModuleData* data = getWeaponFireSpecialAbilityUpdateModuleData();
	if (!data->m_specialWeapon || !TheWeaponStore)
		return;
	Object* self = getObject();
	if (getAbilityTargetID() != INVALID_ID)
	{
		Object* target = TheGameLogic->findObjectByID(getAbilityTargetID());
		if (target && !(data->m_needLivingTargets && target->isEffectivelyDead()))
			TheWeaponStore->createAndFireTempWeapon(data->m_specialWeapon, self, target);
		return;
	}
	const Coord3D position = hasAbilityTargetPos() ? getAbilityTargetPos() : *self->getPosition();
	TheWeaponStore->createAndFireTempWeapon(data->m_specialWeapon, self, &position);
}

Bool WeaponFireSpecialAbilityUpdate::skipsContinue() const
{
	return getWeaponFireSpecialAbilityUpdateModuleData()->m_skipContinue;
}

UnsignedInt WeaponFireSpecialAbilityUpdate::holdFramesAfterTrigger() const
{
	const WeaponFireSpecialAbilityUpdateModuleData* data = getWeaponFireSpecialAbilityUpdateModuleData();
	const UnsignedInt hold = SpecialAbilityUpdate::holdFramesAfterTrigger();
	const UnsignedInt spent = data->m_unpackTime + data->m_preparationFrames + data->m_packTime;
	if (data->m_busyForDuration > spent)
		return std::max(hold, data->m_busyForDuration - spent);
	return hold;
}

void WeaponFireSpecialAbilityUpdate::addAbilityConditions(ModelConditionFlags& flags) const
{
	SpecialAbilityUpdate::addAbilityConditions(flags);
	const std::string name = AbilityRules::specialWeaponCondition(getWeaponFireSpecialAbilityUpdateModuleData()->m_whichSpecialWeapon);
	if (!name.empty())
		flags.setBitByName(name.c_str());
}

void WeaponFireSpecialAbilityUpdate::crc(Xfer* xfer)
{
	SpecialAbilityUpdate::crc(xfer);
}

void WeaponFireSpecialAbilityUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	SpecialAbilityUpdate::xfer(xfer);
}

void WeaponFireSpecialAbilityUpdate::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}

void ToggleMountedSpecialAbilityUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "OpacityTarget", INI::parseReal, nullptr, offsetof(ToggleMountedSpecialAbilityUpdateModuleData, m_opacityTarget) },
		{ "TriggerInstantlyOnCreate", INI::parseBool, nullptr, offsetof(ToggleMountedSpecialAbilityUpdateModuleData, m_triggerInstantlyOnCreate) },
		{ "CancelDisguiseWhenDismounting", INI::parseBool, nullptr, offsetof(ToggleMountedSpecialAbilityUpdateModuleData, m_cancelDisguiseWhenDismounting) },
		{ "MountedTemplate", INI::parseAsciiString, nullptr, offsetof(ToggleMountedSpecialAbilityUpdateModuleData, m_mountedTemplate) },
		{ "SynchronizeTimerOnSpecialPower", INI::parseAsciiStringVector, nullptr, offsetof(ToggleMountedSpecialAbilityUpdateModuleData, m_synchronizeTimerOnSpecialPower) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

ToggleMountedSpecialAbilityUpdate::ToggleMountedSpecialAbilityUpdate(Thing* thing, const ModuleData* moduleData)
	: SpecialAbilityUpdate(thing, moduleData)
{
}

Bool ToggleMountedSpecialAbilityUpdate::isMounted(const Object* obj)
{
	return obj->testWeaponSetFlag(WEAPONSET_MOUNTED);
}

void ToggleMountedSpecialAbilityUpdate::setMounted(Object* obj, Bool mounted)
{
	if (mounted == isMounted(obj))
		return;
	if (mounted)
	{
		obj->setWeaponSetFlag(WEAPONSET_MOUNTED);
		obj->setArmorSetFlag(ARMORSET_MOUNTED);
		obj->setModelConditionState(MODELCONDITION_MOUNTED);
	}
	else
	{
		obj->clearWeaponSetFlag(WEAPONSET_MOUNTED);
		obj->clearArmorSetFlag(ARMORSET_MOUNTED);
		obj->clearModelConditionState(MODELCONDITION_MOUNTED);
	}
	AIUpdateInterface* ai = obj->getAI();
	if (ai && (ai->getCurLocomotorSetType() == LOCOMOTORSET_NORMAL || ai->getCurLocomotorSetType() == LOCOMOTORSET_NORMAL_UPGRADED ||
		ai->getCurLocomotorSetType() == LOCOMOTORSET_MOUNTED))
		ai->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
}

void ToggleMountedSpecialAbilityUpdate::onObjectCreated()
{
	SpecialAbilityUpdate::onObjectCreated();
	const ToggleMountedSpecialAbilityUpdateModuleData* data = getToggleMountedSpecialAbilityUpdateModuleData();
	if (usesRotwkRules() && data->m_triggerInstantlyOnCreate && data->m_mountedTemplate.isEmpty())
		setMounted(getObject(), TRUE);
}

void ToggleMountedSpecialAbilityUpdate::triggerAbilityEffect()
{
	SpecialAbilityUpdate::triggerAbilityEffect();
	if (getToggleMountedSpecialAbilityUpdateModuleData()->m_mountedTemplate.isNotEmpty())
		return;
	setMounted(getObject(), !isMounted(getObject()));
}

void ToggleMountedSpecialAbilityUpdate::crc(Xfer* xfer)
{
	SpecialAbilityUpdate::crc(xfer);
}

void ToggleMountedSpecialAbilityUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	SpecialAbilityUpdate::xfer(xfer);
}

void ToggleMountedSpecialAbilityUpdate::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}

void ActivateModuleSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "TriggerSpecialPower", parseTriggerSpecialPower, nullptr, offsetof(ActivateModuleSpecialPowerModuleData, m_triggers) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

void ActivateModuleSpecialPowerModuleData::parseTriggerSpecialPower(INI* ini, void*, void* store, const void*)
{
	ActivateModuleEntry entry;
	entry.m_moduleTagName = ini->getNextToken();
	entry.m_moduleTag = NAMEKEY(entry.m_moduleTagName);
	if (const char* position = ini->getNextTokenOrNull())
	{
		if (stricmp(position, "OBJECTPOS") == 0 || stricmp(position, "OBJECT_POS") == 0)
			entry.m_position = ActivateModuleEntry::POSITION_OBJECT;
		else if (stricmp(position, "TARGETPOS") == 0 || stricmp(position, "TARGET_POS") == 0)
			entry.m_position = ActivateModuleEntry::POSITION_TARGET;
		else
			throw INI_INVALID_DATA;
	}
	static_cast<std::vector<ActivateModuleEntry>*>(store)->push_back(entry);
}

ActivateModuleSpecialPower::ActivateModuleSpecialPower(Thing* thing, const ModuleData* moduleData)
	: SpecialAbilityUpdate(thing, moduleData)
{
}

void ActivateModuleSpecialPower::triggerAbilityEffect()
{
	SpecialAbilityUpdate::triggerAbilityEffect();
	Object* self = getObject();
	const Object* target = TheGameLogic->findObjectByID(getAbilityTargetID());
	Coord3D targetPos = *self->getPosition();
	if (target)
		targetPos = *target->getPosition();
	else if (hasAbilityTargetPos())
		targetPos = getAbilityTargetPos();

	for (const ActivateModuleEntry& entry : getActivateModuleSpecialPowerModuleData()->m_triggers)
	{
		for (BehaviorModule** module = self->getBehaviorModules(); *module; ++module)
		{
			if ((*module)->getModuleTagNameKey() != entry.m_moduleTag)
				continue;
			SpecialPowerModuleInterface* power = (*module)->getSpecialPower();
			if (!power)
				break;
			const Coord3D ownPos = *self->getPosition();
			switch (entry.m_position)
			{
				case ActivateModuleEntry::POSITION_OBJECT:
					power->doSpecialPowerAtLocation(&ownPos, INVALID_ANGLE, 0);
					break;
				case ActivateModuleEntry::POSITION_TARGET:
					power->doSpecialPowerAtLocation(&targetPos, INVALID_ANGLE, 0);
					break;
				default:
					power->doSpecialPower(0);
					break;
			}
			break;
		}
	}
}

void ActivateModuleSpecialPower::crc(Xfer* xfer)
{
	SpecialAbilityUpdate::crc(xfer);
}

void ActivateModuleSpecialPower::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	SpecialAbilityUpdate::xfer(xfer);
}

void ActivateModuleSpecialPower::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}

void HeroModeSpecialAbilityUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "HeroEffectDuration", INI::parseDurationUnsignedInt, nullptr, offsetof(HeroModeSpecialAbilityUpdateModuleData, m_heroEffectDuration) },
		{ "HeroAttributeModifier", INI::parseAsciiString, nullptr, offsetof(HeroModeSpecialAbilityUpdateModuleData, m_heroAttributeModifier) },
		{ "StopUnitBeforeActivating", INI::parseBool, nullptr, offsetof(HeroModeSpecialAbilityUpdateModuleData, m_stopUnitBeforeActivating) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

HeroModeSpecialAbilityUpdate::HeroModeSpecialAbilityUpdate(Thing* thing, const ModuleData* moduleData)
	: SpecialAbilityUpdate(thing, moduleData)
{
}

void HeroModeSpecialAbilityUpdate::triggerAbilityEffect()
{
	SpecialAbilityUpdate::triggerAbilityEffect();
	const HeroModeSpecialAbilityUpdateModuleData* data = getHeroModeSpecialAbilityUpdateModuleData();
	if (data->m_heroAttributeModifier.isNotEmpty())
		AttributeModifiers::apply(getObject(), data->m_heroAttributeModifier,
			data->m_heroEffectDuration > 0 ? data->m_heroEffectDuration : AttributeModifierHelper::USE_LIST_DURATION, 0, AttributeModifiers::ownerOf(getObject(), getModuleTagNameKey()));
}

void HeroModeSpecialAbilityUpdate::crc(Xfer* xfer)
{
	SpecialAbilityUpdate::crc(xfer);
}

void HeroModeSpecialAbilityUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	SpecialAbilityUpdate::xfer(xfer);
}

void HeroModeSpecialAbilityUpdate::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}

void WeaponModeSpecialPowerUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "SpecialPowerTemplate", INI::parseSpecialPowerTemplate, nullptr, offsetof(WeaponModeSpecialPowerUpdateModuleData, m_power) + offsetof(SpecialPowerModuleData, m_specialPowerTemplate) },
		{ "AttributeModifier", INI::parseAsciiString, nullptr, offsetof(WeaponModeSpecialPowerUpdateModuleData, m_attributeModifier) },
		{ "Duration", INI::parseDurationUnsignedInt, nullptr, offsetof(WeaponModeSpecialPowerUpdateModuleData, m_duration) },
		{ "LockWeaponSlot", INI::parseLookupList, TheWeaponSlotTypeNamesLookupList, offsetof(WeaponModeSpecialPowerUpdateModuleData, m_lockWeaponSlot) },
		{ "WeaponSetFlags", WeaponSetFlags::parseFromINI, nullptr, offsetof(WeaponModeSpecialPowerUpdateModuleData, m_weaponSetFlags) },
		{ "StartsPaused", INI::parseBool, nullptr, offsetof(WeaponModeSpecialPowerUpdateModuleData, m_power) + offsetof(SpecialPowerModuleData, m_startsPaused) },
		{ "InitiateSound", INI::parseAudioEventRTS, nullptr, offsetof(WeaponModeSpecialPowerUpdateModuleData, m_initiateSound) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

WeaponModeSpecialPowerUpdate::WeaponModeSpecialPowerUpdate(Thing* thing, const ModuleData* moduleData)
	: SpecialPowerUpdateModule(thing, moduleData)
{
	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}

void WeaponModeSpecialPowerUpdate::resolveCastOwner()
{
	const auto& power = getWeaponModeSpecialPowerUpdateModuleData()->m_power;
	if (power.m_specialPowerTemplate && !getObject()->getSpecialPowerModule(power.m_specialPowerTemplate))
		m_power = newInstance(SpecialPowerModule)(getObject(), &power);
}

Bool WeaponModeSpecialPowerUpdate::isModuleForPower(const SpecialPowerTemplate* power) const
{
	return power && power == getWeaponModeSpecialPowerUpdateModuleData()->m_power.m_specialPowerTemplate;
}

void WeaponModeSpecialPowerUpdate::onDelete()
{
	if (!m_lockedMembers.empty())
		endMode();
	SpecialPowerUpdateModule::onDelete();
}

Bool WeaponModeSpecialPowerUpdate::initiateIntentToDoSpecialPower(const SpecialPowerTemplate* specialPowerTemplate, const Object*,
	const Coord3D*, const Waypoint*, UnsignedInt)
{
	const WeaponModeSpecialPowerUpdateModuleData* data = getWeaponModeSpecialPowerUpdateModuleData();
	if (!isModuleForPower(specialPowerTemplate))
		return FALSE;
	Object* self = getObject();
	if (m_endFrame != 0)
		endMode();

	self->setWeaponSetFlags(data->m_weaponSetFlags, TRUE);
	const WeaponSlotType slot = data->m_lockWeaponSlot;
	if (slot >= PRIMARY_WEAPON && slot < WEAPONSLOT_COUNT)
	{
		if (self->getWeaponInWeaponSlot(slot))
			self->setWeaponLock(slot, LOCKED_PERMANENTLY);
		else if (const HordeContain* horde = HordeContain::get(self))
		{
			for (Object* member : *horde->getContainedItemsList())
			{
				if (!member->isEffectivelyDead() && member->getWeaponInWeaponSlot(slot) &&
					member->setWeaponLock(slot, LOCKED_PERMANENTLY))
					m_lockedMembers.push_back(member->getID());
			}
		}
	}

	AudioEventRTS sound = data->m_initiateSound;
	sound.setObjectID(self->getID());
	TheAudio->addAudioEvent(&sound);

	const UnsignedInt now = TheGameLogic->getFrame();
	m_endFrame = now + std::max<UnsignedInt>(1, data->m_duration);
	if (data->m_attributeModifier.isNotEmpty())
		AttributeModifiers::apply(self, data->m_attributeModifier,
			data->m_duration > 0 ? data->m_duration : AttributeModifierHelper::USE_LIST_DURATION, 0, AttributeModifiers::ownerOf(self, getModuleTagNameKey()));

	SpecialPowerModule* power = powerModuleOf(self, specialPowerTemplate);
	if (power)
	{
		const SpecialPowerModuleData* powerData = power->getPowerModuleData();
		if (powerData->m_updateModuleStartsAttack)
			power->markSpecialPowerTriggered(nullptr);
		const UnsignedInt frames = SpecialAbilityUpdate::secondsToFrames(powerData->m_setModelConditionTime);
		if (powerData->m_setModelCondition >= 0 && frames > 0)
		{
			if (m_condition >= 0)
				self->clearModelConditionState((ModelConditionFlagType)m_condition);
			m_condition = powerData->m_setModelCondition;
			m_conditionEndFrame = now + frames;
			self->setModelConditionState((ModelConditionFlagType)m_condition);
		}
	}
	setWakeFrame(self, UPDATE_SLEEP_NONE);
	return TRUE;
}

void WeaponModeSpecialPowerUpdate::endMode()
{
	const WeaponModeSpecialPowerUpdateModuleData* data = getWeaponModeSpecialPowerUpdateModuleData();
	Object* self = getObject();
	if (data->m_attributeModifier.isNotEmpty())
		AttributeModifiers::remove(self, data->m_attributeModifier, AttributeModifiers::ownerOf(getObject(), getModuleTagNameKey()));
	const auto releaseSlot = [slot = data->m_lockWeaponSlot](Object* object)
	{
		WeaponSlotType currentSlot = WEAPONSLOT_COUNT;
		if (object && object->getCurrentWeapon(&currentSlot) && currentSlot == slot)
			object->releaseWeaponLock(LOCKED_PERMANENTLY);
	};
	releaseSlot(self);
	for (ObjectID id : m_lockedMembers)
		releaseSlot(TheGameLogic->findObjectByID(id));
	m_lockedMembers.clear();
	self->setWeaponSetFlags(data->m_weaponSetFlags, FALSE);
	m_endFrame = 0;
}

UpdateSleepTime WeaponModeSpecialPowerUpdate::update()
{
	const UnsignedInt now = TheGameLogic->getFrame();
	if (m_endFrame != 0 && now >= m_endFrame)
		endMode();
	if (m_condition >= 0 && now >= m_conditionEndFrame)
	{
		getObject()->clearModelConditionState((ModelConditionFlagType)m_condition);
		m_condition = -1;
	}
	return (m_endFrame != 0 || m_condition >= 0) ? UPDATE_SLEEP_NONE : UPDATE_SLEEP_FOREVER;
}

void WeaponModeSpecialPowerUpdate::crc(Xfer* xfer)
{
	SpecialPowerUpdateModule::crc(xfer);
	xfer->xferUnsignedInt(&m_endFrame);
	xfer->xferInt(&m_condition);
	xfer->xferUnsignedInt(&m_conditionEndFrame);
	xfer->xferSTLObjectIDVector(&m_lockedMembers);
}

void WeaponModeSpecialPowerUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 3;
	xfer->xferVersion(&version, 3);
	SpecialPowerUpdateModule::xfer(xfer);
	xfer->xferUnsignedInt(&m_endFrame);
	xfer->xferInt(&m_condition);
	xfer->xferUnsignedInt(&m_conditionEndFrame);
	if (version >= 2 && m_power)
		xfer->xferSnapshot(m_power);
	if (version >= 3)
		xfer->xferSTLObjectIDVector(&m_lockedMembers);
}

void WeaponModeSpecialPowerUpdate::loadPostProcess()
{
	SpecialPowerUpdateModule::loadPostProcess();
	if (m_power)
		m_power->loadPostProcess();
}

void PlayerUpgradeSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	SpecialPowerModuleData::buildFieldParse(fields);
	static const FieldParse table[] =
	{
		{ "UpgradeName", INI::parseAsciiStringVector, nullptr, offsetof(PlayerUpgradeSpecialPowerModuleData, m_upgrades) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(table);
}

ModuleData* PlayerUpgradeSpecialPower::friend_newModuleData(INI* ini)
{
	return parsePowerData<PlayerUpgradeSpecialPowerModuleData>(ini, "PlayerUpgradeSpecialPower requires a defined SpecialPowerTemplate");
}

void PlayerUpgradeSpecialPower::onScienceUnlocked()
{
	if (getObject() == getObject()->getControllingPlayer()->getRetailSpellBook())
		doSpecialPower(0);
}

const std::vector<AsciiString>& PlayerUpgradeSpecialPower::getGrantedUpgradeNames() const
{
	return static_cast<const PlayerUpgradeSpecialPowerModuleData*>(getModuleData())->m_upgrades;
}

void PlayerUpgradeSpecialPower::applyPowerEffect(const Coord3D*)
{
	Player* player = getObject()->getControllingPlayer();
	for (const AsciiString& name : getGrantedUpgradeNames())
	{
		const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(name);
		if (upgrade && upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER && !player->hasUpgradeComplete(upgrade))
			player->addUpgrade(upgrade, UPGRADE_STATUS_COMPLETE);
	}
}

void CloudBreakSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	SpecialPowerModuleData::buildFieldParse(fields);
	static const FieldParse table[] =
	{
		{ "SunbeamObject", INI::parseAsciiString, nullptr, offsetof(CloudBreakSpecialPowerModuleData, m_sunbeam) },
		{ "ObjectSpacing", INI::parseInt, nullptr, offsetof(CloudBreakSpecialPowerModuleData, m_spacing) },
		{ "AntiFX", INI::parseFXList, nullptr, offsetof(CloudBreakSpecialPowerModuleData, m_antiFX) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(table);
}

ModuleData* CloudBreakSpecialPower::friend_newModuleData(INI* ini)
{
	return parsePowerData<CloudBreakSpecialPowerModuleData>(ini, "CloudBreakSpecialPower requires a defined SpecialPowerTemplate");
}

void CloudBreakSpecialPower::applyPowerEffect(const Coord3D*)
{
	const auto* data = static_cast<const CloudBreakSpecialPowerModuleData*>(getModuleData());
	Region3D bounds;
	TheTerrainLogic->getExtent(&bounds);
	if (data->m_antiFX)
	{
		Coord3D center{(bounds.lo.x + bounds.hi.x) * 0.5f, (bounds.lo.y + bounds.hi.y) * 0.5f, 0.0f};
		center.z = TheTerrainLogic->getGroundHeight(center.x, center.y);
		FXList::doFXPos(data->m_antiFX, &center);
	}

	const ThingTemplate* sunbeam = data->m_sunbeam.isNotEmpty() ? TheThingFactory->findTemplate(data->m_sunbeam, FALSE) : nullptr;
	if (!sunbeam || data->m_spacing <= 0)
		return;

	const Int spacing = data->m_spacing;
	for (Int y = static_cast<Int>(bounds.lo.y + spacing); y < bounds.hi.y - spacing; y += spacing)
	{
		for (Int x = static_cast<Int>(bounds.lo.x + spacing); x < bounds.hi.x - spacing; x += spacing)
		{
			Coord3D point{static_cast<Real>(x), static_cast<Real>(y), 0.0f};
			point.z = TheTerrainLogic->getGroundHeight(point.x, point.y);
			if (Object* beam = TheThingFactory->newObject(sunbeam, getObject()->getTeam()))
				beam->setPosition(&point);
		}
	}
}

void TaintSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	SpecialPowerModuleData::buildFieldParse(fields);
	static const FieldParse table[] =
	{
		{ "TaintObject", INI::parseAsciiString, nullptr, offsetof(TaintSpecialPowerModuleData, m_land) },
		{ "TaintRadius", INI::parseReal, nullptr, offsetof(TaintSpecialPowerModuleData, m_radius) },
		{ "TaintFX", INI::parseFXList, nullptr, offsetof(TaintSpecialPowerModuleData, m_fx) },
		{ "TaintOCL", INI::parseObjectCreationList, nullptr, offsetof(TaintSpecialPowerModuleData, m_ocl) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(table);
}

ModuleData* TaintSpecialPower::friend_newModuleData(INI* ini)
{
	return parsePowerData<TaintSpecialPowerModuleData>(ini, "TaintSpecialPower requires a defined SpecialPowerTemplate");
}

void TaintSpecialPower::applyPowerEffect(const Coord3D* position)
{
	const auto* data = static_cast<const TaintSpecialPowerModuleData*>(getModuleData());
	if (!position || data->m_land.isEmpty() || data->m_radius < 0.0f)
		return;

	const ThingTemplate* land = TheThingFactory->findTemplate(data->m_land, FALSE);
	if (!land)
		return;

	const Coord3D point = *position;
	if (data->m_fx)
		FXList::doFXPos(data->m_fx, &point);
	ObjectCreationList::create(data->m_ocl, getObject(), &point, &point, INVALID_ANGLE);
	if (Object* patch = TheThingFactory->newObject(land, getObject()->getControllingPlayer()->getDefaultTeam()))
		patch->setPosition(&point);
}

Bool UntamedAllegianceSpecialPower::isValidPowerTarget(const Object* target) const
{
	const Object* caster = getObject();
	const auto& filter = getPowerModuleData()->m_attributeModifierAffects;
	return target && target != caster && !target->isEffectivelyDead() && !target->isDestroyed() &&
		target->getControllingPlayer() != caster->getControllingPlayer() && filter.matchesObject(caster, target);
}

void UntamedAllegianceSpecialPower::applyPowerEffect(const Coord3D* position)
{
	if (!position)
		return;
	Object* caster = getObject();
	Team* team = caster->getControllingPlayer()->getDefaultTeam();
	const Real radius = getPowerModuleData()->m_attributeModifierRange;
	std::vector<ObjectID> targets;
	{
		SimpleObjectIterator* objects = ThePartitionManager->iterateObjectsInRange(position, radius, FROM_CENTER_2D);
		MemoryPoolObjectHolder hold(objects);
		for (Object* target = objects->first(); target; target = objects->next())
			if (isValidPowerTarget(target))
				targets.push_back(target->getID());
	}
	std::sort(targets.begin(), targets.end());
	for (ObjectID id : targets)
	{
		Object* target = TheGameLogic->findObjectByID(id);
		if (!isValidPowerTarget(target))
			continue;
		target->defect(team, 0);
		if (target->getControllingPlayer() == team->getControllingPlayer())
			target->updateUpgradeModules();
	}
}

void MonitorConditionUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "WeaponSetFlags", WeaponSetFlags::parseFromINI, nullptr, offsetof(MonitorConditionUpdateModuleData, m_weaponSetFlags) },
		{ "WeaponToggleCommandSet", INI::parseAsciiString, nullptr, offsetof(MonitorConditionUpdateModuleData, m_weaponToggleCommandSet) },
		{ "ModelConditionFlags", ModelConditionFlags::parseFromINI, nullptr, offsetof(MonitorConditionUpdateModuleData, m_modelConditionFlags) },
		{ "ModelConditionCommandSet", INI::parseAsciiString, nullptr, offsetof(MonitorConditionUpdateModuleData, m_modelConditionCommandSet) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

MonitorConditionUpdate::MonitorConditionUpdate(Thing* thing, const ModuleData* moduleData)
	: UpdateModule(thing, moduleData)
{
}

const AsciiString& MonitorConditionUpdate::getActiveCommandSet() const
{
	const MonitorConditionUpdateModuleData* data = getMonitorConditionUpdateModuleData();
	const Object* self = getObject();
	ModelConditionFlags conditions = self->getLogicModelConditions();
	const AIUpdateInterface* ai = self->getAI();
	if (ai && ai->getAIStateType() == AI_ATTACK_POSITION)
		conditions.set(MODELCONDITION_ATTACKING_POSITION);
	if (data->m_modelConditionCommandSet.isNotEmpty() && data->m_modelConditionFlags.any() &&
		conditions.testForAll(data->m_modelConditionFlags))
		return data->m_modelConditionCommandSet;
	if (data->m_weaponToggleCommandSet.isNotEmpty() && data->m_weaponSetFlags.any() &&
		self->getWeaponSetFlags().testForAll(data->m_weaponSetFlags))
		return data->m_weaponToggleCommandSet;
	return AsciiString::TheEmptyString;
}

UpdateSleepTime MonitorConditionUpdate::update()
{
	const AsciiString& current = getObject()->getCommandSetString();
	if (current != m_lastCommandSet)
	{
		m_lastCommandSet = current;
		const Drawable* draw = getObject()->getDrawable();
		if (TheControlBar && draw && draw->isSelected())
			TheControlBar->markUIDirty();
	}
	return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND / 5);
}

void MonitorConditionUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
}

void MonitorConditionUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 2;
	xfer->xferVersion(&version, 2);
	UpdateModule::xfer(xfer);
	if (version == 1)
	{
		xfer->xferAsciiString(&m_savedWeaponSet);
		xfer->xferAsciiString(&m_savedModelSet);
	}
}

void MonitorConditionUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	Object* self = getObject();
	const MonitorConditionUpdateModuleData* data = getMonitorConditionUpdateModuleData();
	if (m_savedModelSet.isNotEmpty() && self->getBaseCommandSetString() == data->m_modelConditionCommandSet)
		self->setCommandSetStringOverride(m_savedModelSet);
	if (m_savedWeaponSet.isNotEmpty() && self->getBaseCommandSetString() == data->m_weaponToggleCommandSet)
		self->setCommandSetStringOverride(m_savedWeaponSet);
}

void AutoAbilityBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "SpecialAbility", INI::parseSpecialPowerTemplate, nullptr, offsetof(AutoAbilityBehaviorModuleData, m_specialAbility) },
		{ "Query", parseQuery, nullptr, offsetof(AutoAbilityBehaviorModuleData, m_queries) },
		{ "ForbiddenStatus", parseStatusNames, nullptr, offsetof(AutoAbilityBehaviorModuleData, m_forbiddenStatus) },
		{ "MaxScanRange", INI::parseReal, nullptr, offsetof(AutoAbilityBehaviorModuleData, m_maxScanRange) },
		{ "MinScanRange", INI::parseReal, nullptr, offsetof(AutoAbilityBehaviorModuleData, m_minScanRange) },
		{ "IdleTimeSeconds", parseIdleSeconds, nullptr, offsetof(AutoAbilityBehaviorModuleData, m_idleFrames) },
		{ "StartsActive", INI::parseBool, nullptr, offsetof(AutoAbilityBehaviorModuleData, m_startsActive) },
		{ "AllowSelf", INI::parseBool, nullptr, offsetof(AutoAbilityBehaviorModuleData, m_allowSelf) },
		{ "AdjustAttackMeleePosition", INI::parseBool, nullptr, offsetof(AutoAbilityBehaviorModuleData, m_adjustAttackMeleePosition) },
		{ "BaseMaxRangeFromStartPos", INI::parseBool, nullptr, offsetof(AutoAbilityBehaviorModuleData, m_baseMaxRangeFromStartPos) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

void AutoAbilityBehaviorModuleData::parseQuery(INI* ini, void*, void* store, const void*)
{
	AutoAbilityQuery query;
	query.m_count = INI::scanInt(ini->getNextToken());
	query.m_filter.addTokens(ini, ini->getNextToken());
	static_cast<std::vector<AutoAbilityQuery>*>(store)->push_back(std::move(query));
}

void AutoAbilityBehaviorModuleData::parseStatusNames(INI* ini, void*, void* store, const void*)
{
	ObjectStatusMaskType& mask = *static_cast<ObjectStatusMaskType*>(store);
	for (const char* token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
	{
		const Int bit = ObjectStatusMaskType::getSingleBitFromName(token);
		if (bit >= 0)
			mask.set(bit);
		else if (INIAudit::isEnabled())
			INIAudit::record("field-error", std::string("ForbiddenStatus = ") + token, ini->getFilename().str(), ini->getLineNum());
	}
}

void AutoAbilityBehaviorModuleData::parseIdleSeconds(INI* ini, void*, void* store, const void*)
{
	*static_cast<UnsignedInt*>(store) = SpecialAbilityUpdate::secondsToFrames(INI::scanReal(ini->getNextToken()));
}

AutoAbilityBehavior::AutoAbilityBehavior(Thing* thing, const ModuleData* moduleData)
	: UpdateModule(thing, moduleData)
{
	setWakeFrame(getObject(), UPDATE_SLEEP(AUTO_ABILITY_SCAN_FRAMES));
}

const SpecialPowerTemplate* AutoAbilityBehavior::getSpecialAbility() const
{
	return getAutoAbilityBehaviorModuleData()->m_specialAbility;
}

Bool AutoAbilityBehavior::isAutoActive() const
{
	const Player* owner = getObject()->getControllingPlayer();
	return AbilityRules::autoAbilityActive(getAutoAbilityBehaviorModuleData()->m_startsActive,
		owner && owner->getPlayerType() == PLAYER_COMPUTER, m_toggledState);
}

AutoAbilityBehavior* AutoAbilityBehavior::find(Object* obj, const SpecialPowerTemplate* power)
{
	static const NameKeyType key = NAMEKEY("AutoAbilityBehavior");
	for (BehaviorModule** module = obj->getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() != key)
			continue;
		AutoAbilityBehavior* behavior = static_cast<AutoAbilityBehavior*>(*module);
		if (behavior->getSpecialAbility() == power)
			return behavior;
	}
	return nullptr;
}

Bool AutoAbilityBehavior::objectMatches(const SpecialPowerObjectFilter& filter, const Object* other) const
{
	const Object* self = getObject();
	if (other->isEffectivelyDead() || other->isDestroyed())
		return FALSE;
	const Player* player = self->getControllingPlayer();
	if (!player || other->getShroudedStatus(player->getPlayerIndex()) >= OBJECTSHROUD_FOGGED)
		return FALSE;
	if (self->getRelationship(other) != ALLIES &&
		other->testStatus(OBJECT_STATUS_STEALTHED) && !other->testStatus(OBJECT_STATUS_DETECTED) &&
		!other->testStatus(OBJECT_STATUS_DISGUISED))
		return FALSE;

	AbilityRules::RelationFilter relations;
	relations.self = hasRule(filter, RULE_SELF);
	relations.allies = hasRule(filter, RULE_ALLIES);
	relations.enemies = hasRule(filter, RULE_ENEMIES);
	relations.neutrals = hasRule(filter, RULE_NEUTRALS);
	relations.samePlayer = hasRule(filter, RULE_SAME_PLAYER);
	AbilityRules::Relation relation = AbilityRules::Relation::Neutral;
	if (other == self)
		relation = AbilityRules::Relation::Self;
	else if (other->getControllingPlayer() == self->getControllingPlayer())
		relation = AbilityRules::Relation::SamePlayer;
	else if (self->getRelationship(other) == ALLIES)
		relation = AbilityRules::Relation::Ally;
	else if (self->getRelationship(other) == ENEMIES)
		relation = AbilityRules::Relation::Enemy;
	if (!AbilityRules::relationAllows(relations, relation))
		return FALSE;

	AbilityRules::QueryCandidate candidate;
	candidate.excluded = other->getTemplate()->isAnyKindOf(filter.m_excludeKinds) || namedIn(filter.m_excludeObjects, other);
	candidate.nameIncluded = namedIn(filter.m_includeObjects, other);
	candidate.hasIncludeNames = !filter.m_includeObjects.empty();
	for (Int bit = 0; bit < filter.m_includeKinds.size(); ++bit)
	{
		if (!filter.m_includeKinds.test(bit))
			continue;
		++candidate.includeKinds;
		if (other->isKindOf((KindOfType)bit))
			++candidate.includeKindsHit;
	}
	AbilityRules::QueryMode mode = AbilityRules::QueryMode::Any;
	if (hasRule(filter, RULE_ALL))
		mode = AbilityRules::QueryMode::All;
	else if (hasRule(filter, RULE_NONE))
		mode = AbilityRules::QueryMode::None;
	return AbilityRules::queryIncludes(mode, candidate);
}

Bool AutoAbilityBehavior::queryMatches(const AutoAbilityQuery& query, UnsignedInt options, Object*& target)
{
	const AutoAbilityBehaviorModuleData* data = getAutoAbilityBehaviorModuleData();
	Object* self = getObject();
	const SpecialPowerTemplate* power = getSpecialAbility();
	const Bool needsObject = (options & COMMAND_OPTION_NEED_OBJECT_TARGET) != 0;
	const Bool needsPosition = (options & NEED_TARGET_POS) != 0;
	const Bool checkQueryTargetPlacement = needsPosition &&
		power->getName().compareNoCase("SpecialAbilityCallFromTheDeep") != 0;
	const auto eligible = [&](Object* candidate) -> Bool
	{
		if (needsObject)
			return TheActionManager->canDoSpecialPowerAtObject(self, candidate, CMD_FROM_AI, power, options);
		if (checkQueryTargetPlacement)
			return TheActionManager->canDoSpecialPowerAtLocation(self, candidate->getPosition(), CMD_FROM_AI,
				power, candidate, options);
		return TRUE;
	};
	Int count = 0;
	Object* first = nullptr;
	if (data->m_allowSelf && objectMatches(query.m_filter, self))
	{
		if (eligible(self))
			first = self;
		++count;
	}
	if (count >= query.m_count)
	{
		if (!first && needsPosition && !needsObject &&
			TheActionManager->canDoSpecialPowerAtLocation(self, self->getPosition(), CMD_FROM_AI, power, nullptr, options))
			first = self;
		if (first || (!needsObject && !needsPosition))
		{
			target = first;
			return TRUE;
		}
	}
	const Real minSquared = data->m_minScanRange * data->m_minScanRange;
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(self, data->m_maxScanRange, FROM_BOUNDINGSPHERE_2D,
		nullptr, ITER_SORTED_NEAR_TO_FAR);
	MemoryPoolObjectHolder hold(iter);
	for (Object* other = iter->first(); other; other = iter->next())
	{
		if (other == self)
			continue;
		if (minSquared > 0.0f && ThePartitionManager->getDistanceSquared(self, other, FROM_BOUNDINGSPHERE_2D) < minSquared)
			continue;
		if (!objectMatches(query.m_filter, other))
			continue;
		if (!first && eligible(other))
			first = other;
		if (++count >= query.m_count && first)
		{
			target = first;
			return TRUE;
		}
	}
	return FALSE;
}

Bool AutoAbilityBehavior::cast(UnsignedInt options, Object* target)
{
	Object* self = getObject();
	const SpecialPowerTemplate* power = getSpecialAbility();
	if (options & COMMAND_OPTION_NEED_OBJECT_TARGET)
	{
		self->doSpecialPowerAtObject(power, target, options);
	}
	else if (options & NEED_TARGET_POS)
	{
		const Coord3D position = *target->getPosition();
		self->doSpecialPowerAtLocation(power, &position, INVALID_ANGLE, options);
	}
	else
	{
		if (!TheActionManager->canDoSpecialPower(self, power, CMD_FROM_AI, options))
			return FALSE;
		self->doSpecialPower(power, options);
	}
	return TRUE;
}

UpdateSleepTime AutoAbilityBehavior::update()
{
	const UpdateSleepTime scan = UPDATE_SLEEP(AUTO_ABILITY_SCAN_FRAMES);
	const AutoAbilityBehaviorModuleData* data = getAutoAbilityBehaviorModuleData();
	Object* self = getObject();
	if (!data->m_specialAbility || self->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	if (!isAutoActive() || self->isDisabled() || self->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) ||
		self->testStatus(OBJECT_STATUS_IS_USING_ABILITY) || self->getStatusBits().testForAny(data->m_forbiddenStatus))
		return scan;
	SpecialPowerModuleInterface* power = self->getSpecialPowerModule(data->m_specialAbility);
	if (!power || !power->isReady())
		return scan;
	const CommandButton* button = TheRetailCommandCatalog ? TheRetailCommandCatalog->findPowerButton(self, data->m_specialAbility) : nullptr;
	if (!TheSpecialPowerStore->canUseSpecialPower(self, data->m_specialAbility))
		return scan;
	const UnsignedInt options = button ? button->getOptions() : 0;
	for (BehaviorModule** module = self->getBehaviorModules(); *module; ++module)
	{
		SpecialPowerUpdateInterface* update = (*module)->getSpecialPowerUpdateInterface();
		if (update && update->isSpecialAbility() && update->isActive())
			return scan;
	}
	AIUpdateInterface* ai = self->getAI();
	if (ai && ai->isMoving() && !ai->isAttacking())
	{
		m_idleSinceFrame = 0;
		return scan;
	}
	if (data->m_idleFrames > 0)
	{
		const UnsignedInt now = TheGameLogic->getFrame();
		if (!ai || !ai->isIdle())
		{
			m_idleSinceFrame = 0;
			return scan;
		}
		if (m_idleSinceFrame == 0)
			m_idleSinceFrame = now;
		if (now - m_idleSinceFrame < data->m_idleFrames)
			return scan;
	}
	for (const AutoAbilityQuery& query : data->m_queries)
	{
		Object* target = nullptr;
		if (queryMatches(query, options, target) && cast(options, target))
			break;
	}
	return scan;
}

void AutoAbilityBehavior::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xfer->xferInt(&m_toggledState);
	xfer->xferUnsignedInt(&m_idleSinceFrame);
}

void AutoAbilityBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	xfer->xferInt(&m_toggledState);
	xfer->xferUnsignedInt(&m_idleSinceFrame);
}

void AutoAbilityBehavior::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}

void PlayerHealSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialPowerModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "HealAffects", KindOfMaskType::parseFromINI, nullptr, offsetof(PlayerHealSpecialPowerModuleData, m_healAffects) },
		{ "HealAmount", INI::parseReal, nullptr, offsetof(PlayerHealSpecialPowerModuleData, m_healAmount) },
		{ "HealRadius", INI::parseReal, nullptr, offsetof(PlayerHealSpecialPowerModuleData, m_healRadius) },
		{ "HealFX", INI::parseFXList, nullptr, offsetof(PlayerHealSpecialPowerModuleData, m_healFX) },
		{ "HealOCL", INI::parseObjectCreationList, nullptr, offsetof(PlayerHealSpecialPowerModuleData, m_healOCL) },
		{ "HealAsPercent", INI::parseBool, nullptr, offsetof(PlayerHealSpecialPowerModuleData, m_healAsPercent) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

PlayerHealSpecialPower::PlayerHealSpecialPower(Thing* thing, const ModuleData* moduleData)
	: SpecialPowerModule(thing, moduleData)
{
	m_pendingCenter.zero();
}

void PlayerHealSpecialPower::doSpecialPower(UnsignedInt commandOptions)
{
	doSpecialPowerAtLocation(getObject()->getPosition(), INVALID_ANGLE, commandOptions);
}

void PlayerHealSpecialPower::doSpecialPowerAtObject(Object* obj, UnsignedInt commandOptions)
{
	if (obj)
		doSpecialPowerAtLocation(obj->getPosition(), INVALID_ANGLE, commandOptions);
}

void PlayerHealSpecialPower::doSpecialPowerAtLocation(const Coord3D* loc, Real angle, UnsignedInt commandOptions)
{
	if (!loc || m_pausedCount > 0 || getObject()->isDisabled() || !meetsRequiredRank())
		return;
	const Coord3D center = *loc;
	m_pendingCenter = center;
	m_hasPendingCenter = TRUE;
	SpecialPowerModule::doSpecialPowerAtLocation(&center, angle, commandOptions);
	if (!getPlayerHealSpecialPowerModuleData()->m_updateModuleStartsAttack && m_hasPendingCenter)
	{
		m_hasPendingCenter = FALSE;
		heal(&center);
	}
}

void PlayerHealSpecialPower::markSpecialPowerTriggered(const Coord3D* location)
{
	SpecialPowerModule::markSpecialPowerTriggered(location);
	if (!getPlayerHealSpecialPowerModuleData()->m_updateModuleStartsAttack)
		return;
	const Coord3D center = location ? *location : (m_hasPendingCenter ? m_pendingCenter : *getObject()->getPosition());
	m_hasPendingCenter = FALSE;
	heal(&center);
}

void PlayerHealSpecialPower::heal(const Coord3D* center)
{
	const PlayerHealSpecialPowerModuleData* data = getPlayerHealSpecialPowerModuleData();
	Object* self = getObject();
	if (data->m_healOCL)
		ObjectCreationList::create(data->m_healOCL, self, center, center, INVALID_ANGLE);
	if (data->m_healRadius <= 0.0f)
	{
		healObject(self);
		return;
	}
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(center, data->m_healRadius, FROM_CENTER_2D, nullptr,
		ITER_SORTED_NEAR_TO_FAR);
	MemoryPoolObjectHolder hold(iter);
	for (Object* other = iter->first(); other; other = iter->next())
		healObject(other);
}

void PlayerHealSpecialPower::healObject(Object* other)
{
	const PlayerHealSpecialPowerModuleData* data = getPlayerHealSpecialPowerModuleData();
	Object* self = getObject();
	if (other->isEffectivelyDead() || !other->getTemplate()->isAnyKindOf(data->m_healAffects))
		return;
	const Bool own = other->getControllingPlayer() == self->getControllingPlayer();
	if (!own && !(data->m_affectAllies && self->getRelationship(other) == ALLIES))
		return;
	BodyModuleInterface* body = other->getBodyModule();
	if (!body || body->getHealth() >= body->getMaxHealth())
		return;
	other->attemptHealing(AbilityRules::healAmount(body->getMaxHealth(), data->m_healAmount, data->m_healAsPercent), self);
	FXList::doFXObj(data->m_healFX, other);
}

void PlayerHealSpecialPower::crc(Xfer* xfer)
{
	SpecialPowerModule::crc(xfer);
}

void PlayerHealSpecialPower::xfer(Xfer* xfer)
{
	const XferVersion currentVersion = RetailFileLayers::enabled() ? 2 : 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	SpecialPowerModule::xfer(xfer);
	if (version >= 2)
	{
		xfer->xferBool(&m_hasPendingCenter);
		xfer->xferCoord3D(&m_pendingCenter);
	}
}

void PlayerHealSpecialPower::loadPostProcess()
{
	SpecialPowerModule::loadPostProcess();
}

WeaponFireSpecialAbilityUpdate::~WeaponFireSpecialAbilityUpdate()
{
}

ToggleMountedSpecialAbilityUpdate::~ToggleMountedSpecialAbilityUpdate()
{
}

ActivateModuleSpecialPower::~ActivateModuleSpecialPower()
{
}

HeroModeSpecialAbilityUpdate::~HeroModeSpecialAbilityUpdate()
{
}

WeaponModeSpecialPowerUpdate::~WeaponModeSpecialPowerUpdate()
{
	deleteInstance(m_power);
}

PlayerUpgradeSpecialPower::~PlayerUpgradeSpecialPower() = default;
CloudBreakSpecialPower::~CloudBreakSpecialPower() = default;
TaintSpecialPower::~TaintSpecialPower() = default;
UntamedAllegianceSpecialPower::~UntamedAllegianceSpecialPower() = default;

void PlayerUpgradeSpecialPower::crc(Xfer* xfer) { SpecialPowerModule::crc(xfer); }
void PlayerUpgradeSpecialPower::xfer(Xfer* xfer) { SpecialPowerModule::xfer(xfer); }
void PlayerUpgradeSpecialPower::loadPostProcess() { SpecialPowerModule::loadPostProcess(); }

void CloudBreakSpecialPower::crc(Xfer* xfer) { SpecialPowerModule::crc(xfer); }
void CloudBreakSpecialPower::xfer(Xfer* xfer) { SpecialPowerModule::xfer(xfer); }
void CloudBreakSpecialPower::loadPostProcess() { SpecialPowerModule::loadPostProcess(); }

void TaintSpecialPower::crc(Xfer* xfer) { SpecialPowerModule::crc(xfer); }
void TaintSpecialPower::xfer(Xfer* xfer) { SpecialPowerModule::xfer(xfer); }
void TaintSpecialPower::loadPostProcess() { SpecialPowerModule::loadPostProcess(); }

void UntamedAllegianceSpecialPower::crc(Xfer* xfer) { SpecialPowerModule::crc(xfer); }
void UntamedAllegianceSpecialPower::xfer(Xfer* xfer) { SpecialPowerModule::xfer(xfer); }
void UntamedAllegianceSpecialPower::loadPostProcess() { SpecialPowerModule::loadPostProcess(); }

MonitorConditionUpdate::~MonitorConditionUpdate()
{
}

AutoAbilityBehavior::~AutoAbilityBehavior()
{
}

PlayerHealSpecialPower::~PlayerHealSpecialPower()
{
}
