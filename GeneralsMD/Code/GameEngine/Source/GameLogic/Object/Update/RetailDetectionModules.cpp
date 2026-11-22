#include "PreRTS.h"

#include "GameLogic/Module/RetailDetectionModules.h"

#include "Common/Xfer.h"
#include "Common/Player.h"
#include "Common/AudioEventRTS.h"
#include "Common/GameAudio.h"
#include "GameClient/FXList.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"

#include "Common/Upgrade.h"
#include "GameLogic/TerrainLogic.h"

#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"

SpecialEnemySenseUpdate::~SpecialEnemySenseUpdate() = default;

void SpecialEnemySenseUpdateModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	UpdateModuleData::buildFieldParse(fields);
	static const FieldParse table[] =
	{
		{ "SpecialEnemyFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(SpecialEnemySenseUpdateModuleData, m_filter) },
		{ "ScanRange", INI::parseReal, nullptr, offsetof(SpecialEnemySenseUpdateModuleData, m_range) },
		{ "ScanInterval", INI::parseDurationUnsignedInt, nullptr, offsetof(SpecialEnemySenseUpdateModuleData, m_interval) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(table);
}

UpdateSleepTime SpecialEnemySenseUpdate::update()
{
	Object* self = getObject();
	const auto* data = getSpecialEnemySenseUpdateModuleData();
	Bool found = FALSE;
	if (!self->isEffectivelyDead() && data->m_range > 0.0f)
	{
		SimpleObjectIterator* nearby = ThePartitionManager->iterateObjectsInRange(self, data->m_range, FROM_CENTER_2D);
		MemoryPoolObjectHolder hold(nearby);
		for (Object* candidate = nearby->first(); candidate; candidate = nearby->next())
		{
			if (!candidate->isEffectivelyDead() && !candidate->isContained() &&
				self->getRelationship(candidate) == ENEMIES && data->m_filter.matchesObject(self, candidate))
			{
				found = TRUE;
				break;
			}
		}
	}

	const auto apply = [found](Object* object)
	{
		if (found)
			object->setModelConditionState(MODELCONDITION_SPECIAL_ENEMY_NEAR);
		else
			object->clearModelConditionState(MODELCONDITION_SPECIAL_ENEMY_NEAR);
	};
	apply(self);
	if (self->isKindOf(KINDOF_HORDE) && self->getContain())
		if (const auto* members = self->getContain()->getContainedItemsList())
			for (Object* member : *members)
				apply(member);
	return UPDATE_SLEEP(std::max(1u, data->m_interval));
}

void SpecialEnemySenseUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
}

void SpecialEnemySenseUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
}

void SpecialEnemySenseUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}

Bool RetailDetection::nearTrees(const Coord3D& position)
{
	constexpr Real range = 50.0f;
	if (TheTerrainLogic->isNearMapTree(position, range))
		return TRUE;
	SimpleObjectIterator* nearby = ThePartitionManager->iterateObjectsInRange(&position, range, FROM_CENTER_2D);
	MemoryPoolObjectHolder hold(nearby);
	for (Object* other = nearby->first(); other; other = nearby->next())
		if (!other->isEffectivelyDead() && other->isKindOf(KINDOF_TREE))
			return TRUE;
	return FALSE;
}

Bool RetailDetection::nearTrees(const Object* object)
{
	return nearTrees(*object->getPosition());
}

Bool RetailDetection::hasUpgrade(const Object* object, const AsciiString& name)
{
	const auto* upgrade = TheUpgradeCenter->findUpgrade(name);
	if (!upgrade)
		return FALSE;
	const auto* player = object->getControllingPlayer();
	return upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER ? player && player->hasUpgradeComplete(upgrade) : object->hasUpgrade(upgrade);
}

Bool RetailDetection::hasNearbyObserver(const Object* object, Real range, Bool friendliesOnly)
{
	SimpleObjectIterator* nearby = ThePartitionManager->iterateObjectsInRange(object, range, FROM_CENTER_2D);
	MemoryPoolObjectHolder hold(nearby);
	for (Object* other = nearby->first(); other; other = nearby->next())
	{
		if (object == other || other->isEffectivelyDead() || (other->isContained() && !other->testStatus(OBJECT_STATUS_HORDE_MEMBER)))
			continue;
		const Relationship relationship = object->getRelationship(other);
		if ((friendliesOnly ? relationship == ALLIES : relationship == ENEMIES) &&
			(other->isKindOf(KINDOF_INFANTRY) || other->isKindOf(KINDOF_VEHICLE) ||
				other->isKindOf(KINDOF_MONSTER) || other->isKindOf(KINDOF_HORDE) ||
				other->isKindOf(KINDOF_CAVALRY) || other->isKindOf(KINDOF_STRUCTURE)))
			return TRUE;
	}
	return FALSE;
}

