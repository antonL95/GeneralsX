#include "PreRTS.h"

#define DEFINE_DEATH_NAMES
#include "GameLogic/WeaponNugget.h"

#include <algorithm>
#include <cstdio>

#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/RetailTestCommands.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponNuggetRules.h"

namespace
{
constexpr UnsignedInt FILTER_SELF = 1u << 0;
constexpr UnsignedInt FILTER_ALLIES = 1u << 1;
constexpr UnsignedInt FILTER_ENEMIES = 1u << 2;
constexpr UnsignedInt FILTER_NEUTRALS = 1u << 3;
constexpr UnsignedInt FILTER_ALL = 1u << 10;
constexpr UnsignedInt FILTER_SAME_PLAYER = 1u << 13;
constexpr Int MAX_WARHEAD_DEPTH = 2;

Bool filterMatches(const SpecialPowerObjectFilter& filter, const Object* attacker, const Object* victim)
{
	const AsciiString& name = victim->getTemplate()->getName();
	const auto named = [&name](const AsciiString& other)
	{
		return stricmp(name.str(), other.str()) == 0;
	};
	const Relationship relationship = attacker ? attacker->getRelationship(victim) : NEUTRAL;
	WeaponNuggetRules::FilterFacts facts{};
	facts.ruleAll = (filter.m_rules & FILTER_ALL) != 0;
	facts.ruleNone = (filter.m_rules & SpecialPowerObjectFilter::RULE_NONE) != 0;
	facts.wantsSelf = (filter.m_rules & FILTER_SELF) != 0;
	facts.wantsAllies = (filter.m_rules & FILTER_ALLIES) != 0;
	facts.wantsEnemies = (filter.m_rules & FILTER_ENEMIES) != 0;
	facts.wantsNeutrals = (filter.m_rules & FILTER_NEUTRALS) != 0;
	facts.wantsSamePlayer = (filter.m_rules & FILTER_SAME_PLAYER) != 0;
	facts.isSelf = attacker == victim;
	facts.isAlly = relationship == ALLIES;
	facts.isEnemy = relationship == ENEMIES;
	facts.isNeutral = relationship == NEUTRAL;
	facts.isSamePlayer = attacker && attacker->getControllingPlayer() == victim->getControllingPlayer();
	facts.includedByName = std::any_of(filter.m_includeObjects.begin(), filter.m_includeObjects.end(), named);
	facts.excludedByName = std::any_of(filter.m_excludeObjects.begin(), filter.m_excludeObjects.end(), named);
	facts.hasIncludedKinds = filter.m_includeKinds.any();
	facts.includedByKind = victim->isAnyKindOf(filter.m_includeKinds);
	facts.excludedByKind = victim->isAnyKindOf(filter.m_excludeKinds);
	facts.hasIncludedNames = !filter.m_includeObjects.empty();
	return WeaponNuggetRules::filterMatches(facts);
}

Bool hasUpgrade(const Object* obj, const UpgradeTemplate* upgrade)
{
	if (!obj || !upgrade)
		return FALSE;
	if (obj->hasUpgrade(upgrade))
		return TRUE;
	const Player* player = obj->getControllingPlayer();
	return player && player->hasUpgradeComplete(upgrade);
}

ObjectID launcherOf(const Object* source)
{
	if (!source || !source->isKindOf(KINDOF_PROJECTILE))
		return INVALID_ID;
	for (BehaviorModule** module = source->getBehaviorModules(); *module; ++module)
	{
		if (ProjectileUpdateInterface* projectile = (*module)->getProjectileUpdateInterface())
			return projectile->projectileGetLauncherID();
	}
	return INVALID_ID;
}

ProjectileUpdateInterface* projectileInterface(Object* obj)
{
	for (BehaviorModule** module = obj->getBehaviorModules(); *module; ++module)
	{
		if (ProjectileUpdateInterface* projectile = (*module)->getProjectileUpdateInterface())
			return projectile;
	}
	return nullptr;
}

void parseUpgradeNames(INI* ini, void*, void* store, const void*)
{
	auto& names = *static_cast<std::vector<AsciiString>*>(store);
	for (const char* token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
		names.emplace_back(token);
}

void parseSpecialObjectFilter(INI* ini, void* instance, void*, const void*)
{
	auto& nugget = *static_cast<WeaponNugget*>(instance);
	SpecialPowerObjectFilter::parse(ini, nullptr, &nugget.m_specialObjectFilter, nullptr);
	nugget.m_hasSpecialObjectFilter = TRUE;
}

Real cosHalfArc(Real degrees)
{
	return Cos(degrees * 0.5f * PI / 180.0f);
}

void parseDamageArc(INI* ini, void* instance, void*, const void*)
{
	auto& nugget = *static_cast<WeaponNugget*>(instance);
	nugget.m_damageArc = INI::scanReal(ini->getNextToken());
	nugget.m_cosHalfDamageArc = cosHalfArc(nugget.m_damageArc);
}

void parseDamageScalar(INI* ini, void* instance, void*, const void*)
{
	WeaponNuggetScalar scalar;
	scalar.m_scalar = INI::scanPercentToReal(ini->getNextToken(ini->getSepsPercent()));
	if (const char* token = ini->getNextTokenOrNull())
		scalar.m_filter.addTokens(ini, token);
	else
		scalar.m_filter.m_rules = FILTER_ALL;
	static_cast<WeaponNugget*>(instance)->m_damageScalars.push_back(std::move(scalar));
}

const FieldParse s_commonFields[] =
{
	{ "SpecialObjectFilter", parseSpecialObjectFilter, nullptr, 0 },
	{ "RequiredUpgradeNames", parseUpgradeNames, nullptr, offsetof(WeaponNugget, m_requiredUpgradeNames) },
	{ "ForbiddenUpgradeNames", parseUpgradeNames, nullptr, offsetof(WeaponNugget, m_forbiddenUpgradeNames) },
	{ nullptr, nullptr, nullptr, 0 }
};

const FieldParse s_damageFields[] =
{
	{ "Damage", INI::parseReal, nullptr, offsetof(WeaponNugget, m_damage) },
	{ "Radius", INI::parseReal, nullptr, offsetof(WeaponNugget, m_radius) },
	{ "DelayTime", INI::parseDurationUnsignedInt, nullptr, offsetof(WeaponNugget, m_delayFrames) },
	{ "DamageType", DamageTypeFlags::parseSingleBitFromINI, nullptr, offsetof(WeaponNugget, m_damageType) },
	{ "DamageFXType", DamageTypeFlags::parseSingleBitFromINI, nullptr, offsetof(WeaponNugget, m_damageFXType) },
	{ "DeathType", INI::parseIndexList, TheDeathNames, offsetof(WeaponNugget, m_deathType) },
	{ "DamageSubType", INI::parseAsciiString, nullptr, offsetof(WeaponNugget, m_damageSubType) },
	{ "DamageArc", parseDamageArc, nullptr, 0 },
	{ "DamageArcInverted", INI::parseBool, nullptr, offsetof(WeaponNugget, m_damageArcInverted) },
	{ "DamageScalar", parseDamageScalar, nullptr, 0 },
	{ "FlankingBonus", INI::parsePercentToReal, nullptr, offsetof(WeaponNugget, m_flankingBonus) },
	{ "AcceptDamageAdd", INI::parseBool, nullptr, offsetof(WeaponNugget, m_acceptDamageAdd) },
	{ "DamageSpeed", INI::parseVelocityReal, nullptr, offsetof(WeaponNugget, m_damageSpeed) },
	{ "DamageMaxHeight", INI::parseReal, nullptr, offsetof(WeaponNugget, m_damageMaxHeight) },
	{ "DamageMaxHeightAboveTerrain", INI::parseReal, nullptr, offsetof(WeaponNugget, m_damageMaxHeightAboveTerrain) },
	{ "DamageTaperOff", INI::parseReal, nullptr, offsetof(WeaponNugget, m_damageTaperOff) },
	{ "MinRadius", INI::parseReal, nullptr, offsetof(WeaponNugget, m_minRadius) },
	{ "CylinderAOE", INI::parseBool, nullptr, offsetof(WeaponNugget, m_cylinderAOE) },
	{ "DrainLife", INI::parseBool, nullptr, offsetof(WeaponNugget, m_drainLife) },
	{ "DrainLifeMultiplier", INI::parseReal, nullptr, offsetof(WeaponNugget, m_drainLifeMultiplier) },
	{ "ForceKillObjectFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(WeaponNugget, m_forceKillObjectFilter) },
	{ nullptr, nullptr, nullptr, 0 }
};

const FieldParse s_dotFields[] =
{
	{ "DamageInterval", INI::parseDurationUnsignedInt, nullptr, offsetof(WeaponNugget, m_damageIntervalFrames) },
	{ "DamageDuration", INI::parseDurationUnsignedInt, nullptr, offsetof(WeaponNugget, m_damageDurationFrames) },
	{ nullptr, nullptr, nullptr, 0 }
};

const FieldParse s_projectileFields[] =
{
	{ "ProjectileTemplateName", INI::parseAsciiString, nullptr, offsetof(WeaponNugget, m_projectileTemplateName) },
	{ "WarheadTemplateName", INI::parseAsciiString, nullptr, offsetof(WeaponNugget, m_warheadTemplateName) },
	{ "AlwaysAttackHereOffset", INI::parseCoord3D, nullptr, offsetof(WeaponNugget, m_alwaysAttackHereOffset) },
	{ "UseAlwaysAttackOffset", INI::parseBool, nullptr, offsetof(WeaponNugget, m_useAlwaysAttackOffset) },
	{ "WeaponLaunchBoneSlotOverride", INI::parseAsciiString, nullptr, offsetof(WeaponNugget, m_launchBoneSlotOverride) },
	{ nullptr, nullptr, nullptr, 0 }
};

const FieldParse s_metaImpactFields[] =
{
	{ "HeroResist", INI::parseReal, nullptr, offsetof(WeaponNugget, m_heroResist) },
	{ "ShockWaveAmount", INI::parseReal, nullptr, offsetof(WeaponNugget, m_shockWaveAmount) },
	{ "ShockWaveRadius", INI::parseReal, nullptr, offsetof(WeaponNugget, m_shockWaveRadius) },
	{ "ShockWaveTaperOff", INI::parseReal, nullptr, offsetof(WeaponNugget, m_shockWaveTaperOff) },
	{ "ShockWaveArc", INI::parseReal, nullptr, offsetof(WeaponNugget, m_shockWaveArc) },
	{ "ShockWaveArcInverted", INI::parseBool, nullptr, offsetof(WeaponNugget, m_shockWaveArcInverted) },
	{ "ShockWaveZMult", INI::parseReal, nullptr, offsetof(WeaponNugget, m_shockWaveZMult) },
	{ "ShockWaveSpeed", INI::parseVelocityReal, nullptr, offsetof(WeaponNugget, m_shockWaveSpeed) },
	{ "InvertShockWave", INI::parseBool, nullptr, offsetof(WeaponNugget, m_invertShockWave) },
	{ "DelayTime", INI::parseDurationUnsignedInt, nullptr, offsetof(WeaponNugget, m_delayFrames) },
	{ "FlipDirection", INI::parseBool, nullptr, offsetof(WeaponNugget, m_flipDirection) },
	{ "OnlyWhenJustDied", INI::parseBool, nullptr, offsetof(WeaponNugget, m_onlyWhenJustDied) },
	{ "KillObjectFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(WeaponNugget, m_killObjectFilter) },
	{ "ShockWaveClearRadius", INI::parseBool, nullptr, offsetof(WeaponNugget, m_shockWaveClearRadius) },
	{ "ShockWaveClearMult", INI::parseReal, nullptr, offsetof(WeaponNugget, m_shockWaveClearMult) },
	{ "ShockWaveClearFlingHeight", INI::parseInt, nullptr, offsetof(WeaponNugget, m_shockWaveClearFlingHeight) },
	{ "CyclonicFactor", INI::parseReal, nullptr, offsetof(WeaponNugget, m_cyclonicFactor) },
	{ "AffectHordes", INI::parseBool, nullptr, offsetof(WeaponNugget, m_affectHordes) },
	{ nullptr, nullptr, nullptr, 0 }
};

const FieldParse s_oclFields[] =
{
	{ "WeaponOCLName", INI::parseAsciiString, nullptr, offsetof(WeaponNugget, m_oclName) },
	{ nullptr, nullptr, nullptr, 0 }
};

const FieldParse s_attributeModifierFields[] =
{
	{ "AttributeModifier", INI::parseAsciiString, nullptr, offsetof(WeaponNugget, m_attributeModifier) },
	{ "DamageFXType", DamageTypeFlags::parseSingleBitFromINI, nullptr, offsetof(WeaponNugget, m_damageFXType) },
	{ "AntiCategories", parseUpgradeNames, nullptr, offsetof(WeaponNugget, m_antiCategories) },
	{ "Radius", INI::parseReal, nullptr, offsetof(WeaponNugget, m_radius) },
	{ "AffectHordeMembers", INI::parseBool, nullptr, offsetof(WeaponNugget, m_affectHordeMembers) },
	{ nullptr, nullptr, nullptr, 0 }
};

void parseNugget(INI* ini, void* store, WeaponNuggetType type, const FieldParse* fields, const FieldParse* extraFields = nullptr)
{
	WeaponNugget nugget(type);
	MultiIniFieldParse parse;
	parse.add(s_commonFields);
	parse.add(fields);
	if (extraFields)
		parse.add(extraFields);
	ini->initFromINIMulti(&nugget, parse);
	static_cast<std::vector<WeaponNugget>*>(store)->push_back(std::move(nugget));
}

const char* nuggetTypeName(WeaponNuggetType type)
{
	switch (type)
	{
		case WEAPON_NUGGET_DAMAGE: return "DamageNugget";
		case WEAPON_NUGGET_DOT: return "DOTNugget";
		case WEAPON_NUGGET_PROJECTILE: return "ProjectileNugget";
		case WEAPON_NUGGET_META_IMPACT: return "MetaImpactNugget";
		case WEAPON_NUGGET_OCL: return "WeaponOCLNugget";
		case WEAPON_NUGGET_ATTRIBUTE_MODIFIER: return "AttributeModifierNugget";
	}
	return "Nugget";
}

}

void WeaponNugget::parseDamageNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_DAMAGE, s_damageFields);
}

void WeaponNugget::parseDOTNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_DOT, s_damageFields, s_dotFields);
}

