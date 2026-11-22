#include "PreRTS.h"

#include "GameLogic/Module/RotwkModifierModules.h"

#include <cstdio>

#include "Common/AbilityRules.h"
#include "Common/AttributeModifierRules.h"
#include "Common/GameAudio.h"
#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameClient/RetailEva.h"
#include "GameLogic/AttributeModifierStore.h"
#include "GameLogic/Damage.h"
#include "GameLogic/ExperienceLevelStore.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LogicRandomValue.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/StealthUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/RetailBuildingEvents.h"
#include "GameLogic/RetailTestCommands.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/WeaponNugget.h"
#include "WWMath/wwmath.h"

#include <algorithm>

namespace
{
const char* const emotionTypes[] = {
	"TAUNT", "CHEER", "HERO_CHEER", "POINT", "FEAR", "UNCONTROLLABLE_FEAR", "TERROR", "DOOM",
	"ALERT", "QUARRELSOME", "OVERRIDE", "CHEER_FOR_ABOUT_TO_CRUSH", "BRACE_FOR_BEING_CRUSHED", nullptr
};

enum EmotionAI
{
	EMOTION_AI_NONE, EMOTION_AI_BACK_AWAY, EMOTION_AI_AVOID_SCARER, EMOTION_AI_IDLE,
	EMOTION_AI_RUN_AWAY_PANIC, EMOTION_AI_FACE_OBJECT, EMOTION_AI_QUARREL
};

const char* const emotionAINames[] = {
	"NONE", "BACK_AWAY", "AVOID_SCARER", "IDLE", "RUN_AWAY_PANIC", "FACE_OBJECT", "QUARREL", nullptr
};

struct EmotionNugget
{
	AsciiString m_name;
	ModelConditionFlags m_conditions;
	ModelConditionFlags m_clearConditions;
	const FXList* m_startFX = nullptr;
	const FXList* m_updateFX = nullptr;
	const FXList* m_endFX = nullptr;
	Int m_type = -1;
	Int m_ai = EMOTION_AI_NONE;
	UnsignedInt m_duration = 0;
	UnsignedInt m_inactive = 0;
	UnsignedInt m_inactiveSameObject = 0;
	UnsignedInt m_inactiveSameType = 0;
	UnsignedInt m_aiLock = 0;
	UnsignedInt m_modifierDelay = 0;
	Real m_enemyThreatLimit = -1.0f;
	Bool m_ignoreIdle = FALSE;
	Bool m_ignoreBusy = FALSE;
	Bool m_preventCommands = FALSE;
	Bool m_modifierWhileActive = FALSE;
};

std::vector<EmotionNugget> emotionNuggets;

const EmotionNugget* findEmotionNugget(const AsciiString& name)
{
	for (const auto& nugget : emotionNuggets)
		if (nugget.m_name.compareNoCase(name) == 0)
			return &nugget;
	return nullptr;
}

void parseEmotionDurationOverride(INI* ini, void*, void* store, const void*)
{
	UnsignedInt frames = 0;
	INI::parseDurationUnsignedInt(ini, nullptr, &frames, nullptr);
	*static_cast<Int*>(store) = static_cast<Int>(frames);
}

void parseEmotionEntry(INI* ini, void*, void* store, const void*)
{
	RetailEmotionEntry entry;
	const AsciiString first = ini->getNextAsciiString();
	if (const char* name = ini->getNextTokenOrNull())
	{
		entry.m_type = INI::scanIndexList(first.str(), emotionTypes);
		entry.m_name = name;
		static const FieldParse fields[] = {
			{ "Duration", parseEmotionDurationOverride, nullptr, offsetof(RetailEmotionEntry, m_duration) },
			{ "AILockDuration", parseEmotionDurationOverride, nullptr, offsetof(RetailEmotionEntry, m_aiLock) },
			{ "AttributeModifier", INI::parseAsciiString, nullptr, offsetof(RetailEmotionEntry, m_modifier) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		ini->initFromINI(&entry, fields);
	}
	else
		entry.m_name = first;
	static_cast<std::vector<RetailEmotionEntry>*>(store)->push_back(std::move(entry));
}

Real squaredDistance(const Object* source, const Object* target)
{
	const Real dx = source->getPosition()->x - target->getPosition()->x;
	const Real dy = source->getPosition()->y - target->getPosition()->y;
	return dx * dx + dy * dy;
}

Bool matchesEmotionFilter(const SpecialPowerObjectFilter& filter, const Object* source, const Object* target)
{
	return (filter.m_rules || filter.m_includeKinds.any() || !filter.m_includeObjects.empty() ||
		filter.m_excludeKinds.any() || !filter.m_excludeObjects.empty()) && filter.matchesObject(source, target);
}

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

Bool isEvil(const Object* obj)
{
	const Player* player = obj->getControllingPlayer();
	const PlayerTemplate* side = player ? player->getPlayerTemplate() : nullptr;
	return side && side->isEvil();
}

Int rankOf(const Object* obj)
{
	const ExperienceTracker* tracker = obj->getExperienceTracker();
	return tracker ? tracker->getRank() : 0;
}

Bool healedRecently(const BodyModuleInterface* body, UnsignedInt window)
{
	const UnsignedInt* last = body->getLastHealingTimestamp();
	return last && AttributeModifierRules::healedWithin(TheGameLogic->getFrame(), *last, window);
}

void setEnragedState(Object* object, Bool enabled)
{
	object->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_ENRAGED), enabled);
	if (enabled)
	{
		object->setWeaponSetFlag(WEAPONSET_ENRAGED);
		object->setModelConditionState(MODELCONDITION_WEAPONSET_ENRAGED);
	}
	else
	{
		object->clearWeaponSetFlag(WEAPONSET_ENRAGED);
		object->clearModelConditionState(MODELCONDITION_WEAPONSET_ENRAGED);
	}
	if (auto* ai = object->getAI())
		if (object->getTemplate()->findRotWKLocomotor(LOCOMOTORSET_ENRAGED) ||
			(object->getTemplate()->getDefaultOwningSide().compareNoCase("Civilian") == 0 &&
				object->getTemplate()->findRotWKLocomotor(LOCOMOTORSET_PANIC)))
			ai->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
	if (traces())
		std::fprintf(stderr, "TEST enrage frame=%u object=%s active=%d\n",
			TheGameLogic->getFrame(), object->getTemplate()->getName().str(), enabled);
}
}

void EnragedBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] = {
		{ "EnragedLifeTimer", INI::parseDurationUnsignedInt, nullptr, offsetof(EnragedBehaviorModuleData, m_life) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse(p);
	p.add(fields);
}

EnragedBehavior::~EnragedBehavior() = default;

void EnragedBehavior::setEnraged(Bool enabled)
{
	Object* self = getObject();
	if (!RetailFileLayers::enabled() || self->isEffectivelyDead())
		return;
	const UnsignedInt life = getEnragedBehaviorModuleData()->m_life;
	m_end = enabled && life ? TheGameLogic->getFrame() + life : 0;
	if (self->testStatus(OBJECT_STATUS_ENRAGED) != enabled)
		setEnragedState(self, enabled);
}

UpdateSleepTime EnragedBehavior::update()
{
	const Object* self = getObject();
	if (!RetailFileLayers::enabled() || self->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	const Bool user45 = self->getLogicModelConditions().test(MODELCONDITION_USER_45);
	const Bool aflame = self->testStatus(OBJECT_STATUS_AFLAME);
	if (user45 != m_user45)
	{
		m_user45 = user45;
		const Int action = RetailBuildingEvents::enrageAction(*self, user45 ? "User45IsActive" : "User45IsNotActive");
		if (action >= 0)
			setEnraged(action != 0);
	}
	if (aflame != m_aflame)
	{
		m_aflame = aflame;
		const Int action = RetailBuildingEvents::enrageAction(*self, aflame ? "OnAflame" : "OnQuenched");
		if (action >= 0)
			setEnraged(action != 0);
	}
	if (m_end && TheGameLogic->getFrame() >= m_end)
		setEnraged(FALSE);
	return UPDATE_SLEEP_NONE;
}

void EnragedBehavior::xferState(Xfer* xfer)
{
	xfer->xferUnsignedInt(&m_end);
	xfer->xferBool(&m_user45);
	xfer->xferBool(&m_aflame);
}

void EnragedBehavior::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xferState(xfer);
}

void EnragedBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	xferState(xfer);
}

void EnragedBehavior::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}

void EntEnragedUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] = {
		{ "HatedObjectFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(EntEnragedUpdateModuleData, m_hated) },
		{ "FriendlyDeadFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(EntEnragedUpdateModuleData, m_friendlyDead) },
		{ "EnragedSearchDistance", INI::parseReal, nullptr, offsetof(EntEnragedUpdateModuleData, m_radius) },
		{ "EnragedTime", INI::parseDurationUnsignedInt, nullptr, offsetof(EntEnragedUpdateModuleData, m_duration) },
		{ "TimeUntilCanRageAgain", INI::parseDurationUnsignedInt, nullptr, offsetof(EntEnragedUpdateModuleData, m_cooldown) },
		{ "EnragedTransitionTime", INI::parseDurationUnsignedInt, nullptr, offsetof(EntEnragedUpdateModuleData, m_transition) },
		{ "EnragedTransitionFX", INI::parseFXList, nullptr, offsetof(EntEnragedUpdateModuleData, m_transitionFX) },
		{ "EnragedOnBuffFX", INI::parseFXList, nullptr, offsetof(EntEnragedUpdateModuleData, m_onFX) },
		{ "EnragedOffBuffFX", INI::parseFXList, nullptr, offsetof(EntEnragedUpdateModuleData, m_offFX) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse(p);
	p.add(fields);
}

EntEnragedUpdate::~EntEnragedUpdate() = default;

UpdateSleepTime EntEnragedUpdate::update()
{
	Object* self = getObject();
	if (!RetailFileLayers::enabled() || self->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	const auto* data = getEntEnragedUpdateModuleData();
	const UnsignedInt now = TheGameLogic->getFrame();
	const Bool burning = self->testStatus(OBJECT_STATUS_AFLAME);
	const Bool ignited = burning && !m_wasBurning;
	const Bool quenched = m_wasBurning && !burning;
	m_wasBurning = burning;
	if (m_end && (quenched || now >= m_end))
	{
		m_end = 0;
		m_cooldownEnd = now + data->m_cooldown;
		setEnragedState(self, FALSE);
		if (data->m_offFX)
			FXList::doFXObj(data->m_offFX, self);
	}
	if (m_end || quenched || (!ignited && (burning || now < m_cooldownEnd || data->m_radius <= 0.0f)))
		return UPDATE_SLEEP_NONE;

	Object* nearestThreat = nullptr;
	if (!ignited)
	{
		Bool friendlyLoss = FALSE;
		Real nearestDistance = data->m_radius * data->m_radius;
		SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(self->getPosition(), data->m_radius, FROM_CENTER_2D);
		MemoryPoolObjectHolder holder(iter);
		for (Object* other = iter->first(); other; other = iter->next())
		{
			if (other == self || other->isDestroyed())
				continue;
			if (other->isEffectivelyDead())
			{
				if (self->getRelationship(other) == ALLIES && matchesEmotionFilter(data->m_friendlyDead, self, other))
					friendlyLoss = TRUE;
				continue;
			}
			if (!matchesEmotionFilter(data->m_hated, self, other))
				continue;
			const Real distance = squaredDistance(self, other);
			if (distance < nearestDistance || (distance == nearestDistance && (!nearestThreat || other->getID() < nearestThreat->getID())))
			{
				nearestDistance = distance;
				nearestThreat = other;
			}
		}
		if (!nearestThreat && !friendlyLoss)
			return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND / 5);
	}

	m_end = now + std::max(data->m_duration, 1u);
	setEnragedState(self, TRUE);
	if (data->m_transition && !self->isDisabledByType(DISABLED_PARALYZED))
		self->setDisabledUntil(DISABLED_PARALYZED, now + data->m_transition);
	if (data->m_transitionFX)
		FXList::doFXObj(data->m_transitionFX, self);
	if (data->m_onFX)
		FXList::doFXObj(data->m_onFX, self);
	if (nearestThreat && self->getTemplate()->getDefaultOwningSide().compareNoCase("Civilian") == 0)
		if (auto* ai = self->getAI())
			ai->fleeFromFear(nearestThreat, data->m_duration, MODELCONDITION_EMOTION_TERROR);
	return UPDATE_SLEEP_NONE;
}

void EntEnragedUpdate::xferState(Xfer* xfer)
{
	xfer->xferUnsignedInt(&m_end);
	xfer->xferUnsignedInt(&m_cooldownEnd);
	xfer->xferBool(&m_wasBurning);
}

void EntEnragedUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xferState(xfer);
}

void EntEnragedUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	xferState(xfer);
}

void EntEnragedUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}

void OneRingPenaltyUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] = {
		{ "SpecialObjectName", INI::parseAsciiString, nullptr, offsetof(OneRingPenaltyUpdateModuleData, m_specialObject) },
		{ "RingTimeBeforeSpawning", INI::parseDurationUnsignedInt, nullptr, offsetof(OneRingPenaltyUpdateModuleData, m_beforeSpawn) },
		{ "TimeSpentRoamingAround", INI::parseDurationUnsignedInt, nullptr, offsetof(OneRingPenaltyUpdateModuleData, m_roaming) },
		{ "TimeRingPowerSuppressed", INI::parseDurationUnsignedInt, nullptr, offsetof(OneRingPenaltyUpdateModuleData, m_suppression) },
		{ "TimeFrozenFromPenalty", INI::parseDurationUnsignedInt, nullptr, offsetof(OneRingPenaltyUpdateModuleData, m_freeze) },
		{ "StartingDistanceFromMe", INI::parseReal, nullptr, offsetof(OneRingPenaltyUpdateModuleData, m_distance) },
		{ "DiscoveredSound", INI::parseAsciiString, nullptr, offsetof(OneRingPenaltyUpdateModuleData, m_discoveredSound) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse(p);
	p.add(fields);
}

OneRingPenaltyUpdate::~OneRingPenaltyUpdate() = default;

void OneRingPenaltyUpdate::clearEye()
{
	if (m_eye != INVALID_ID)
	{
		if (Object* eye = TheGameLogic->findObjectByID(m_eye))
			TheGameLogic->destroyObject(eye);
		m_eye = INVALID_ID;
	}
	m_eyeSince = 0;
}

void OneRingPenaltyUpdate::onDelete()
{
	clearEye();
}

DisabledMaskType OneRingPenaltyUpdate::getDisabledTypesToProcess() const
{
	DisabledMaskType allowed;
	allowed.flip();
	return allowed;
}

UpdateSleepTime OneRingPenaltyUpdate::update()
{
	Object* self = getObject();
	StealthUpdate* stealth = self->getStealth();
	const auto* data = getOneRingPenaltyUpdateModuleData();
	if (!RetailFileLayers::enabled() || !stealth || self->isEffectivelyDead() || self->isDestroyed() ||
		!data->m_beforeSpawn || !data->m_roaming)
	{
		clearEye();
		return UPDATE_SLEEP_FOREVER;
	}
	if (!self->testWeaponSetFlag(WEAPONSET_ONE_RING_MODE))
	{
		m_wornSince = 0;
		clearEye();
		return UPDATE_SLEEP_NONE;
	}

	const UnsignedInt now = TheGameLogic->getFrame();
	if (!m_wornSince)
		m_wornSince = now;
	if (now - m_wornSince < data->m_beforeSpawn)
		return UPDATE_SLEEP_NONE;

	if (!m_eyeSince)
	{
		m_eyeSince = now;
		m_angle = GameLogicRandomValueReal(0.0f, 2.0f * PI);
		if (const ThingTemplate* type = TheThingFactory->findTemplate(data->m_specialObject, FALSE))
			if (Object* eye = TheThingFactory->newObject(type, self->getTeam()))
				m_eye = eye->getID();
		if (traces())
			std::fprintf(stderr, "TEST ring-penalty frame=%u spawned=%d\n", now, m_eye != INVALID_ID);
	}
	const UnsignedInt elapsed = now - m_eyeSince;
	if (elapsed >= data->m_roaming)
	{
		clearEye();
		stealth->suppressRing(now + data->m_suppression);
		if (data->m_freeze && !self->isDisabledByType(DISABLED_PARALYZED))
			self->setDisabledUntil(DISABLED_PARALYZED, now + data->m_freeze);
		if (TheAudio && data->m_discoveredSound.isNotEmpty())
		{
			AudioEventRTS sound(data->m_discoveredSound, self->getID());
			TheAudio->addAudioEvent(&sound);
		}
		m_wornSince = 0;
		if (traces())
			std::fprintf(stderr, "TEST ring-penalty frame=%u suppressed=%u frozen=%u\n", now, data->m_suppression, data->m_freeze);
	}
	else if (Object* eye = TheGameLogic->findObjectByID(m_eye))
	{
		const Real progress = static_cast<Real>(elapsed) / data->m_roaming;
		const Real distance = data->m_distance * (1.0f - progress);
		const Real angle = m_angle + progress * (2.0f * PI);
		Coord3D point = *self->getPosition();
		point.x += distance * WWMath::CosTrig(angle);
		point.y += distance * WWMath::SinTrig(angle);
		point.z = TheTerrainLogic->getGroundHeight(point.x, point.y);
		eye->setPosition(&point);
	}
	return UPDATE_SLEEP_NONE;
}

void OneRingPenaltyUpdate::transferState(Xfer* xfer)
{
	xfer->xferObjectID(&m_eye);
	xfer->xferUnsignedInt(&m_wornSince);
	xfer->xferUnsignedInt(&m_eyeSince);
	xfer->xferReal(&m_angle);
}

void OneRingPenaltyUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	transferState(xfer);
}

void OneRingPenaltyUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	transferState(xfer);
}

void OneRingPenaltyUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}

void BloodthirstyUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] = {
		{ "SacrificeFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(BloodthirstyUpdateModuleData, m_sacrificeFilter) },
		{ "NumToSacrifice", INI::parseInt, nullptr, offsetof(BloodthirstyUpdateModuleData, m_numToSacrifice) },
		{ "InitiateVoice", INI::parseAsciiString, nullptr, offsetof(BloodthirstyUpdateModuleData, m_initiateVoice) },
		{ "InitiateVoice2", INI::parseAsciiString, nullptr, offsetof(BloodthirstyUpdateModuleData, m_initiateVoice2) },
		{ "ExperienceModifier", INI::parseReal, nullptr, offsetof(BloodthirstyUpdateModuleData, m_experienceModifier) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse(p);
	p.add(fields);
}

BloodthirstyUpdate::BloodthirstyUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data)
{
	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}

BloodthirstyUpdate::~BloodthirstyUpdate() = default;

BloodthirstyUpdate* BloodthirstyUpdate::find(const Object* source)
{
	if (!source || !RetailFileLayers::enabled())
		return nullptr;
	static const NameKeyType key = NAMEKEY("BloodthirstyUpdate");
	for (BehaviorModule** module = source->getBehaviorModules(); *module; ++module)
		if ((*module)->getModuleNameKey() == key)
			return static_cast<BloodthirstyUpdate*>(*module);
	return nullptr;
}

Bool BloodthirstyUpdate::canSacrifice(const Object* donor) const
{
	const Object* self = getObject();
	return donor && donor != self && !self->isEffectivelyDead() && !self->isDestroyed() &&
		!donor->isEffectivelyDead() && !donor->isDestroyed() &&
		!self->isDisabled() && !self->isContained() && !donor->isContained() &&
		donor->getControllingPlayer() == self->getControllingPlayer() &&
		HordeContain::get(self) && HordeContain::get(donor) && find(donor) &&
		getBloodthirstyUpdateModuleData()->m_sacrificeFilter.matchesObject(self, donor);
}

void BloodthirstyUpdate::orderSacrifice(Object* donor)
{
	if (!canSacrifice(donor))
		return;
	Object* self = getObject();
	auto* ai = self->getAI();
	if (!ai)
		return;
	m_donor = donor->getID();
	m_deadline = TheGameLogic->getFrame() + 15 * LOGICFRAMES_PER_SECOND;
	ai->aiMoveToPosition(donor->getPosition(), CMD_FROM_AI);
	setWakeFrame(self, UPDATE_SLEEP_NONE);
}

UpdateSleepTime BloodthirstyUpdate::update()
{
	Object* self = getObject();
	Object* donor = m_donor != INVALID_ID ? TheGameLogic->findObjectByID(m_donor) : nullptr;
	const auto* ai = self->getAI();
	if (!canSacrifice(donor) || !ai || ai->getLastCommandSource() != CMD_FROM_AI || TheGameLogic->getFrame() >= m_deadline)
	{
		m_donor = INVALID_ID;
		return UPDATE_SLEEP_FOREVER;
	}
	const Real range = self->getGeometryInfo().getBoundingCircleRadius() + donor->getGeometryInfo().getBoundingCircleRadius() + 10.0f;
	if (squaredDistance(self, donor) > range * range)
		return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND / 5);

	const auto* horde = HordeContain::get(donor);
	const auto* data = getBloodthirstyUpdateModuleData();
	const auto* contents = horde->getContainedItemsList();
	std::vector<Object*> members(contents->begin(), contents->end());
	std::sort(members.begin(), members.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });
	Int count = 0;
	Real experience = 0.0f;
	for (Object* member : members)
	{
		if (member->isEffectivelyDead() || member->isDestroyed() || member->isKindOf(KINDOF_BANNER))
			continue;
		const auto* tracker = member->getExperienceTracker();
		experience += tracker->getRetailExperience() +
			static_cast<Real>(ExperienceLevelStore::award(member, tracker->getRetailThreshold()));
		member->kill(DAMAGE_UNRESISTABLE, DEATH_SLAUGHTERED);
		if (++count == data->m_numToSacrifice)
			break;
	}
	if (count > 0)
	{
		self->getExperienceTracker()->addRetailExperience(experience * data->m_experienceModifier, FALSE);
		const AsciiString& voice = data->m_initiateVoice.isNotEmpty() ? data->m_initiateVoice : data->m_initiateVoice2;
		if (voice.startsWith("EVA:"))
			RetailEva::queue(self->getControllingPlayer(), AsciiString(voice.str() + 4));
		else if (TheAudio && voice.isNotEmpty())
		{
			AudioEventRTS event(voice);
			event.setObjectID(self->getID());
			TheAudio->addAudioEvent(&event);
		}
		if (traces())
			std::fprintf(stderr, "TEST bloodthirsty frame=%u recipient=%u donor=%u members=%d experience=%.1f\n",
				TheGameLogic->getFrame(), unsigned(self->getID()), unsigned(donor->getID()), count, experience * data->m_experienceModifier);
	}
	m_donor = INVALID_ID;
	return UPDATE_SLEEP_FOREVER;
}

void BloodthirstyUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xfer->xferObjectID(&m_donor);
	xfer->xferUnsignedInt(&m_deadline);
}

void BloodthirstyUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	xfer->xferObjectID(&m_donor);
	xfer->xferUnsignedInt(&m_deadline);
}

void BloodthirstyUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}

void EmotionTrackerUpdate::clearEmotionNuggets()
{
	emotionNuggets.clear();
}