void RetailInvisibilityNugget::parse(INI* ini, void*, void* store, const void*)
{
	static const char* const types[] = { "CAMOUFLAGE", "STEALTH", nullptr };
	static const char* const options[] = { "DETECTED_BY_FRIENDLIES", "UNTOGGLE_HIDDEN_WHEN_LEAVING_STEALTH", "ALLOW_NEAR_TREES", nullptr };
	const auto forbidden = [](INI* input, void* instance, void*, const void*)
	{
		auto& nugget = *static_cast<RetailInvisibilityNugget*>(instance);
		nugget.m_forbiddenModels.clear();
		nugget.m_awayFromTrees = nugget.m_takingDamage = nugget.m_usingAbility = nugget.m_moving = nugget.m_firingAny = FALSE;
		for (const char* token = input->getNextToken(); token; token = input->getNextTokenOrNull())
		{
			if (stricmp(token, "NONE") == 0)
				break;
			if (stricmp(token, "AWAY_FROM_TREES") == 0)
				nugget.m_awayFromTrees = TRUE;
			else if (stricmp(token, "TAKING_DAMAGE") == 0)
				nugget.m_takingDamage = TRUE;
			else if (stricmp(token, "USING_ABILITY") == 0)
				nugget.m_usingAbility = TRUE;
			else if (stricmp(token, "MOVING") == 0)
				nugget.m_moving = TRUE;
			else if (stricmp(token, "FIRING_ANY") == 0)
				nugget.m_firingAny = TRUE;
			else
				nugget.m_forbiddenModels.set(INI::scanIndexList(token, ModelConditionFlags::getBitNames()));
		}
	};
	const FieldParse table[] =
	{
		{ "InvisibilityType", INI::parseIndexList, types, offsetof(RetailInvisibilityNugget, m_type) },
		{ "DetectionRange", INI::parseReal, nullptr, offsetof(RetailInvisibilityNugget, m_detectionRange) },
		{ "Options", INI::parseBitString32, options, offsetof(RetailInvisibilityNugget, m_options) },
		{ "ForbiddenConditions", forbidden, nullptr, 0 },
		{ "ForbiddenWeaponConditions", WeaponSetFlags::parseFromINI, nullptr, offsetof(RetailInvisibilityNugget, m_forbiddenWeapons) },
		{ "HintDetectableConditions", ObjectStatusMaskType::parseFromINI, nullptr, offsetof(RetailInvisibilityNugget, m_hintDetected) },
		{ "BecomeStealthedFX", INI::parseFXList, nullptr, offsetof(RetailInvisibilityNugget, m_enterFX) },
		{ "ExitStealthFX", INI::parseFXList, nullptr, offsetof(RetailInvisibilityNugget, m_exitFX) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(store, table);
}

Bool RetailInvisibilityNugget::canConceal(const Object* object) const
{
	if (object->isEffectivelyDead() || object->testScriptStatusBit(OBJECT_STATUS_SCRIPT_UNSTEALTHED))
		return FALSE;
	Bool blocked = object->getLogicModelConditions().testForAny(m_forbiddenModels) ||
		object->getWeaponSetFlags().testForAny(m_forbiddenWeapons) ||
		(m_usingAbility && object->testStatus(OBJECT_STATUS_IS_USING_ABILITY)) ||
		(m_firingAny && object->testStatus(OBJECT_STATUS_IS_FIRING_WEAPON)) ||
		(m_moving && object->getAI() && object->getAI()->isMoving());
	if (!blocked && m_takingDamage && object->getBodyModule())
	{
		const auto* damage = object->getBodyModule()->getLastDamageTimestamp();
		const auto frame = TheGameLogic->getFrame();
		blocked = damage && *damage <= frame && frame - *damage <= 1;
	}
	if (m_awayFromTrees || (blocked && (m_options & ALLOW_NEAR_TREES)))
	{
		const Bool near = RetailDetection::nearTrees(object);
		if (m_awayFromTrees && !near)
			return FALSE;
		if (near && (m_options & ALLOW_NEAR_TREES))
			return TRUE;
	}
	return !blocked;
}

void InvisibilityUpdateModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	UpdateModuleData::buildFieldParse(fields);
	static const FieldParse table[] =
	{
		{ "InvisibilityNugget", RetailInvisibilityNugget::parse, nullptr, offsetof(InvisibilityUpdateModuleData, m_nugget) },
		{ "StartsActive", INI::parseBool, nullptr, offsetof(InvisibilityUpdateModuleData, m_startsActive) },
		{ "UpdatePeriod", INI::parseDurationUnsignedInt, nullptr, offsetof(InvisibilityUpdateModuleData, m_period) },
		{ "RequiredUpgrades", INI::parseAsciiStringVector, nullptr, offsetof(InvisibilityUpdateModuleData, m_required) },
		{ "ForbiddenUpgrades", INI::parseAsciiStringVector, nullptr, offsetof(InvisibilityUpdateModuleData, m_forbidden) },
		{ "Broadcast", INI::parseBool, nullptr, offsetof(InvisibilityUpdateModuleData, m_broadcast) },
		{ "BroadcastRange", INI::parseReal, nullptr, offsetof(InvisibilityUpdateModuleData, m_range) },
		{ "BroadcastObjectFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(InvisibilityUpdateModuleData, m_filter) },
		{ "UnitSpecificSoundNameToUseAsVoiceMoveToStealthyArea", INI::parseAsciiString, nullptr, offsetof(InvisibilityUpdateModuleData, m_moveVoice) },
		{ "UnitSpecificSoundNameToUseAsVoiceEnterStateMoveToStealthyArea", INI::parseAsciiString, nullptr, offsetof(InvisibilityUpdateModuleData, m_arrivalVoice) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(table);
}

InvisibilityUpdate::InvisibilityUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data)
{
	m_active = getInvisibilityUpdateModuleData()->m_startsActive;
}

InvisibilityUpdate::~InvisibilityUpdate() = default;

InvisibilityUpdate* InvisibilityUpdate::find(const Object* object, NameKeyType tag)
{
	const auto key = NAMEKEY("InvisibilityUpdate");
	for (BehaviorModule** module = object->getBehaviorModules(); *module; ++module)
		if ((*module)->getModuleNameKey() == key && (tag == NAMEKEY_INVALID || (*module)->getModuleTagNameKey() == tag))
			return static_cast<InvisibilityUpdate*>(*module);
	return nullptr;
}

Bool InvisibilityUpdate::isAvailable() const
{
	const auto* data = getInvisibilityUpdateModuleData();
	const auto has = [this](const AsciiString& name) { return RetailDetection::hasUpgrade(getObject(), name); };
	return std::all_of(data->m_required.begin(), data->m_required.end(), has) &&
		std::none_of(data->m_forbidden.begin(), data->m_forbidden.end(), has);
}

Bool InvisibilityUpdate::isEnabled() const
{
	return m_active && !getObject()->isEffectivelyDead() && (!m_end || TheGameLogic->getFrame() < m_end) && isAvailable();
}

const RetailInvisibilityNugget& InvisibilityUpdate::getNugget() const
{
	return getInvisibilityUpdateModuleData()->m_nugget;
}

void InvisibilityUpdate::deactivate()
{
	m_active = FALSE;
	m_end = 0;
	getObject()->clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_HIDDEN));
	getObject()->clearModelConditionState(MODELCONDITION_HIDDEN);
}

void InvisibilityUpdate::toggle(UnsignedInt duration)
{
	if (m_active)
		deactivate();
	else
	{
		const auto frame = TheGameLogic->getFrame();
		m_active = TRUE;
		m_nextPulse = frame;
		m_end = duration ? frame + duration : 0;
		getObject()->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_HIDDEN));
		getObject()->setModelConditionState(MODELCONDITION_HIDDEN);
	}
	setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
}