void WeaponNugget::parseProjectileNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_PROJECTILE, s_projectileFields);
}

void WeaponNugget::parseMetaImpactNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_META_IMPACT, s_metaImpactFields);
}

void WeaponNugget::parseWeaponOCLNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_OCL, s_oclFields);
}

void WeaponNugget::parseAttributeModifierNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_ATTRIBUTE_MODIFIER, s_attributeModifierFields);
}

void WeaponNugget::postProcessLoad()
{
	m_requiredUpgrades.clear();
	m_forbiddenUpgrades.clear();
	for (const AsciiString& name : m_requiredUpgradeNames)
	{
		if (const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(name))
			m_requiredUpgrades.push_back(upgrade);
	}
	for (const AsciiString& name : m_forbiddenUpgradeNames)
	{
		if (const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(name))
			m_forbiddenUpgrades.push_back(upgrade);
	}
	if (!m_projectileTemplateName.isEmpty())
		m_projectileTemplate = TheThingFactory->findTemplate(m_projectileTemplateName, FALSE);
	if (!m_warheadTemplateName.isEmpty())
		m_warheadTemplate = TheWeaponStore->findWeaponTemplate(m_warheadTemplateName);
	if (!m_oclName.isEmpty())
		m_ocl = TheObjectCreationListStore->findObjectCreationList(m_oclName.str());
}