void EmotionTrackerUpdate::parseEmotionNugget(INI* ini)
{
	static const FieldParse fields[] = {
		{ "Type", INI::parseIndexList, emotionTypes, offsetof(EmotionNugget, m_type) },
		{ "AIState", INI::parseIndexList, emotionAINames, offsetof(EmotionNugget, m_ai) },
		{ "Duration", INI::parseDurationUnsignedInt, nullptr, offsetof(EmotionNugget, m_duration) },
		{ "InactiveDuration", INI::parseDurationUnsignedInt, nullptr, offsetof(EmotionNugget, m_inactive) },
		{ "InactiveDurationSameObject", INI::parseDurationUnsignedInt, nullptr, offsetof(EmotionNugget, m_inactiveSameObject) },
		{ "InactiveDurationSameType", INI::parseDurationUnsignedInt, nullptr, offsetof(EmotionNugget, m_inactiveSameType) },
		{ "AILockDuration", INI::parseDurationUnsignedInt, nullptr, offsetof(EmotionNugget, m_aiLock) },
		{ "AttributeStartDelay", INI::parseDurationUnsignedInt, nullptr, offsetof(EmotionNugget, m_modifierDelay) },
		{ "OnlyIfEnemyThreatBelow", INI::parseReal, nullptr, offsetof(EmotionNugget, m_enemyThreatLimit) },
		{ "ModelConditions", ModelConditionFlags::parseFromINI, nullptr, offsetof(EmotionNugget, m_conditions) },
		{ "ModelConditionsClear", ModelConditionFlags::parseFromINI, nullptr, offsetof(EmotionNugget, m_clearConditions) },
		{ "IgnoreIfUnitIdle", INI::parseBool, nullptr, offsetof(EmotionNugget, m_ignoreIdle) },
		{ "IgnoreIfUnitBusy", INI::parseBool, nullptr, offsetof(EmotionNugget, m_ignoreBusy) },
		{ "PreventPlayerCommands", INI::parseBool, nullptr, offsetof(EmotionNugget, m_preventCommands) },
		{ "AttributeModifierWhileEmotionActive", INI::parseBool, nullptr, offsetof(EmotionNugget, m_modifierWhileActive) },
		{ "StartFXList", INI::parseFXList, nullptr, offsetof(EmotionNugget, m_startFX) },
		{ "UpdateFXList", INI::parseFXList, nullptr, offsetof(EmotionNugget, m_updateFX) },
		{ "EndFXList", INI::parseFXList, nullptr, offsetof(EmotionNugget, m_endFX) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	EmotionNugget nugget;
	nugget.m_name = ini->getNextAsciiString();
	ini->initFromINI(&nugget, fields);
	for (auto& previous : emotionNuggets)
		if (previous.m_name.compareNoCase(nugget.m_name) == 0)
		{
			previous = std::move(nugget);
			return;
		}
	emotionNuggets.push_back(std::move(nugget));
}

void EmotionTrackerUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] = {
		{ "AddEmotion", parseEmotionEntry, nullptr, offsetof(EmotionTrackerUpdateModuleData, m_emotions) },
		{ "AfraidOf", SpecialPowerObjectFilter::parse, nullptr, offsetof(EmotionTrackerUpdateModuleData, m_afraidOf) },
		{ "AlwaysAfraidOf", SpecialPowerObjectFilter::parse, nullptr, offsetof(EmotionTrackerUpdateModuleData, m_alwaysAfraidOf) },
		{ "PointAt", SpecialPowerObjectFilter::parse, nullptr, offsetof(EmotionTrackerUpdateModuleData, m_pointAt) },
		{ "TauntAndPointExcluded", SpecialPowerObjectFilter::parse, nullptr, offsetof(EmotionTrackerUpdateModuleData, m_tauntExcluded) },
		{ "TauntAndPointDistance", INI::parseReal, nullptr, offsetof(EmotionTrackerUpdateModuleData, m_tauntRadius) },
		{ "TauntAndPointUpdateDelay", INI::parseDurationUnsignedInt, nullptr, offsetof(EmotionTrackerUpdateModuleData, m_tauntInterval) },
		{ "FearScanDistance", INI::parseReal, nullptr, offsetof(EmotionTrackerUpdateModuleData, m_fearRadius) },
		{ "HeroScanDistance", INI::parseReal, nullptr, offsetof(EmotionTrackerUpdateModuleData, m_heroRadius) },
		{ "ImmuneToFearLevel", INI::parseInt, nullptr, offsetof(EmotionTrackerUpdateModuleData, m_immuneRank) },
		{ "IgnoreVeterancy", INI::parseBool, nullptr, offsetof(EmotionTrackerUpdateModuleData, m_ignoreVeterancy) },
		{ "QuarrelProbability", INI::parsePercentToReal, nullptr, offsetof(EmotionTrackerUpdateModuleData, m_quarrelChance) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse(p);
	p.add(fields);
}

EmotionTrackerUpdate::~EmotionTrackerUpdate() = default;

void EmotionTrackerUpdate::finishEmotion()
{
	Object* self = getObject();
	const auto& entry = getEmotionTrackerUpdateModuleData()->m_emotions[m_activeEntry];
	if (const auto* nugget = findEmotionNugget(entry.m_name))
	{
		if (nugget->m_modifierWhileActive && entry.m_modifier.isNotEmpty())
			AttributeModifiers::remove(self, entry.m_modifier, AttributeModifiers::ownerOf(self, getModuleTagNameKey()));
		if (nugget->m_endFX && !self->isEffectivelyDead())
			FXList::doFXObj(nugget->m_endFX, self);
	}
	self->clearModelConditionFlags(m_appliedConditions);
	m_appliedConditions.clear();
	m_activeEntry = -1;
	m_modifierFrame = 0;
}

UpdateSleepTime EmotionTrackerUpdate::update()
{
	Object* self = getObject();
	const auto* data = getEmotionTrackerUpdateModuleData();
	if (!RetailFileLayers::enabled() || data->m_emotions.empty() || emotionNuggets.empty() || self->isDestroyed())
		return UPDATE_SLEEP_FOREVER;

	const UnsignedInt now = TheGameLogic->getFrame();
	if (m_activeEntry >= 0)
	{
		if (now >= m_endFrame || self->isEffectivelyDead())
			finishEmotion();
		else
		{
			if (m_modifierFrame && m_modifierFrame <= now)
			{
				const auto& entry = data->m_emotions[m_activeEntry];
				const auto* nugget = findEmotionNugget(entry.m_name);
				if (nugget && nugget->m_modifierWhileActive)
					AttributeModifiers::apply(self, entry.m_modifier, m_endFrame - now, 0, AttributeModifiers::ownerOf(self, getModuleTagNameKey()));
				else
					AttributeModifiers::apply(self, entry.m_modifier);
				m_modifierFrame = 0;
			}
			return UPDATE_SLEEP(m_modifierFrame ? std::min(m_endFrame, m_modifierFrame) - now : m_endFrame - now);
		}
	}

	const Real range = std::max({data->m_tauntRadius, data->m_fearRadius, data->m_heroRadius});
	if (self->isEffectivelyDead() || range <= 0.0f)
		return UPDATE_SLEEP_FOREVER;
	const UnsignedInt interval = std::max(data->m_tauntInterval, UnsignedInt(LOGICFRAMES_PER_SECOND / 5));
	if (m_recheckFrame > now)
		return UPDATE_SLEEP(m_recheckFrame - now);

	Object* anchor = HordeContain::getAnchor(self);
	Object* subject = anchor ? anchor : self;
	const Bool veteranImmune = !data->m_ignoreVeterancy && data->m_immuneRank > 0 && rankOf(subject) >= data->m_immuneRank;
	Real enemyThreat = 0.0f;
	Object* fearSource = nullptr;
	Object* terrorSource = nullptr;
	Object* pointTarget = nullptr;
	Object* tauntTarget = nullptr;
	Object* heroTarget = nullptr;
	const auto closer = [&](Object*& nearest, Object* candidate, Real distance)
	{
		const Real bestDistance = nearest ? squaredDistance(self, nearest) : range * range;
		if (!nearest || distance < bestDistance || (distance == bestDistance && candidate->getID() < nearest->getID()))
			nearest = candidate;
	};
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(self->getPosition(), range, FROM_CENTER_2D);
	MemoryPoolObjectHolder holder(iter);
	for (Object* target = iter->first(); target; target = iter->next())
	{
		if (target == self || target->isEffectivelyDead() || target->isDestroyed())
			continue;
		const auto relationship = self->getRelationship(target);
		const Real distance = squaredDistance(self, target);
		if (relationship == ALLIES && target->isKindOf(KINDOF_HERO) && distance <= data->m_heroRadius * data->m_heroRadius)
			closer(heroTarget, target, distance);
		if (relationship != ENEMIES)
			continue;
		enemyThreat = std::max(enemyThreat, target->getTemplate()->getRotWKObjectData().threatLevel);
		if (distance <= data->m_fearRadius * data->m_fearRadius)
		{
			if (matchesEmotionFilter(data->m_alwaysAfraidOf, self, target))
				closer(terrorSource, target, distance);
			if (!veteranImmune && matchesEmotionFilter(data->m_afraidOf, self, target))
				closer(fearSource, target, distance);
		}
		if (distance <= data->m_tauntRadius * data->m_tauntRadius &&
			!matchesEmotionFilter(data->m_tauntExcluded, self, target))
		{
			if (matchesEmotionFilter(data->m_pointAt, self, target))
				closer(pointTarget, target, distance);
			else if (target->isKindOf(KINDOF_INFANTRY) || target->isKindOf(KINDOF_CAVALRY) || target->isKindOf(KINDOF_MONSTER))
				closer(tauntTarget, target, distance);
		}
	}

	struct Opportunity { Int type; const Object* source; };
	const Opportunity opportunities[] = {
		{EMOTION_TERROR, terrorSource}, {EMOTION_FEAR, fearSource},
		{EMOTION_POINT, pointTarget}, {EMOTION_TAUNT, tauntTarget}, {EMOTION_HERO_CHEER, heroTarget}
	};
	for (const auto& opportunity : opportunities)
	{
		if (!opportunity.source)
			continue;
		if (startEmotion(static_cast<EmotionType>(opportunity.type), opportunity.source, enemyThreat))
			return UPDATE_SLEEP(m_modifierFrame ? std::max(1u, m_modifierFrame - now) : m_endFrame - now);
	}
	if (data->m_quarrelChance > 0.0f && (!subject->getAI() || subject->getAI()->isIdle()) &&
		GameLogicRandomValueReal(0.0f, 1.0f) < data->m_quarrelChance &&
		startEmotion(EMOTION_QUARRELSOME, nullptr, enemyThreat))
		return UPDATE_SLEEP(m_endFrame - now);
	return UPDATE_SLEEP(interval);
}

Bool EmotionTrackerUpdate::signal(Object* victim, EmotionType type, const Object* source)
{
	if (!RetailFileLayers::enabled() || !victim || victim->isEffectivelyDead() || victim->isDestroyed())
		return FALSE;
	auto* tracker = static_cast<EmotionTrackerUpdate*>(victim->findUpdateModule(NAMEKEY("EmotionTrackerUpdate")));
	if (!tracker || !tracker->startEmotion(type, source, -1.0f))
		return FALSE;
	tracker->setWakeFrame(victim, UPDATE_SLEEP_NONE);
	return TRUE;
}

Bool EmotionTrackerUpdate::blocksPlayerCommands(const Object* object)
{
	const auto* tracker = static_cast<const EmotionTrackerUpdate*>(object->findUpdateModule(NAMEKEY("EmotionTrackerUpdate")));
	if (!tracker || tracker->m_activeEntry < 0 || TheGameLogic->getFrame() >= tracker->m_endFrame)
		return FALSE;
	const auto& entry = tracker->getEmotionTrackerUpdateModuleData()->m_emotions[tracker->m_activeEntry];
	const auto* nugget = findEmotionNugget(entry.m_name);
	return nugget && nugget->m_preventCommands;
}

static Int emotionPriority(Int type)
{
	switch (type)
	{
		case EMOTION_DOOM:
		case EMOTION_BRACE:
		case EMOTION_UNCONTROLLABLE_FEAR:
			return 3;
		case EMOTION_TERROR:
		case EMOTION_FEAR:
			return 2;
		case EMOTION_ALERT:
		case EMOTION_CHEER:
		case EMOTION_PRE_CRUSH:
			return 1;
		default:
			return 0;
	}
}

Bool EmotionTrackerUpdate::startEmotion(EmotionType type, const Object* source, Real enemyThreat)
{
	Object* self = getObject();
	const auto* data = getEmotionTrackerUpdateModuleData();
	const UnsignedInt now = TheGameLogic->getFrame();
	if (m_activeEntry >= 0 && now < m_endFrame)
	{
		const auto& active = data->m_emotions[m_activeEntry];
		const auto* current = findEmotionNugget(active.m_name);
		const Int activeType = active.m_type >= 0 && active.m_type != EMOTION_OVERRIDE ?
			active.m_type : (current ? current->m_type : -1);
		if (emotionPriority(type) <= emotionPriority(activeType))
			return FALSE;
	}
	else if (now < m_recheckFrame && emotionPriority(type) < 2)
		return FALSE;

	Object* anchor = HordeContain::getAnchor(self);
	Object* subject = anchor ? anchor : self;
	auto* ai = subject->getAI();
	const Bool idle = !ai || ai->isIdle();
	const UnsignedInt interval = std::max(data->m_tauntInterval, UnsignedInt(LOGICFRAMES_PER_SECOND / 5));
	for (Int index = 0; index < static_cast<Int>(data->m_emotions.size()); ++index)
	{
		const auto& entry = data->m_emotions[index];
		const auto* nugget = findEmotionNugget(entry.m_name);
		if (!nugget)
			continue;
		const Int configured = entry.m_type >= 0 && entry.m_type != EMOTION_OVERRIDE ? entry.m_type : nugget->m_type;
		if (configured != type || (idle && nugget->m_ignoreIdle) || (!idle && nugget->m_ignoreBusy))
			continue;
		if (nugget->m_enemyThreatLimit >= 0.0f && enemyThreat < 0.0f)
		{
			enemyThreat = 0.0f;
			const Real radius = std::max({data->m_tauntRadius, data->m_fearRadius, data->m_heroRadius});
			if (radius > 0.0f)
			{
				SimpleObjectIterator* scan = ThePartitionManager->iterateObjectsInRange(self->getPosition(), radius, FROM_CENTER_2D);
				MemoryPoolObjectHolder holder(scan);
				for (Object* other = scan->first(); other; other = scan->next())
					if (!other->isEffectivelyDead() && self->getRelationship(other) == ENEMIES)
						enemyThreat = std::max(enemyThreat, other->getTemplate()->getRotWKObjectData().threatLevel);
			}
		}
		if (nugget->m_enemyThreatLimit >= 0.0f && enemyThreat >= nugget->m_enemyThreatLimit)
			continue;

		const UnsignedInt aiLock = entry.m_aiLock >= 0 ? static_cast<UnsignedInt>(entry.m_aiLock) : nugget->m_aiLock;
		UnsignedInt duration = entry.m_duration >= 0 ? static_cast<UnsignedInt>(entry.m_duration) : nugget->m_duration;
		if (!duration)
			duration = std::max(aiLock, interval);
		if (type == EMOTION_FEAR || type == EMOTION_TERROR || type == EMOTION_UNCONTROLLABLE_FEAR)
		{
			const Int resistance = type == EMOTION_TERROR ? AttributeModifierRules::RESIST_TERROR : AttributeModifierRules::RESIST_FEAR;
			const Real scale = 1.0f - std::clamp(AttributeModifiers::value(subject, resistance), -1.0f, 1.0f);
			duration = static_cast<UnsignedInt>(duration * scale);
			if (!duration)
				continue;
			if (ai && !anchor && source && (nugget->m_ai == EMOTION_AI_BACK_AWAY || nugget->m_ai == EMOTION_AI_AVOID_SCARER ||
				nugget->m_ai == EMOTION_AI_RUN_AWAY_PANIC))
				ai->fleeFromFear(source, aiLock ? static_cast<UnsignedInt>(aiLock * scale) : duration,
					type == EMOTION_TERROR ? MODELCONDITION_EMOTION_TERROR :
					(type == EMOTION_UNCONTROLLABLE_FEAR ? MODELCONDITION_EMOTION_UNCONTROLLABLY_AFRAID : MODELCONDITION_EMOTION_AFRAID));
		}
		if (ai && !anchor && nugget->m_ai == EMOTION_AI_IDLE)
			ai->aiIdle(CMD_FROM_AI);

		if (m_activeEntry >= 0)
			finishEmotion();
		m_activeEntry = index;
		m_endFrame = now + duration;
		m_recheckFrame = m_endFrame + std::max({interval, nugget->m_inactive, nugget->m_inactiveSameObject, nugget->m_inactiveSameType});
		m_appliedConditions = nugget->m_conditions;
		m_appliedConditions.clear(self->getLogicModelConditions());
		self->clearAndSetModelConditionFlags(nugget->m_clearConditions, nugget->m_conditions);
		if (nugget->m_startFX)
			FXList::doFXObj(nugget->m_startFX, self);
		if (entry.m_modifier.isNotEmpty() && nugget->m_modifierDelay < duration)
			m_modifierFrame = std::max(1u, now + nugget->m_modifierDelay);
		if (traces())
			std::fprintf(stderr, "TEST emotion frame=%u unit=%s type=%s source=%s duration=%u\n",
				now, self->getTemplate()->getName().str(), emotionTypes[type],
				source ? source->getTemplate()->getName().str() : "-", duration);
		return TRUE;
	}
	return FALSE;
}

void EmotionTrackerUpdate::xferEmotionState(Xfer* xfer)
{
	xfer->xferInt(&m_activeEntry);
	xfer->xferUnsignedInt(&m_endFrame);
	xfer->xferUnsignedInt(&m_recheckFrame);
	xfer->xferUnsignedInt(&m_modifierFrame);
	m_appliedConditions.xfer(xfer);
}

void EmotionTrackerUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xferEmotionState(xfer);
}

void EmotionTrackerUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	xferEmotionState(xfer);
}

void EmotionTrackerUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	if (m_activeEntry >= 0 && m_activeEntry < static_cast<Int>(getEmotionTrackerUpdateModuleData()->m_emotions.size()))
		getObject()->setModelConditionFlags(m_appliedConditions);
	else
		m_activeEntry = -1;
}

void LargeGroupBonusUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] = {
		{ "UpdateRate", INI::parseDurationUnsignedInt, nullptr, offsetof(LargeGroupBonusUpdateModuleData, m_updateRate) },
		{ "HordeMemberFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(LargeGroupBonusUpdateModuleData, m_memberFilter) },
		{ "Count", INI::parseInt, nullptr, offsetof(LargeGroupBonusUpdateModuleData, m_count) },
		{ "Radius", INI::parseReal, nullptr, offsetof(LargeGroupBonusUpdateModuleData, m_radius) },
		{ "RubOffRadius", INI::parseReal, nullptr, offsetof(LargeGroupBonusUpdateModuleData, m_rubOffRadius) },
		{ "AlliesOnly", INI::parseBool, nullptr, offsetof(LargeGroupBonusUpdateModuleData, m_alliesOnly) },
		{ "AttributeModifier", INI::parseAsciiString, nullptr, offsetof(LargeGroupBonusUpdateModuleData, m_modifier) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse(p);
	p.add(fields);
}

LargeGroupBonusUpdate::~LargeGroupBonusUpdate() = default;

UpdateSleepTime LargeGroupBonusUpdate::update()
{
	Object* self = getObject();
	const auto* data = getLargeGroupBonusUpdateModuleData();
	if (!RetailFileLayers::enabled() || self->isEffectivelyDead() || self->isDestroyed() ||
		data->m_count <= 0 || AttributeModifiers::findList(data->m_modifier) < 0)
		return UPDATE_SLEEP_FOREVER;

	Int count = 1;
	Bool rubOff = FALSE;
	const Real scanRadius = std::max(data->m_radius, data->m_rubOffRadius);
	if (scanRadius > 0.0f)
	{
		SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(self->getPosition(), scanRadius, FROM_CENTER_2D);
		MemoryPoolObjectHolder holder(iter);
		for (Object* target = iter->first(); target; target = iter->next())
		{
			if (target->isEffectivelyDead() || target->isDestroyed() ||
				(data->m_alliesOnly && target != self && self->getRelationship(target) != ALLIES))
				continue;

			const Real dx = self->getPosition()->x - target->getPosition()->x;
			const Real dy = self->getPosition()->y - target->getPosition()->y;
			const Real distanceSquared = dx * dx + dy * dy;
			if (const auto* horde = HordeContain::get(target))
			{
				if (count < data->m_count && distanceSquared <= data->m_radius * data->m_radius)
				{
					for (const Object* member : *horde->getContainedItemsList())
						if (!member->isEffectivelyDead() && !member->isDestroyed() &&
							data->m_memberFilter.matchesObject(self, member))
							++count;
				}
			}

			if (target != self && data->m_rubOffRadius > 0.0f &&
				distanceSquared <= data->m_rubOffRadius * data->m_rubOffRadius)
			{
				for (BehaviorModule** module = target->getBehaviorModules(); *module; ++module)
				{
					if ((*module)->getModuleNameKey() != getModuleNameKey())
						continue;
					const auto* bonus = static_cast<const LargeGroupBonusUpdate*>(*module);
					if (bonus->m_meetsThreshold &&
						bonus->getLargeGroupBonusUpdateModuleData()->m_modifier == data->m_modifier)
					{
						rubOff = TRUE;
						break;
					}
				}
			}
			if (count >= data->m_count)
				break;
		}
	}

	m_meetsThreshold = count >= data->m_count;
	const auto owner = AttributeModifiers::ownerOf(self, getModuleTagNameKey());
	if (m_meetsThreshold || rubOff)
		AttributeModifiers::apply(self, data->m_modifier, 0, 0, owner);
	else
		AttributeModifiers::remove(self, data->m_modifier, owner);
	return UPDATE_SLEEP(std::max(data->m_updateRate, 1u));
}

void LargeGroupBonusUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xfer->xferBool(&m_meetsThreshold);
}

void LargeGroupBonusUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	xfer->xferBool(&m_meetsThreshold);
}

void LargeGroupBonusUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
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

void RadiateFearUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] =
	{
		{ "InitiallyActive", INI::parseBool, nullptr, offsetof(RadiateFearUpdateModuleData, m_initiallyActive) },
		{ "GenerateFear", INI::parseBool, nullptr, offsetof(RadiateFearUpdateModuleData, m_generateFear) },
		{ "GenerateTerror", INI::parseBool, nullptr, offsetof(RadiateFearUpdateModuleData, m_generateTerror) },
		{ "GenerateUncontrollableFear", INI::parseBool, nullptr, offsetof(RadiateFearUpdateModuleData, m_generateUncontrollableFear) },
		{ "WhichSpecialPower", INI::parseInt, nullptr, offsetof(RadiateFearUpdateModuleData, m_whichSpecialPower) },
		{ "EmotionPulseRadius", INI::parseReal, nullptr, offsetof(RadiateFearUpdateModuleData, m_pulseRadius) },
		{ "EmotionPulseInterval", INI::parseDurationUnsignedInt, nullptr, offsetof(RadiateFearUpdateModuleData, m_pulseInterval) },
		{ "VictimFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(RadiateFearUpdateModuleData, m_victimFilter) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse(p);
	p.add(fields);
	p.add(UpgradeMuxData::getFieldParse(), offsetof(RadiateFearUpdateModuleData, m_upgrades));
}

UpdateSleepTime RadiateFearUpdate::update()
{
	const auto* data = getRadiateFearUpdateModuleData();
	Object* self = getObject();
	if (self->isEffectivelyDead() || data->m_pulseRadius <= 0.0f ||
		!(data->m_generateFear || data->m_generateTerror || data->m_generateUncontrollableFear))
		return UPDATE_SLEEP_FOREVER;

	UpgradeMaskType activation, conflicting;
	data->m_upgrades.getUpgradeActivationMasks(activation, conflicting);
	UpgradeMaskType upgrades = self->getObjectCompletedUpgradeMask();
	if (const Player* player = self->getControllingPlayer())
		upgrades.set(player->getCompletedUpgradeMask());
	const Bool activated = activation.any() && (data->m_upgrades.m_requiresAllTriggers ?
		upgrades.testForAll(activation) : upgrades.testForAny(activation));
	if ((!data->m_initiallyActive && !activated) || upgrades.testForAny(conflicting) || self->isDisabled())
		return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND);

	const auto condition = data->m_generateTerror ? MODELCONDITION_EMOTION_TERROR :
		(data->m_generateUncontrollableFear ? MODELCONDITION_EMOTION_UNCONTROLLABLY_AFRAID : MODELCONDITION_EMOTION_AFRAID);
	const Int kind = data->m_generateTerror ? AttributeModifierRules::RESIST_TERROR : AttributeModifierRules::RESIST_FEAR;
	const UnsignedInt interval = data->m_pulseInterval ? data->m_pulseInterval : LOGICFRAMES_PER_SECOND;
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(self->getPosition(), data->m_pulseRadius, FROM_CENTER_2D);
	MemoryPoolObjectHolder holder(iter);
	std::vector<Object*> targets;
	for (Object* victim = iter->first(); victim; victim = iter->next())
	{
		if (self->getRelationship(victim) != ENEMIES || !data->m_victimFilter.matchesObject(self, victim))
			continue;
		if (Object* anchor = HordeContain::getAnchor(victim))
			victim = anchor;
		targets.push_back(victim);
	}
	std::sort(targets.begin(), targets.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });
	targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
	for (Object* victim : targets)
	{
		if (EmotionTrackerUpdate::signal(victim, data->m_generateTerror ? EMOTION_TERROR :
			(data->m_generateUncontrollableFear ? EMOTION_UNCONTROLLABLE_FEAR : EMOTION_FEAR), self))
			continue;
		const Real resistance = std::clamp(AttributeModifiers::value(victim, kind), -1.0f, 1.0f);
		const UnsignedInt duration = static_cast<UnsignedInt>(std::max(interval, UnsignedInt(LOGICFRAMES_PER_SECOND)) * (1.0f - resistance));
		auto* ai = victim->getAI();
		if (ai && ai->fleeFromFear(self, duration, condition) && RetailTestCommands::tracesCombat())
		{
			std::fprintf(stderr, "TEST fear frame=%u source=%u(%s) victim=%u(%s) duration=%u terror=%d\n",
				TheGameLogic->getFrame(), unsigned(self->getID()), self->getTemplate()->getName().str(),
				unsigned(victim->getID()), victim->getTemplate()->getName().str(), duration, data->m_generateTerror);
			std::fflush(stderr);
		}
	}
	return UPDATE_SLEEP(interval + self->getID() % 5);
}

