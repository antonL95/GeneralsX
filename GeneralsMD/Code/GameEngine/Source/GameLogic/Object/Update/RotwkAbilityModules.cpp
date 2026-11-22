#include "PreRTS.h"

#define DEFINE_WEAPONSLOTTYPE_NAMES

#include "GameLogic/Module/RotwkAbilityModules.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "Common/AbilityRules.h"
#include "Common/ActionManager.h"
#include "Common/GameAudio.h"
#include "Common/INIAudit.h"
#include "Common/INIException.h"
#include "Common/Player.h"
#include "Common/RandomValue.h"
#include "Common/RetailFileLayers.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIStateMachine.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/DoCommandUpgrade.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/RotwkParseOnlyModules.h"
#include "GameLogic/Module/RotwkModifierModules.h"

void CurseSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] = {
		{ "CursePercentage", INI::parsePercentToReal, nullptr, offsetof(CurseSpecialPowerModuleData, m_cursePercentage) },
		{ "CursedFX", INI::parseFXList, nullptr, offsetof(CurseSpecialPowerModuleData, m_cursedFX) },
		{ "TriggerFX", INI::parseFXList, nullptr, offsetof(CurseSpecialPowerModuleData, m_triggerFX) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	p.add(fields);
}

CurseSpecialPower::~CurseSpecialPower() = default;

Bool CurseSpecialPower::acceptsIntent(const Object* target, const Coord3D* position) const
{
	if (!usesRotwkRules() || !SpecialAbilityUpdate::acceptsIntent(target, position))
		return FALSE;
	return target ? !target->isEffectivelyDead() && getObject()->getRelationship(target) == ENEMIES :
		position && getTemplate()->getRadiusCursorRadius() > 0.0f;
}

void CurseSpecialPower::triggerAbilityEffect()
{
	SpecialAbilityUpdate::triggerAbilityEffect();
	Object* self = getObject();
	const auto* data = getCurseSpecialPowerModuleData();
	const auto* templateData = getTemplate();
	const Real fraction = std::clamp(data->m_cursePercentage, 0.0f, 1.0f);
	const UnsignedInt now = TheGameLogic->getFrame();
	std::vector<std::pair<Player*, const SpecialPowerTemplate*>> sharedTimers;
	const auto curseTarget = [&](Object* victim)
	{
		if (!victim || victim->isEffectivelyDead() || victim->isDestroyed() || self->getRelationship(victim) != ENEMIES)
			return;
		if (templateData->hasRetailFlag(SpecialPowerTemplate::RETAIL_NEEDS_OBJECT_FILTER) &&
			!templateData->getObjectFilter().matchesObject(self, victim))
			return;

		for (BehaviorModule** module = victim->getBehaviorModules(); *module; ++module)
		{
			auto* power = (*module)->getSpecialPower();
			const auto* powerTemplate = power ? power->getSpecialPowerTemplate() : nullptr;
			if (!powerTemplate || !powerTemplate->getReloadTime())
				continue;
			Player* sharedOwner = powerTemplate->isSharedNSync() ? victim->getControllingPlayer() : nullptr;
			if (sharedOwner)
			{
				const auto timer = std::make_pair(sharedOwner, powerTemplate);
				if (std::find(sharedTimers.begin(), sharedTimers.end(), timer) != sharedTimers.end())
					continue;
				sharedTimers.push_back(timer);
			}
			const UnsignedInt reload = powerTemplate->getReloadTime();
			const UnsignedInt ready = power->getReadyFrame();
			const UnsignedInt remaining = ready > now ? ready - now : 0;
			if (remaining < reload)
			{
				const UnsignedInt increase = static_cast<UnsignedInt>(reload * fraction);
				const UnsignedInt nextReady = now + remaining + std::min(increase, reload - remaining);
				if (sharedOwner)
					sharedOwner->expressSpecialPowerReadyFrame(powerTemplate, nextReady);
				else
					power->setReadyFrame(nextReady);
			}
		}
		if (data->m_cursedFX)
			FXList::doFXObj(data->m_cursedFX, victim);
	};

	if (data->m_triggerFX)
		FXList::doFXObj(data->m_triggerFX, self);
	if (getAbilityTargetID() != INVALID_ID)
		curseTarget(TheGameLogic->findObjectByID(getAbilityTargetID()));
	else if (templateData->getRadiusCursorRadius() > 0.0f)
	{
		SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(&getAbilityTargetPos(),
			templateData->getRadiusCursorRadius(), FROM_CENTER_2D);
		MemoryPoolObjectHolder holder(iter);
		std::vector<Object*> targets;
		for (Object* target = iter->first(); target; target = iter->next())
			targets.push_back(target);
		std::sort(targets.begin(), targets.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });
		for (Object* target : targets)
			curseTarget(target);
	}
}