Bool WeaponNugget::isEnabledFor(const Object* attacker) const
{
	if (m_requiredUpgradeNames.empty() && m_forbiddenUpgradeNames.empty())
		return TRUE;
	const auto present = [attacker](const UpgradeTemplate* upgrade) { return hasUpgrade(attacker, upgrade); };
	return WeaponNuggetRules::upgradeGateOpen(!m_requiredUpgradeNames.empty(),
		std::any_of(m_requiredUpgrades.begin(), m_requiredUpgrades.end(), present),
		std::any_of(m_forbiddenUpgrades.begin(), m_forbiddenUpgrades.end(), present));
}

Bool WeaponNugget::affects(const Object* attacker, const Object* victim) const
{
	return !m_hasSpecialObjectFilter || filterMatches(m_specialObjectFilter, attacker, victim);
}

Real WeaponNugget::scalarFor(const Object* attacker, const Object* victim) const
{
	std::vector<WeaponNuggetRules::Scalar> scalars;
	scalars.reserve(m_damageScalars.size());
	for (const WeaponNuggetScalar& scalar : m_damageScalars)
		scalars.push_back({scalar.m_scalar, filterMatches(scalar.m_filter, attacker, victim)});
	return WeaponNuggetRules::scalarProduct(scalars);
}

Bool WeaponTemplate::hasProjectileNuggets() const
{
	return std::any_of(m_nuggets.begin(), m_nuggets.end(),
		[](const WeaponNugget& nugget) { return nugget.m_type == WEAPON_NUGGET_PROJECTILE; });
}