UpdateSleepTime InvisibilityUpdate::update()
{
	const UnsignedInt frame = TheGameLogic->getFrame();
	const auto* data = getInvisibilityUpdateModuleData();
	Object* self = getObject();
	if (m_end && frame >= m_end)
		deactivate();

	const Bool enabled = isEnabled();
	if (!data->m_broadcast)
	{
		const Bool concealed = enabled && data->m_nugget.canConceal(self);
		if (concealed != m_wasConcealed)
		{
			if (const auto* fx = concealed ? data->m_nugget.m_enterFX : data->m_nugget.m_exitFX)
				FXList::doFXObj(fx, self);
			if (!concealed && (data->m_nugget.m_options & RetailInvisibilityNugget::UNTOGGLE_HIDDEN_WHEN_LEAVING_STEALTH) &&
				!data->m_startsActive)
				deactivate();
			m_wasConcealed = concealed;
		}
	}

	const UnsignedInt period = std::max(1u, data->m_period);
	if (enabled && m_active && frame >= m_nextPulse)
	{
		const AsciiString tag = TheNameKeyGenerator->keyToName(getModuleTagNameKey());
		const auto apply = [&](Object* object)
		{
			object->offerRetailInvisibility(self->getID(), tag, frame + period + 1);
			if (object->isKindOf(KINDOF_HORDE) && object->getContain())
				if (const auto* members = object->getContain()->getContainedItemsList())
					for (Object* member : *members)
						member->offerRetailInvisibility(self->getID(), tag, frame + period + 1);
		};
		if (data->m_broadcast)
		{
			SimpleObjectIterator* nearby = ThePartitionManager->iterateObjectsInRange(self, data->m_range, FROM_CENTER_2D);
			MemoryPoolObjectHolder hold(nearby);
			for (Object* object = nearby->first(); object; object = nearby->next())
				if (!object->isEffectivelyDead() && !object->isContained() && data->m_filter.matchesObject(self, object))
					apply(object);
		}
		else
			apply(self);
		m_nextPulse = frame + period;
	}

	if (enabled && self->isLocallyControlled() && self->getAI() &&
		(!data->m_moveVoice.isEmpty() || !data->m_arrivalVoice.isEmpty()))
	{
		const Bool near = RetailDetection::nearTrees(self);
		const Bool moving = self->getAI()->isMoving();
		const Coord3D* goal = moving ? self->getAI()->getGoalPosition() : nullptr;
		const Bool movingToTrees = !near && goal && RetailDetection::nearTrees(*goal);
		const AsciiString* voice = nullptr;
		if (near && !m_wasNearTrees && moving)
			voice = &data->m_arrivalVoice;
		else if (movingToTrees && !m_wasMovingToTrees)
			voice = &data->m_moveVoice;
		if (voice && !voice->isEmpty())
		{
			AudioEventRTS event(*voice);
			event.setObjectID(self->getID());
			TheAudio->addAudioEvent(&event);
		}
		m_wasNearTrees = near;
		m_wasMovingToTrees = movingToTrees;
	}
	return !m_active ? UPDATE_SLEEP_FOREVER : data->m_broadcast ? UPDATE_SLEEP(period) : UPDATE_SLEEP_NONE;
}