void CurseSpecialPower::crc(Xfer* xfer)
{
	SpecialAbilityUpdate::crc(xfer);
}

void CurseSpecialPower::xfer(Xfer* xfer)
{
	SpecialAbilityUpdate::xfer(xfer);
}

void CurseSpecialPower::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}

void BoredUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "ScanDelayTime", INI::parseDurationUnsignedInt, nullptr, offsetof(BoredUpdateModuleData, m_scanDelay) },
		{ "ScanDistance", INI::parseReal, nullptr, offsetof(BoredUpdateModuleData, m_scanDistance) },
		{ "CanScanWhileAttackingOrMoving", INI::parseBool, nullptr, offsetof(BoredUpdateModuleData, m_scanWhileAttackingOrMoving) },
		{ "BoredFilter", [](INI* ini, void* instance, void* store, const void* data) {
			SpecialPowerObjectFilter::parse(ini, instance, store, data);
			static_cast<BoredUpdateModuleData*>(instance)->m_hasFilter = TRUE;
		}, nullptr, offsetof(BoredUpdateModuleData, m_filter) },
		{ "SpecialPowerTemplate", INI::parseSpecialPowerTemplate, nullptr, offsetof(BoredUpdateModuleData, m_power) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

BoredUpdate::BoredUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data)
{
	const UnsignedInt delay = std::min(getBoredUpdateModuleData()->m_scanDelay, static_cast<UnsignedInt>(UPDATE_SLEEP_FOREVER));
	if (delay)
		setWakeFrame(getObject(), UPDATE_SLEEP(std::max(1, GameLogicRandomValue(0, static_cast<Int>(delay)))));
}

BoredUpdate::~BoredUpdate() = default;

UpdateSleepTime BoredUpdate::update()
{
	Object* self = getObject();
	const auto* data = getBoredUpdateModuleData();
	const auto* power = data->m_power;
	if (!power || self->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	const UpdateSleepTime delay = UPDATE_SLEEP(std::clamp(data->m_scanDelay, 1u, static_cast<UnsignedInt>(UPDATE_SLEEP_FOREVER)));
	const auto* ai = self->getAI();
	const auto* ability = self->getSpecialPowerModule(power);
	if (!ai || !ability || !ability->isReady() ||
		(!data->m_scanWhileAttackingOrMoving && (ai->isMoving() || ai->isAttacking())))
		return delay;
	if (!data->m_hasFilter)
	{
		if (TheActionManager->canDoSpecialPower(self, power, CMD_FROM_AI, 0))
			self->doSpecialPower(power, 0);
		return delay;
	}
	if (!(data->m_scanDistance > 0.0f) || !std::isfinite(data->m_scanDistance))
		return delay;

	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(self->getPosition(), data->m_scanDistance,
		FROM_CENTER_2D, nullptr, ITER_FASTEST);
	MemoryPoolObjectHolder holder(iter);
	Object* nearest = nullptr;
	Real nearestDistance = 0.0f;
	for (Object* candidate = iter->first(); candidate; candidate = iter->next())
	{
		if (candidate == self || candidate->isEffectivelyDead() || self->getRelationship(candidate) != ALLIES ||
			!data->m_filter.matchesObject(self, candidate))
			continue;
		const Real dx = candidate->getPosition()->x - self->getPosition()->x;
		const Real dy = candidate->getPosition()->y - self->getPosition()->y;
		const Real distance = dx * dx + dy * dy;
		if (!nearest || distance < nearestDistance || (distance == nearestDistance && candidate->getID() < nearest->getID()))
		{
			nearest = candidate;
			nearestDistance = distance;
		}
	}
	const UnsignedInt options = NEED_TARGET_ALLY_OBJECT;
	if (nearest && TheActionManager->canDoSpecialPowerAtObject(self, nearest, CMD_FROM_AI, power, options))
		self->doSpecialPowerAtObject(power, nearest, options);
	return delay;
}

void BoredUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	UnsignedInt scanDeadline = friend_getNextCallFrame();
	xfer->xferUnsignedInt(&scanDeadline);
}
void BoredUpdate::xfer(Xfer* xfer) { UpdateModule::xfer(xfer); }
void BoredUpdate::loadPostProcess() { UpdateModule::loadPostProcess(); }

void ModelConditionSpecialAbilityUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "WhichSpecialPower", INI::parseInt, nullptr, offsetof(ModelConditionSpecialAbilityUpdateModuleData, m_whichSpecialPower) },
		{ "GenerateTerror", INI::parseBool, nullptr, offsetof(ModelConditionSpecialAbilityUpdateModuleData, m_generateTerror) },
		{ "GenerateUncontrollableFear", INI::parseBool, nullptr, offsetof(ModelConditionSpecialAbilityUpdateModuleData, m_generateUncontrollableFear) },
		{ "EmotionPulseRadius", INI::parseReal, nullptr, offsetof(ModelConditionSpecialAbilityUpdateModuleData, m_emotionPulseRadius) },
		{ "ObjectFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(ModelConditionSpecialAbilityUpdateModuleData, m_filter) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

ModelConditionSpecialAbilityUpdate::~ModelConditionSpecialAbilityUpdate() = default;

void ModelConditionSpecialAbilityUpdate::addAbilityConditions(ModelConditionFlags& flags) const
{
	SpecialAbilityUpdate::addAbilityConditions(flags);
	constexpr ModelConditionFlagType conditions[] = {MODELCONDITION_INVULNERABLE, MODELCONDITION_SPECIAL_POWER_1,
		MODELCONDITION_SPECIAL_POWER_2, MODELCONDITION_SPECIAL_POWER_3};
	const Int index = getModelConditionSpecialAbilityUpdateModuleData()->m_whichSpecialPower;
	if (index >= 0 && index < 4)
		flags.set(conditions[index]);
}

void ModelConditionSpecialAbilityUpdate::triggerAbilityEffect()
{
	SpecialAbilityUpdate::triggerAbilityEffect();
	const auto* data = getModelConditionSpecialAbilityUpdateModuleData();
	if (!(data->m_emotionPulseRadius > 0.0f) || !std::isfinite(data->m_emotionPulseRadius) ||
		!(data->m_generateTerror || data->m_generateUncontrollableFear))
		return;

	Object* self = getObject();
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(self->getPosition(), data->m_emotionPulseRadius,
		FROM_CENTER_2D, nullptr, ITER_FASTEST);
	MemoryPoolObjectHolder holder(iter);
	std::vector<Object*> targets;
	for (Object* target = iter->first(); target; target = iter->next())
	{
		if (self->getRelationship(target) == ENEMIES && !target->isEffectivelyDead() && data->m_filter.matchesObject(self, target))
			targets.push_back(target);
	}
	std::sort(targets.begin(), targets.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });
	for (Object* target : targets)
	{
		if (data->m_generateTerror)
			EmotionTrackerUpdate::signal(target, EMOTION_TERROR, self);
		if (data->m_generateUncontrollableFear)
			EmotionTrackerUpdate::signal(target, EMOTION_UNCONTROLLABLE_FEAR, self);
	}
}

void ModelConditionSpecialAbilityUpdate::crc(Xfer* xfer)
{
	SpecialAbilityUpdate::crc(xfer);
	crcRetailAbilityPhase(xfer);
}
void ModelConditionSpecialAbilityUpdate::xfer(Xfer* xfer) { SpecialAbilityUpdate::xfer(xfer); }
void ModelConditionSpecialAbilityUpdate::loadPostProcess() { SpecialAbilityUpdate::loadPostProcess(); }

void RousingSpeechUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] = {
		{ "UpdateInterval", INI::parseDurationUnsignedInt, nullptr, offsetof(RousingSpeechUpdateModuleData, m_interval) },
		{ "SpeechDuration", INI::parseDurationUnsignedInt, nullptr, offsetof(RousingSpeechUpdateModuleData, m_duration) },
		{ "BonusRadius", INI::parseReal, nullptr, offsetof(RousingSpeechUpdateModuleData, m_radius) },
		{ "CreateWave", INI::parseBool, nullptr, offsetof(RousingSpeechUpdateModuleData, m_createWave) },
		{ "WaveWidth", INI::parseReal, nullptr, offsetof(RousingSpeechUpdateModuleData, m_waveWidth) },
		{ "ModifierName", INI::parseAsciiString, nullptr, offsetof(RousingSpeechUpdateModuleData, m_modifier) },
		{ "ObjectFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(RousingSpeechUpdateModuleData, m_filter) },
		{ "LeaderFX", INI::parseFXList, nullptr, offsetof(RousingSpeechUpdateModuleData, m_leaderFX) },
		{ "FollowerFX", INI::parseFXList, nullptr, offsetof(RousingSpeechUpdateModuleData, m_followerFX) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	p.add(fields);
}

RousingSpeechUpdate::~RousingSpeechUpdate() = default;

void RousingSpeechUpdate::triggerAbilityEffect()
{
	SpecialAbilityUpdate::triggerAbilityEffect();
	if (!usesRotwkRules())
		return;

	const auto* data = getRousingSpeechUpdateModuleData();
	m_nextPulse = TheGameLogic->getFrame();
	m_speechEnd = m_nextPulse + std::max(data->m_duration, 1u);
	m_previousRadius = 0.0f;
	if (data->m_leaderFX)
		FXList::doFXObj(data->m_leaderFX, getObject());
}

UnsignedInt RousingSpeechUpdate::holdFramesAfterTrigger() const
{
	return std::max(SpecialAbilityUpdate::holdFramesAfterTrigger(), getRousingSpeechUpdateModuleData()->m_duration);
}

UpdateSleepTime RousingSpeechUpdate::update()
{
	const UpdateSleepTime sleep = SpecialAbilityUpdate::update();
	Object* self = getObject();
	const UnsignedInt now = TheGameLogic->getFrame();
	if (now >= m_speechEnd || self->isEffectivelyDead())
		return sleep;
	if (now < m_nextPulse)
		return std::min(sleep, UPDATE_SLEEP(m_nextPulse - now));

	const auto* data = getRousingSpeechUpdateModuleData();
	const Bool wave = data->m_createWave && data->m_waveWidth > 0.0f;
	const Real radius = wave ? std::min(data->m_radius, m_previousRadius + data->m_waveWidth) : data->m_radius;
	const Int list = AttributeModifiers::findList(data->m_modifier);
	if (radius <= 0.0f || list < 0)
		return sleep;

	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(self->getPosition(), radius, FROM_CENTER_2D);
	MemoryPoolObjectHolder holder(iter);
	std::vector<Object*> targets;
	for (Object* target = iter->first(); target; target = iter->next())
	{
		if (target == self || target->isEffectivelyDead() || target->isDestroyed() ||
			!AttributeModifiers::sidesMatch(self, target, &data->m_filter, AttributeModifiers::SIDES_ALLIES) ||
			!data->m_filter.matchesObject(self, target))
			continue;
		const Real dx = self->getPosition()->x - target->getPosition()->x;
		const Real dy = self->getPosition()->y - target->getPosition()->y;
		if (m_previousRadius == 0.0f || dx * dx + dy * dy > m_previousRadius * m_previousRadius)
			targets.push_back(target);
	}
	std::sort(targets.begin(), targets.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });
	for (Object* target : targets)
		if (AttributeModifiers::apply(target, list) && data->m_followerFX)
			FXList::doFXObj(data->m_followerFX, target);

	if (wave)
	{
		m_previousRadius = radius;
		if (radius >= data->m_radius)
		{
			m_speechEnd = 0;
			return sleep;
		}
	}
	const UnsignedInt interval = std::max(data->m_interval, 1u);
	m_nextPulse = now + interval;
	return std::min(sleep, UPDATE_SLEEP(interval));
}

void RousingSpeechUpdate::crc(Xfer* xfer)
{
	SpecialAbilityUpdate::crc(xfer);
	xfer->xferUnsignedInt(&m_speechEnd);
	xfer->xferUnsignedInt(&m_nextPulse);
	xfer->xferReal(&m_previousRadius);
}

void RousingSpeechUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	SpecialAbilityUpdate::xfer(xfer);
	xfer->xferUnsignedInt(&m_speechEnd);
	xfer->xferUnsignedInt(&m_nextPulse);
	xfer->xferReal(&m_previousRadius);
}