Bool WeaponTemplate::hasImpactNuggets() const
{
	return std::any_of(m_nuggets.begin(), m_nuggets.end(), [](const WeaponNugget& nugget) { return nugget.isImpactNugget(); });
}

Bool WeaponTemplate::nuggetsCanDamage(Int depth) const
{
	for (const WeaponNugget& nugget : m_nuggets)
	{
		if (nugget.dealsDamage() && nugget.m_damage > 0.0f)
			return TRUE;
		if (nugget.m_type == WEAPON_NUGGET_PROJECTILE && nugget.m_warheadTemplate && depth < MAX_WARHEAD_DEPTH &&
			nugget.m_warheadTemplate->nuggetsCanDamage(depth + 1))
			return TRUE;
	}
	return FALSE;
}

Real WeaponTemplate::estimateNuggetDamage(const Object* sourceObj, const Object* victimObj, const WeaponBonus& bonus, Int depth) const
{
	Real total = 0.0f;
	for (const WeaponNugget& nugget : m_nuggets)
	{
		if (!nugget.isEnabledFor(sourceObj))
			continue;
		if (nugget.m_type == WEAPON_NUGGET_PROJECTILE)
		{
			if (nugget.m_warheadTemplate && depth < MAX_WARHEAD_DEPTH)
				total += nugget.m_warheadTemplate->estimateNuggetDamage(sourceObj, victimObj, bonus, depth + 1);
			continue;
		}
		if (!nugget.dealsDamage() || nugget.m_damage <= 0.0f)
			continue;
		const Real amount = nugget.m_damage * bonus.getField(WeaponBonus::DAMAGE);
		if (!victimObj)
		{
			total += amount;
			continue;
		}
		if (!nugget.affects(sourceObj, victimObj))
			continue;
		DamageInfoInput damageInfo;
		damageInfo.m_damageType = nugget.m_damageType;
		damageInfo.m_deathType = nugget.m_deathType;
		damageInfo.m_sourceID = sourceObj->getID();
		damageInfo.m_amount = amount * nugget.scalarFor(sourceObj, victimObj);
		total += victimObj->estimateDamage(damageInfo);
	}
	return total;
}