void InvisibilityUpdate::transferState(Xfer* xfer)
{
	xfer->xferUnsignedInt(&m_end);
	xfer->xferUnsignedInt(&m_nextPulse);
	xfer->xferBool(&m_active);
	xfer->xferBool(&m_wasConcealed);
}

void InvisibilityUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	transferState(xfer);
}

void InvisibilityUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	transferState(xfer);
}

void InvisibilityUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}

void ToggleHiddenSpecialAbilityUpdateModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(fields);
	static const FieldParse table[] =
	{
		{ "ShowPalantirTimer", INI::parseBool, nullptr, offsetof(ToggleHiddenSpecialAbilityUpdateModuleData, m_showTimer) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(table);
}

ToggleHiddenSpecialAbilityUpdate::~ToggleHiddenSpecialAbilityUpdate() = default;

Bool ToggleHiddenSpecialAbilityUpdate::acceptsIntent(const Object*, const Coord3D*) const
{
	const auto* invisibility = InvisibilityUpdate::find(getObject());
	return invisibility && invisibility->isAvailable();
}

void ToggleHiddenSpecialAbilityUpdate::triggerAbilityEffect()
{
	if (auto* invisibility = InvisibilityUpdate::find(getObject()))
		invisibility->toggle(getToggleHiddenSpecialAbilityUpdateModuleData()->m_effectDuration);
	SpecialAbilityUpdate::triggerAbilityEffect();
}

void ToggleHiddenSpecialAbilityUpdate::crc(Xfer* xfer)
{
	SpecialAbilityUpdate::crc(xfer);
}

void ToggleHiddenSpecialAbilityUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	SpecialAbilityUpdate::xfer(xfer);
}

void ToggleHiddenSpecialAbilityUpdate::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}

Real InvisibilityUpdate::timerProgress(UnsignedInt duration) const
{
	if (!duration || !m_end || !isEnabled())
		return -1.0f;
	const auto remaining = std::min(duration, m_end - TheGameLogic->getFrame());
	return static_cast<Real>(duration - remaining) / duration;
}

Real ToggleHiddenSpecialAbilityUpdate::timerProgress(const Object* object, const SpecialPowerTemplate* power)
{
	const auto key = NAMEKEY("ToggleHiddenSpecialAbilityUpdate");
	for (BehaviorModule** module = object->getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() != key)
			continue;
		const auto* toggle = static_cast<const ToggleHiddenSpecialAbilityUpdate*>(*module);
		const auto* data = toggle->getToggleHiddenSpecialAbilityUpdateModuleData();
		if (data->m_specialPowerTemplate == power && data->m_showTimer)
			if (const auto* invisibility = InvisibilityUpdate::find(object))
				return invisibility->timerProgress(data->m_effectDuration);
	}
	return -1.0f;
}