void RousingSpeechUpdate::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}

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

void WeaponFireSpecialAbilityUpdate::onStartWindup()
{
	const auto* data = getWeaponFireSpecialAbilityUpdateModuleData();
	if (!data->m_playWeaponPreFireFX || !data->m_specialWeapon)
		return;
	const Object* target = TheGameLogic->findObjectByID(getAbilityTargetID());
	if (getAbilityTargetID() != INVALID_ID && (!target || (data->m_needLivingTargets && target->isEffectivelyDead())))
		return;
	data->m_specialWeapon->playPreAttackFX(*getObject(), target, hasAbilityTargetPos() ? &getAbilityTargetPos() : nullptr);
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
	const ToggleMountedSpecialAbilityUpdateModuleData* data = getToggleMountedSpecialAbilityUpdateModuleData();
	if (data->m_mountedTemplate.isEmpty())
		setMounted(getObject(), !isMounted(getObject()));
	else if (usesRotwkRules())
		TheGameLogic->queueMountedTemplateSwap(getObject()->getID(), data,
			hasAbilityTargetPos() ? std::optional<Coord3D>(getAbilityTargetPos()) : std::nullopt);
}

void GameLogic::queueMountedTemplateSwap(ObjectID id, const ToggleMountedSpecialAbilityUpdateModuleData* data,
	std::optional<Coord3D> landingPosition)
{
	const auto match = [id](const MountedTemplateSwap& pending) { return pending.id == id; };
	if (std::none_of(m_mountedTemplateSwaps.begin(), m_mountedTemplateSwaps.end(), match))
		m_mountedTemplateSwaps.push_back({id, data, landingPosition});
}