void WeaponTemplate::launchNuggetProjectiles(const Object* sourceObj, WeaponSlotType wslot, Int specificBarrelToUse,
	Object* victimObj, const Coord3D& destination, Weapon* firingWeapon, ObjectID* projectileID) const
{
	Player* owner = sourceObj->getControllingPlayer();
	if (!owner)
		return;
	const VeterancyLevel veterancy = sourceObj->getVeterancyLevel();
	for (size_t index = 0; index < m_nuggets.size(); ++index)
	{
		const WeaponNugget& nugget = m_nuggets[index];
		if (nugget.m_type != WEAPON_NUGGET_PROJECTILE || !nugget.m_projectileTemplate || !nugget.m_warheadTemplate)
			continue;
		if (!nugget.isEnabledFor(sourceObj) || (victimObj && !nugget.affects(sourceObj, victimObj)))
			continue;

		Object* projectile = TheThingFactory->newObject(nugget.m_projectileTemplate, owner->getDefaultTeam());
		projectile->setProducer(sourceObj);
		if (projectileID)
			*projectileID = projectile->getID();
		firingWeapon->newProjectileFired(sourceObj, projectile, victimObj, &destination);

		if (ProjectileUpdateInterface* pui = projectileInterface(projectile))
		{
			pui->projectileSetLaunchWeapon(this);
			pui->projectileLaunchAtObjectOrPosition(victimObj, &destination, sourceObj, wslot, specificBarrelToUse,
				nugget.m_warheadTemplate, m_projectileExhausts[veterancy]);
		}
		else
			projectile->setPosition(&destination);

		if (RetailTestCommands::tracesCombat())
			std::fprintf(stderr, "TEST projectile frame=%u weapon=%s nugget=%u projectile=%s id=%u warhead=%s source=%u victim=%u dest=%.1f,%.1f\n",
				TheGameLogic->getFrame(), m_name.str(), static_cast<unsigned>(index), nugget.m_projectileTemplate->getName().str(),
				static_cast<unsigned>(projectile->getID()), nugget.m_warheadTemplate->getName().str(),
				static_cast<unsigned>(sourceObj->getID()), static_cast<unsigned>(victimObj ? victimObj->getID() : INVALID_ID),
				destination.x, destination.y);
	}
}

