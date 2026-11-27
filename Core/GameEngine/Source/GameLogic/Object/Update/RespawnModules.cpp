#include "PreRTS.h"

#include "GameLogic/Module/RespawnModules.h"

#include <algorithm>

#include "Common/ModelState.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/CastleRules.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/HeroRoster.h"
#include "GameLogic/Object.h"

namespace
{
UnsignedInt msecsToFrames(UnsignedInt msecs)
{
	return static_cast<UnsignedInt>(WWMath::Ceil(ConvertDurationFromMsecsToFrames(static_cast<Real>(msecs))));
}

Bool templateMatches(const SpecialPowerObjectFilter &filter, const ThingTemplate &thing)
{
	const AsciiString &name = thing.getName();
	const auto named = [&name](const AsciiString &other)
	{
		return stricmp(name.str(), other.str()) == 0;
	};
	const CastleRules::FilterFacts facts{
		std::any_of(filter.m_includeObjects.begin(), filter.m_includeObjects.end(), named),
		std::any_of(filter.m_excludeObjects.begin(), filter.m_excludeObjects.end(), named),
		thing.isAnyKindOf(filter.m_excludeKinds),
		filter.m_includeKinds.any(),
		thing.isAnyKindOf(filter.m_includeKinds),
		(filter.m_rules & SpecialPowerObjectFilter::RULE_NONE) != 0};
	return CastleRules::filterMatches(facts);
}
}

void RespawnBodyModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ActiveBodyModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "PermanentlyKilledByFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(RespawnBodyModuleData, m_permanentlyKilledByFilter) },
		{ "CanRespawn", INI::parseBool, nullptr, offsetof(RespawnBodyModuleData, m_canRespawn) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

RespawnBody::RespawnBody(Thing *thing, const ModuleData *moduleData) : ActiveBody(thing, moduleData)
{
}

RespawnBody::~RespawnBody()
{
}

Bool RespawnBody::isKilledByFilter(const DamageInfo *damageInfo) const
{
	const Object *source = TheGameLogic ? TheGameLogic->findObjectByID(damageInfo->in.m_sourceID) : nullptr;
	return source && templateMatches(getRespawnBodyModuleData()->m_permanentlyKilledByFilter, *source->getTemplate());
}

void RespawnBody::attemptDamage(DamageInfo *damageInfo)
{
	Object *obj = getObject();
	const Bool wasAlive = !obj->isEffectivelyDead();
	const Bool killedByFilter = damageInfo && isKilledByFilter(damageInfo);
	ActiveBody::attemptDamage(damageInfo);
	if (wasAlive && obj->isEffectivelyDead())
		HeroRoster::recordDeath(obj, HeroRules::deathOutcome(TRUE, getRespawnBodyModuleData()->m_canRespawn, killedByFilter));
}

void RespawnBody::crc(Xfer *xfer)
{
	ActiveBody::crc(xfer);
}

void RespawnBody::xfer(Xfer *xfer)
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	ActiveBody::xfer(xfer);
}

void RespawnBody::loadPostProcess()
{
	ActiveBody::loadPostProcess();
}

void RespawnUpdateModuleData::parseRespawnRules(INI *ini, void *instance, void *, const void *)
{
	RespawnUpdateModuleData *data = static_cast<RespawnUpdateModuleData *>(instance);
	data->m_autoSpawn = INI::scanBool(ini->getNextSubToken("AutoSpawn"));
	data->m_rules.cost = INI::scanInt(ini->getNextSubToken("Cost"));
	data->m_rules.frames = msecsToFrames(INI::scanUnsignedInt(ini->getNextSubToken("Time")));
	data->m_respawnHealthPercent = static_cast<Int>(INI::scanPercentToReal(ini->getNextSubToken("Health")) * 100.0f + 0.5f);
}

void RespawnUpdateModuleData::parseRespawnEntry(INI *ini, void *instance, void *, const void *)
{
	RespawnUpdateModuleData *data = static_cast<RespawnUpdateModuleData *>(instance);
	HeroRules::ReviveEntry entry;
	entry.level = INI::scanInt(ini->getNextSubToken("Level"));
	entry.cost = INI::scanInt(ini->getNextSubToken("Cost"));
	entry.frames = msecsToFrames(INI::scanUnsignedInt(ini->getNextSubToken("Time")));
	data->m_entries.push_back(entry);
}

void RespawnUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "DeathAnim", ModelConditionFlags::parseSingleBitFromINI, nullptr, offsetof(RespawnUpdateModuleData, m_deathAnim) },
		{ "DeathFX", INI::parseFXList, nullptr, offsetof(RespawnUpdateModuleData, m_deathFX) },
		{ "DeathAnimationTime", INI::parseDurationUnsignedInt, nullptr, offsetof(RespawnUpdateModuleData, m_deathAnimationTime) },
		{ "InitialSpawnAnim", ModelConditionFlags::parseSingleBitFromINI, nullptr, offsetof(RespawnUpdateModuleData, m_initialSpawnAnim) },
		{ "InitialSpawnFX", INI::parseFXList, nullptr, offsetof(RespawnUpdateModuleData, m_initialSpawnFX) },
		{ "InitialSpawnAnimationTime", INI::parseDurationUnsignedInt, nullptr, offsetof(RespawnUpdateModuleData, m_initialSpawnAnimationTime) },
		{ "RespawnAnim", ModelConditionFlags::parseSingleBitFromINI, nullptr, offsetof(RespawnUpdateModuleData, m_respawnAnim) },
		{ "RespawnFX", INI::parseFXList, nullptr, offsetof(RespawnUpdateModuleData, m_respawnFX) },
		{ "RespawnAnimationTime", INI::parseDurationUnsignedInt, nullptr, offsetof(RespawnUpdateModuleData, m_respawnAnimationTime) },
		{ "AutoRespawnAtObjectFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(RespawnUpdateModuleData, m_autoRespawnAtObjectFilter) },
		{ "RespawnRules", RespawnUpdateModuleData::parseRespawnRules, nullptr, 0 },
		{ "RespawnEntry", RespawnUpdateModuleData::parseRespawnEntry, nullptr, 0 },
		{ "ButtonImage", INI::parseAsciiString, nullptr, offsetof(RespawnUpdateModuleData, m_buttonImage) },
		{ "RespawnAsTemplate", INI::parseAsciiString, nullptr, offsetof(RespawnUpdateModuleData, m_respawnAsTemplate) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

RespawnUpdate::RespawnUpdate(Thing *thing, const ModuleData *moduleData) : UpdateModule(thing, moduleData)
{
	setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
}

RespawnUpdate::~RespawnUpdate()
{
}

void RespawnUpdate::markRevived(Int healthPercent)
{
	m_revived = TRUE;
	if (healthPercent > 0 && healthPercent < 100 && getObject()->getBodyModule())
		getObject()->getBodyModule()->setInitialHealth(healthPercent);
}

const FXList* RespawnUpdate::spawnFX() const
{
	const RespawnUpdateModuleData* data = getRespawnUpdateModuleData();
	return m_revived ? data->m_respawnFX : data->m_initialSpawnFX;
}

UpdateSleepTime RespawnUpdate::update()
{
	const RespawnUpdateModuleData *data = getRespawnUpdateModuleData();
	Object *obj = getObject();
	if (m_spawnHandled)
	{
		if (m_animCondition >= 0)
		{
			obj->clearModelConditionState(static_cast<ModelConditionFlagType>(m_animCondition));
			m_animCondition = -1;
		}
		return UPDATE_SLEEP_FOREVER;
	}

	m_spawnHandled = TRUE;
	FXList::doFXPos(spawnFX(), obj->getPosition(), nullptr, 0.0f, nullptr, 0.0f, obj);
	const Int condition = m_revived ? data->m_respawnAnim : data->m_initialSpawnAnim;
	const UnsignedInt frames = m_revived ? data->m_respawnAnimationTime : data->m_initialSpawnAnimationTime;
	if (condition < 0 || frames == 0)
		return UPDATE_SLEEP_FOREVER;
	m_animCondition = condition;
	obj->setModelConditionState(static_cast<ModelConditionFlagType>(condition));
	return UPDATE_SLEEP(frames);
}

void RespawnUpdate::onDie(const DamageInfo *)
{
	const Object* source = getObject();
	FXList::doFXPos(getRespawnUpdateModuleData()->m_deathFX, source->getPosition(), nullptr, 0.0f,
		nullptr, 0.0f, RetailFileLayers::enabled() ? source : nullptr);
}

void RespawnUpdate::crc(Xfer *xfer)
{
	UpdateModule::crc(xfer);
}

void RespawnUpdate::xfer(Xfer *xfer)
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	UpdateModule::xfer(xfer);
	xfer->xferInt(&m_animCondition);
	xfer->xferBool(&m_spawnHandled);
	xfer->xferBool(&m_revived);
}

void RespawnUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