void GameLogic::processMountedTemplateSwaps()
{
	struct Swap
	{
		ObjectID id;
		ObjectID producer;
		ObjectID builder;
		const ThingTemplate* target;
		const ThingTemplate* identity;
		Team* team;
		AsciiString originalTeamName;
		Matrix3D transform;
		AsciiString name;
		UpgradeMaskType upgrades;
		Real healthRatio;
		Real experience;
		Int rank;
		Int startingRank;
		Bool selected;
		Color indicatorColor;
		std::vector<std::pair<AsciiString, UnsignedInt>> timers;
		std::vector<AsciiString> executedCommands;
		Int stance;
		AsciiString stanceModifier;
		NameKeyType stanceTag;
		AttributeModifierHelper::TransferState modifiers;
		Object* source;
		Object::DominationState domination;
#if !RETAIL_COMPATIBLE_AIGROUP
		AIGroupPtr group;
#endif
	};

	decltype(m_mountedTemplateSwaps) pending;
	pending.swap(m_mountedTemplateSwaps);
	std::vector<Swap> swaps;
	static const NameKeyType commandUpgradeKey = NAMEKEY("DoCommandUpgrade");
	for (const auto& [id, data, landingPosition] : pending)
	{
		Object* source = findObjectByID(id);
		if (!source || source->isDestroyed() || source->isEffectivelyDead() || source->getContainedBy())
			continue;
		const ThingTemplate* definition = TheThingFactory->findTemplate(data->m_mountedTemplate, FALSE);
		const auto* target = definition ? static_cast<const ThingTemplate*>(definition->getFinalOverride()) : nullptr;
		if (!target || target == source->getTemplate() || target->isKindOf(KINDOF_DRAWABLE_ONLY))
			continue;

		const BodyModuleInterface* body = source->getBodyModule();
		const Real healthRatio = body && body->getMaxHealth() > 0.0f ? body->getHealth() / body->getMaxHealth() : 1.0f;
		const ExperienceTracker* tracker = source->getExperienceTracker();
		const Drawable* draw = source->getDrawable();
		Matrix3D transform = *source->getTransformMatrix();
		if (!target->isKindOf(KINDOF_GIANT_BIRD) && source->isKindOf(KINDOF_GIANT_BIRD))
		{
			Vector3 position = transform.Get_Translation();
			if (landingPosition)
			{
				position.X = landingPosition->x;
				position.Y = landingPosition->y;
			}
			position.Z = TheTerrainLogic->getGroundHeight(position.X, position.Y);
			transform = Matrix3D(Vector3(0.0f, 0.0f, 1.0f), source->getOrientation());
			transform.Set_Translation(position);
		}
		const StancesBehavior* stances = StancesBehavior::find(source);
		const AttributeModifierHelper* modifiers = source->getAttributeModifierHelper();
		Swap& swap = swaps.emplace_back(Swap{id, source->m_producerID, source->m_builderID, target,
			source->getHeroIdentityTemplate(), source->getTeam(), source->m_originalTeamName, transform, source->getName(),
			source->getObjectCompletedUpgradeMask(), healthRatio, tracker->getRetailExperience(), tracker->getRank(),
			tracker->getRetailStartingRank(), draw && draw->isSelected(), source->m_indicatorColor,
			std::move(source->m_mountedPowerTimers), std::move(source->m_mountedExecutedCommands),
			stances ? stances->getStance() : source->m_mountedStance,
			stances ? stances->getAppliedModifier() : AsciiString::TheEmptyString,
			stances ? stances->getModuleTagNameKey() : NAMEKEY_INVALID,
			modifiers ? modifiers->getTransferState() : AttributeModifierHelper::TransferState{}, source, {}
#if !RETAIL_COMPATIBLE_AIGROUP
			, source->m_group
#endif
		});
		for (BehaviorModule** module = source->getBehaviorModules(); *module; ++module)
		{
			if ((*module)->getModuleNameKey() != commandUpgradeKey)
				continue;
			const auto* upgrade = static_cast<const DoCommandUpgrade*>(*module);
			const AsciiString key = upgrade->getExecutionKey();
			const auto existing = std::find(swap.executedCommands.begin(), swap.executedCommands.end(), key);
			if (upgrade->isAlreadyUpgraded() && existing == swap.executedCommands.end())
				swap.executedCommands.push_back(key);
			else if (!upgrade->isAlreadyUpgraded() && existing != swap.executedCommands.end())
				swap.executedCommands.erase(existing);
		}
		for (const AsciiString& name : data->m_synchronizeTimerOnSpecialPower)
		{
			const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(name);
			const auto* module = source->getSpecialPowerModule(power);
			if (!module)
				continue;
			const auto match = [&name](const auto& timer) { return timer.first == name; };
			const auto existing = std::find_if(swap.timers.begin(), swap.timers.end(), match);
			if (existing == swap.timers.end())
				swap.timers.emplace_back(name, module->getReadyFrame());
			else
				existing->second = module->getReadyFrame();
		}
		destroyObject(source);
	}
	for (Swap& swap : swaps)
		swap.domination = swap.source->m_domination;
	processDestroyList();

	for (Swap& swap : swaps)
	{
		Object* replacement = TheThingFactory->newObjectFromExactTemplate(swap.target, swap.team, OBJECT_STATUS_MASK_NONE, swap.id,
			std::move(swap.executedCommands));
		replacement->setDominationState(swap.domination);
		replacement->m_mountedHeroIdentity = swap.identity;
		replacement->m_producerID = swap.producer;
		replacement->m_builderID = swap.builder;
		replacement->m_originalTeamName = swap.originalTeamName;
		TheScriptEngine->transferObjectName(swap.name, replacement);
		if (swap.indicatorColor)
			replacement->setCustomIndicatorColor(swap.indicatorColor);

		replacement->setTransformMatrix(&swap.transform);
		replacement->m_objectUpgradesCompleted.set(swap.upgrades);
		replacement->updateUpgradeModules();
		ExperienceTracker* tracker = replacement->getExperienceTracker();
		tracker->advanceRetailLevel(swap.experience, FALSE);
		tracker->setMinRetailRank(swap.rank);
		tracker->setRetailStartingRank(swap.startingRank);
		if (AttributeModifierHelper* modifiers = replacement->getAttributeModifierHelper())
			modifiers->inheritTransferState(std::move(swap.modifiers));
		replacement->m_mountedStance = swap.stance;
		if (StancesBehavior* stances = StancesBehavior::find(replacement))
		{
			const Int stance = swap.stance == AttributeModifierRules::STANCE_UNINITIALIZED ? stances->getStance() : swap.stance;
			stances->inheritStance(stance, swap.stanceModifier, swap.stanceTag);
		}
		else if (swap.stanceModifier.isNotEmpty())
			AttributeModifiers::remove(replacement, swap.stanceModifier, AttributeModifiers::ownerOf(replacement, swap.stanceTag));
		if (BodyModuleInterface* body = replacement->getBodyModule())
			body->internalChangeHealth(body->getMaxHealth() * swap.healthRatio - body->getHealth());

		replacement->m_mountedPowerTimers = std::move(swap.timers);
		for (const auto& [name, readyFrame] : replacement->m_mountedPowerTimers)
		{
			const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(name);
			if (auto* module = replacement->getSpecialPowerModule(power))
				module->setReadyFrame(readyFrame);
		}
#if !RETAIL_COMPATIBLE_AIGROUP
		if (swap.group)
			swap.group->add(replacement);
#endif
		if (swap.selected && TheInGameUI && replacement->getDrawable())
			TheInGameUI->selectDrawable(replacement->getDrawable());
	}
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

SummonReplacementSpecialAbilityUpdate::SummonReplacementSpecialAbilityUpdate(Thing* thing, const ModuleData* data)
	: ToggleMountedSpecialAbilityUpdate(thing, data)
{
}

SummonReplacementSpecialAbilityUpdate::~SummonReplacementSpecialAbilityUpdate() = default;

void SummonReplacementSpecialAbilityUpdate::crc(Xfer* xfer)
{
	ToggleMountedSpecialAbilityUpdate::crc(xfer);
}

void SummonReplacementSpecialAbilityUpdate::xfer(Xfer* xfer)
{
	ToggleMountedSpecialAbilityUpdate::xfer(xfer);
}

void SummonReplacementSpecialAbilityUpdate::loadPostProcess()
{
	ToggleMountedSpecialAbilityUpdate::loadPostProcess();
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
					power->doSpecialPowerAtLocation(&ownPos, INVALID_ANGLE, COMMAND_OPTION_INTERNAL_AUTO);
					break;
				case ActivateModuleEntry::POSITION_TARGET:
					power->doSpecialPowerAtLocation(&targetPos, INVALID_ANGLE, COMMAND_OPTION_INTERNAL_AUTO);
					break;
				default:
					power->doSpecialPower(COMMAND_OPTION_INTERNAL_AUTO);
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
	if (RetailFileLayers::enabled() && data->m_heroEffectDuration > 0)
		startTimedCondition(MODELCONDITION_HERO, data->m_heroEffectDuration);
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
		doSpecialPower(COMMAND_OPTION_INTERNAL_AUTO);
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
	const CommandButton* button = TheRetailCommandCatalog ? TheRetailCommandCatalog->findPowerButton(self, power) : nullptr;
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
	if (data->m_allowSelf && objectMatches(query.m_filter, self) && (!button || button->matchesRetailObjectTarget(self)))
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
		if (!objectMatches(query.m_filter, other) || (button && !button->matchesRetailObjectTarget(other)))
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
	{
		m_readySinceFrame = 0;
		return scan;
	}
	const CommandButton* button = TheRetailCommandCatalog ? TheRetailCommandCatalog->findPowerButton(self, data->m_specialAbility) : nullptr;
	if (button && ((button->getAutoAbilityDisallowedOnModelCondition().any() &&
		self->getLogicModelConditions().testForAny(button->getAutoAbilityDisallowedOnModelCondition())) ||
		!button->isModelConditionAllowed(self) || !button->isValidContainerForCommand(self)))
	{
		m_readySinceFrame = 0;
		return scan;
	}
	if (!TheSpecialPowerStore->canUseSpecialPower(self, data->m_specialAbility))
		return scan;
	const UnsignedInt now = TheGameLogic->getFrame();
	const UnsignedInt delayFrames = button
		? CommandButtonRules::autoDelayFrames(button->getAutoDelay(), LOGICFRAMES_PER_SECOND)
		: 0;
	if (delayFrames > 0)
	{
		if (m_readySinceFrame == 0)
			m_readySinceFrame = now;
		if (!CommandButtonRules::isAutoAbilityAllowed(false, true, delayFrames, m_readySinceFrame, now))
			return scan;
	}
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
	Bool casted = FALSE;
	for (const AutoAbilityQuery& query : data->m_queries)
	{
		Object* target = nullptr;
		if (queryMatches(query, options, target) && cast(options, target))
		{
			casted = TRUE;
			break;
		}
	}
	if (!casted && button && button->isTriggerWhenReady() &&
		!BitIsSet(options, COMMAND_OPTION_NEED_OBJECT_TARGET) && !BitIsSet(options, NEED_TARGET_POS))
	{
		casted = cast(options, self);
	}
	if (casted)
		m_readySinceFrame = 0;
	return scan;
}

void AutoAbilityBehavior::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xfer->xferInt(&m_toggledState);
	xfer->xferUnsignedInt(&m_idleSinceFrame);
	xfer->xferUnsignedInt(&m_readySinceFrame);
}

void AutoAbilityBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 2;
	xfer->xferVersion(&version, 2);
	UpdateModule::xfer(xfer);
	xfer->xferInt(&m_toggledState);
	xfer->xferUnsignedInt(&m_idleSinceFrame);
	if (version >= 2)
		xfer->xferUnsignedInt(&m_readySinceFrame);
	else
		m_readySinceFrame = 0;
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
	resetLastCastAccepted();
	if (obj && CommandButton::isRetailPowerObjectTargetAllowed(getObject(), getSpecialPowerTemplate(), obj))
		doSpecialPowerAtLocation(obj->getPosition(), INVALID_ANGLE, commandOptions);
}

void PlayerHealSpecialPower::doSpecialPowerAtLocation(const Coord3D* loc, Real angle, UnsignedInt commandOptions)
{
	if (!loc || m_pausedCount > 0 || getObject()->isDisabled() || !meetsRequiredRank() || !isRetailCastAllowed(commandOptions))
		return;
	const Coord3D center = *loc;
	m_pendingCenter = center;
	m_hasPendingCenter = TRUE;
	SpecialPowerModule::doSpecialPowerAtLocation(&center, angle, commandOptions);
	if (!wasLastCastAccepted())
	{
		m_hasPendingCenter = FALSE;
		return;
	}
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

SplitHordeSpecialPower::~SplitHordeSpecialPower() = default;
void SplitHordeSpecialPower::crc(Xfer* xfer) { SpecialPowerModule::crc(xfer); }
void SplitHordeSpecialPower::xfer(Xfer* xfer) { SpecialPowerModule::xfer(xfer); }
void SplitHordeSpecialPower::loadPostProcess() { SpecialPowerModule::loadPostProcess(); }

void SplitHordeSpecialPower::applyPowerEffect(const Coord3D*)
{
	if (auto* horde = const_cast<HordeContain*>(HordeContain::get(getObject())))
		horde->splitHorde();
}