void WeaponTemplate::dealNuggetsAtImpact(ObjectID sourceID, ObjectID victimID, const Coord3D* pos, const WeaponBonus& bonus) const
{
	const Object* source = TheGameLogic->findObjectByID(sourceID);
	const ObjectID launcherID = launcherOf(source);
	const Object* launcher = launcherID != INVALID_ID ? TheGameLogic->findObjectByID(launcherID) : nullptr;
	const Object* attacker = launcher ? launcher : source;
	const ObjectID nuggetSourceID = launcher ? launcherID : sourceID;
	if (RetailTestCommands::tracesCombat())
		std::fprintf(stderr, "TEST impact frame=%u weapon=%s source=%u victim=%u pos=%.1f,%.1f bonus=%.2f,%.2f\n", TheGameLogic->getFrame(),
			m_name.str(), static_cast<unsigned>(nuggetSourceID), static_cast<unsigned>(victimID), pos->x, pos->y,
			bonus.getField(WeaponBonus::DAMAGE), bonus.getField(WeaponBonus::RADIUS));

	for (size_t index = 0; index < m_nuggets.size(); ++index)
	{
		const WeaponNugget& nugget = m_nuggets[index];
		if (!nugget.isImpactNugget() || !nugget.isEnabledFor(attacker))
			continue;
		if (nugget.m_delayFrames == 0)
			dealNuggetDamage(static_cast<Int>(index), nuggetSourceID, victimID, pos, bonus);
		else if (TheWeaponStore)
			TheWeaponStore->setDelayedDamage(this, pos, TheGameLogic->getFrame() + nugget.m_delayFrames, nuggetSourceID, victimID,
				bonus, static_cast<Int>(index));
	}
}