RadiateFearUpdate::~RadiateFearUpdate() = default;

void RadiateFearUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
}

void RadiateFearUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
}

void RadiateFearUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
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
	const Bool alignmentFilter = data->m_affectGood || data->m_affectEvil;
	const AttributeModifiers::Sides sides = alignmentFilter ? AttributeModifiers::SIDES_ALL
		: data->m_targetEnemy ? AttributeModifiers::SIDES_ENEMIES : AttributeModifiers::SIDES_ALLIES;
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
		if (alignmentFilter && !AttributeModifierRules::alignmentAllowed(data->m_affectGood, data->m_affectEvil, isEvil(target)))
			return;
		if (!AttributeModifierRules::rankAllowed(data->m_maxActiveRank, rankOf(target)))
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
		if (healFraction > 0.0f && body && body->getHealth() < body->getMaxHealth() && !(data->m_nonStackable && healedRecently(body, delay)))
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
	const Bool applied = AttributeModifiers::apply(self, data->m_attributeModifier, AttributeModifierHelper::USE_LIST_DURATION, 0, AttributeModifiers::ownerOf(getObject(), getModuleTagNameKey()));
	if (traces())
		std::fprintf(stderr, "TEST modifier upgrade frame=%u object=%u(%s) list=%s applied=%d\n", TheGameLogic->getFrame(),
			static_cast<unsigned>(self->getID()), self->getTemplate()->getName().str(), data->m_attributeModifier.str(), applied ? 1 : 0);
}

Bool AttributeModifierUpgrade::resetUpgrade( const UpgradeMaskType& keyMask )
{
	const Bool reset = UpgradeMux::resetUpgrade(keyMask);
	if (reset)
		AttributeModifiers::remove(getObject(), getAttributeModifierUpgradeModuleData()->m_attributeModifier, AttributeModifiers::ownerOf(getObject(), getModuleTagNameKey()));
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
		AttributeModifiers::apply(horde, name, 0, 0, AttributeModifiers::ownerOf(horde, contain->getModuleTagNameKey()));
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
		AttributeModifiers::remove(horde, name, AttributeModifiers::ownerOf(horde, contain->getModuleTagNameKey()));
}
}