void WeaponTemplate::dealNuggetDamage(Int nuggetIndex, ObjectID sourceID, ObjectID victimID, const Coord3D* pos, const WeaponBonus& bonus,
	Bool dotTick) const
{
	const WeaponNugget& nugget = m_nuggets[nuggetIndex];
	Object* source = TheGameLogic->findObjectByID(sourceID);
	Object* primaryVictim = victimID ? TheGameLogic->findObjectByID(victimID) : nullptr;
	if (primaryVictim)
	{
		if (const HordeContain* enemy = HordeContain::get(primaryVictim))
		{
			const Coord3D* fromPos = source ? source->getPosition() : pos;
			primaryVictim = fromPos ? enemy->findNearestLivingMember(*fromPos) : nullptr;
			victimID = primaryVictim ? primaryVictim->getID() : INVALID_ID;
		}
	}
	if (primaryVictim)
		pos = primaryVictim->getPosition();
	if (!pos || (dotTick && (!primaryVictim || primaryVictim->isEffectivelyDead())))
		return;

	if (nugget.m_type == WEAPON_NUGGET_OCL)
	{
		if (nugget.m_ocl && source)
			ObjectCreationList::create(nugget.m_ocl, source, pos, pos, source->getOrientation());
		return;
	}

	const auto damage = [&](Object* victim, Bool killSelf)
	{
		const Real scalar = nugget.scalarFor(source, victim);
		const Real amount = killSelf ? HUGE_DAMAGE_AMOUNT : nugget.m_damage * bonus.getField(WeaponBonus::DAMAGE) * scalar;
		if (amount <= 0.0f)
			return;

		DamageInfo damageInfo;
		damageInfo.in.m_damageType = nugget.m_damageType;
		damageInfo.in.m_deathType = nugget.m_deathType;
		damageInfo.in.m_sourceID = sourceID;
		damageInfo.in.m_sourcePlayerMask = source && source->getControllingPlayer() ? source->getControllingPlayer()->getPlayerMask() : 0;
		damageInfo.in.m_amount = amount;
		const Real healthBefore = victim->getBodyModule() ? victim->getBodyModule()->getHealth() : 0.0f;
		victim->attemptDamage(&damageInfo);

		if (RetailTestCommands::tracesCombat())
			std::fprintf(stderr, "TEST damage frame=%u weapon=%s nugget=%d kind=%s type=%s death=%s source=%u victim=%u(%s) base=%.2f scalar=%.3f amount=%.2f dealt=%.2f health=%.2f->%.2f\n",
				TheGameLogic->getFrame(), m_name.str(), nuggetIndex, nuggetTypeName(nugget.m_type),
				DamageTypeFlags::getNameFromSingleBit(nugget.m_damageType), TheDeathNames[nugget.m_deathType],
				static_cast<unsigned>(sourceID), static_cast<unsigned>(victim->getID()), victim->getTemplate()->getName().str(),
				nugget.m_damage, scalar, amount, damageInfo.out.m_actualDamageDealt, healthBefore,
				victim->getBodyModule() ? victim->getBodyModule()->getHealth() : 0.0f);
	};

	if (dotTick)
	{
		damage(primaryVictim, FALSE);
		return;
	}

	const Real radius = nugget.m_radius * bonus.getField(WeaponBonus::RADIUS);
	const Bool fullArc = WeaponNuggetRules::isFullArc(nugget.m_damageArc);
	const std::vector<UnsignedInt> dotTicks = nugget.m_type == WEAPON_NUGGET_DOT ?
		WeaponNuggetRules::dotTickOffsets(0, nugget.m_damageIntervalFrames, nugget.m_damageDurationFrames) : std::vector<UnsignedInt>();

	SimpleObjectIterator* iter = nullptr;
	Object* curVictim = primaryVictim;
	Real curVictimDistSqr = 0.0f;
	if (radius > 0.0f)
	{
		iter = ThePartitionManager->iterateObjectsInRange(pos, radius, FROM_BOUNDINGSPHERE_3D);
		curVictim = iter->firstWithNumeric(&curVictimDistSqr);
	}
	MemoryPoolObjectHolder hold(iter);

	for (; curVictim != nullptr; curVictim = iter ? iter->nextWithNumeric(&curVictimDistSqr) : nullptr)
	{
		if (HordeContain::get(curVictim))
			continue;
		if (curVictim != primaryVictim && curVictim->isKindOf(KINDOF_PROJECTILE))
			continue;
		const RadiusDamageVerdict verdict = judgeRadiusDamageVictim(source, curVictim, primaryVictim, m_affectsMask);
		if (verdict == RADIUS_DAMAGE_SKIP || !nugget.affects(source, curVictim))
			continue;
		if (!fullArc || nugget.m_damageArcInverted)
		{
			if (!source)
				continue;
			const Vector3 facing = source->getTransformMatrix()->Get_X_Vector();
			if (!WeaponNuggetRules::withinArc(facing.X, facing.Y, curVictim->getPosition()->x - source->getPosition()->x,
				curVictim->getPosition()->y - source->getPosition()->y, nugget.m_cosHalfDamageArc, fullArc, nugget.m_damageArcInverted))
				continue;
		}

		damage(curVictim, verdict == RADIUS_DAMAGE_KILL_SELF);
		for (size_t tick = 1; tick < dotTicks.size() && TheWeaponStore; ++tick)
			TheWeaponStore->setDelayedDamage(this, curVictim->getPosition(), TheGameLogic->getFrame() + dotTicks[tick], sourceID,
				curVictim->getID(), bonus, nuggetIndex, TRUE);
	}
}
