#include "PreRTS.h"

#define DEFINE_DEATH_NAMES
#define DEFINE_WEAPONSLOTTYPE_NAMES
#include "GameLogic/WeaponNugget.h"

#include <algorithm>
#include <cstdio>
#include <set>

#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/RotwkDuration.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/FXList.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/DeletionUpdate.h"
#include "GameLogic/Module/FireWeaponUpdate.h"
#include "GameLogic/Module/GiantBirdAIUpdate.h"
#include "GameLogic/Module/DumbProjectileBehavior.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/ObjectCreationUpgrade.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/RetailGate.h"
#include "GameLogic/Module/RotwkModifierModules.h"
#include "GameLogic/Module/SpawnBehavior.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/RetailBuildingEvents.h"
#include "GameLogic/RetailTestCommands.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponNuggetRules.h"

namespace
{
constexpr UnsignedInt FILTER_ALL = 1u << 10;
constexpr Int MAX_WARHEAD_DEPTH = 2;

RetailDamageFXTrigger damageFXTrigger(const AsciiString& name)
{
	if (name.compareNoCase(RetailDamageFXTriggerNames[RETAIL_DAMAGE_FX_CATAPULT_ROCK]) == 0)
		return RETAIL_DAMAGE_FX_CATAPULT_ROCK;
	if (name.compareNoCase(RetailDamageFXTriggerNames[RETAIL_DAMAGE_FX_TREBUCHET_ROCK]) == 0)
		return RETAIL_DAMAGE_FX_TREBUCHET_ROCK;
	return RETAIL_DAMAGE_FX_NONE;
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

ProjectileUpdateInterface* projectileInterface(const Object* obj)
{
	if (!obj)
		return nullptr;
	for (BehaviorModule** module = obj->getBehaviorModules(); *module; ++module)
	{
		if (ProjectileUpdateInterface* projectile = (*module)->getProjectileUpdateInterface())
			return projectile;
	}
	return nullptr;
}

ObjectID launcherOf(const Object* source)
{
	const ProjectileUpdateInterface* projectile = projectileInterface(source);
	if (source && !source->isKindOf(KINDOF_PROJECTILE))
	{
		const auto* flight = dynamic_cast<const DumbProjectileBehavior*>(projectile);
		if (!flight || !flight->isPassengerFlight())
			return INVALID_ID;
	}
	return projectile ? projectile->projectileGetLauncherID() : INVALID_ID;
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

void parseForceKillObjectFilter(INI* ini, void* instance, void*, const void*)
{
	auto& nugget = *static_cast<WeaponNugget*>(instance);
	SpecialPowerObjectFilter::parse(ini, nullptr, &nugget.m_forceKillObjectFilter, nullptr);
	nugget.m_hasForceKillObjectFilter = TRUE;
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
	{ "ForceKillObjectFilter", parseForceKillObjectFilter, nullptr, 0 },
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

const FieldParse s_spawnAndFadeFields[] =
{
	{ "ObjectTargetFilter", parseSpecialObjectFilter, nullptr, 0 },
	{ "SpawnedObjectName", INI::parseAsciiString, nullptr, offsetof(WeaponNugget, m_spawnedObjectName) },
	{ "SpawnOffset", INI::parseCoord3D, nullptr, offsetof(WeaponNugget, m_spawnOffset) },
	{ nullptr, nullptr, nullptr, 0 }
};

const FieldParse s_grabFields[] =
{
	{ "ContainTargetOnEffect", INI::parseBool, nullptr, offsetof(WeaponNugget, m_containTargetOnEffect) },
	{ "ImpactTargetOnEffect", INI::parseBool, nullptr, offsetof(WeaponNugget, m_impactTargetOnEffect) },
	{ "RemoveTargetFromOtherContain", INI::parseBool, nullptr, offsetof(WeaponNugget, m_removeTargetFromOtherContain) },
	{ "ShockWaveAmount", INI::parseReal, nullptr, offsetof(WeaponNugget, m_shockWaveAmount) },
	{ "ShockWaveRadius", INI::parseReal, nullptr, offsetof(WeaponNugget, m_shockWaveRadius) },
	{ "ShockWaveTaperOff", INI::parseReal, nullptr, offsetof(WeaponNugget, m_shockWaveTaperOff) },
	{ "ShockWaveZMult", INI::parseReal, nullptr, offsetof(WeaponNugget, m_shockWaveZMult) },
	{ nullptr, nullptr, nullptr, 0 }
};

const FieldParse s_damageFieldFields[] =
{
	{ "WeaponTemplateName", INI::parseAsciiString, nullptr, offsetof(WeaponNugget, m_fieldWeaponName) },
	{ "Duration", INI::parseDurationUnsignedInt, nullptr, offsetof(WeaponNugget, m_fieldDuration) },
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

const FieldParse s_hordeAttackFields[] =
{
	{ "LockWeaponSlot", INI::parseLookupList, TheWeaponSlotTypeNamesLookupList, offsetof(WeaponNugget, m_lockWeaponSlot) },
	{ nullptr, nullptr, nullptr, 0 }
};

const char* s_fireActions[] =
{
	"INCREASE_BURN_RATE",
	"DECREASE_BURN_RATE",
	"INCREASE_FUEL",
	"INCREASE_BURN_RATE_ON_EXISTING_FIRE",
	"INCREASE_FUEL_ON_EXISTING_FIRE",
	nullptr
};

const FieldParse s_fireFields[] =
{
	{ "LogicType", INI::parseIndexList, s_fireActions, offsetof(WeaponNugget, m_fireEdit.action) },
	{ "MinMaxBurnRate", INI::parseReal, nullptr, offsetof(WeaponNugget, m_fireEdit.minMaxBurnRate) },
	{ "MinDecay", INI::parseReal, nullptr, offsetof(WeaponNugget, m_fireEdit.minDecay) },
	{ "MaxResistance", INI::parseReal, nullptr, offsetof(WeaponNugget, m_fireEdit.maxResistance) },
	{ nullptr, nullptr, nullptr, 0 }
};

const FieldParse s_emotionFields[] =
{
	{ "EmotionType", EmotionTrackerUpdate::parseEmotionType, nullptr, offsetof(WeaponNugget, m_emotionType) },
	{ "Radius", INI::parseReal, nullptr, offsetof(WeaponNugget, m_radius) },
	{ "Duration", INI::parseUnsignedInt, nullptr, offsetof(WeaponNugget, m_emotionSeconds) },
	{ nullptr, nullptr, nullptr, 0 }
};

const FieldParse s_luaEventFields[] =
{
	{ "LuaEvent", INI::parseAsciiString, nullptr, offsetof(WeaponNugget, m_luaEvent) },
	{ "Radius", INI::parseReal, nullptr, offsetof(WeaponNugget, m_radius) },
	{ "SendToEnemies", INI::parseBool, nullptr, offsetof(WeaponNugget, m_sendToEnemies) },
	{ "SendToAllies", INI::parseBool, nullptr, offsetof(WeaponNugget, m_sendToAllies) },
	{ "SendToNeutral", INI::parseBool, nullptr, offsetof(WeaponNugget, m_sendToNeutral) },
	{ nullptr, nullptr, nullptr, 0 }
};

const FieldParse s_openGateFields[] =
{
	{ "Radius", INI::parseReal, nullptr, offsetof(WeaponNugget, m_radius) },
	{ nullptr, nullptr, nullptr, 0 }
};

const FieldParse s_paralyzeFields[] =
{
	{ "Radius", INI::parseReal, nullptr, offsetof(WeaponNugget, m_radius) },
	{ "Duration", INI::parseDurationUnsignedInt, nullptr, offsetof(WeaponNugget, m_effectDuration) },
	{ "DamageArc", INI::parseAngleReal, nullptr, offsetof(WeaponNugget, m_paralyzeArc) },
	{ "ParalyzeFX", INI::parseFXList, nullptr, offsetof(WeaponNugget, m_paralyzeFX) },
	{ "FreezeAnimation", INI::parseBool, nullptr, offsetof(WeaponNugget, m_freezeAnimation) },
	{ "AffectHordeMembers", INI::parseBool, nullptr, offsetof(WeaponNugget, m_affectHordeMembers) },
	{ nullptr, nullptr, nullptr, 0 }
};

const FieldParse s_modelConditionFields[] =
{
	{ "ModelConditionNames", ModelConditionFlags::parseFromINI, nullptr, offsetof(WeaponNugget, m_effectConditions) },
	{ "ModelConditionDuration", INI::parseDurationUnsignedInt, nullptr, offsetof(WeaponNugget, m_effectDuration) },
	{ nullptr, nullptr, nullptr, 0 }
};

const FieldParse s_stealMoneyFields[] =
{
	{ "AmountStolenPerAttack", INI::parseReal, nullptr, offsetof(WeaponNugget, m_amountStolen) },
	{ nullptr, nullptr, nullptr, 0 }
};

void parseNugget(INI* ini, void* store, WeaponNuggetType type, const FieldParse* fields, const FieldParse* extraFields = nullptr)
{
	WeaponNugget nugget(type);
	MultiIniFieldParse parse;
	parse.add(s_commonFields);
	if (fields)
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
		case WEAPON_NUGGET_SLAVE_ATTACK: return "SlaveAttackNugget";
		case WEAPON_NUGGET_HORDE_ATTACK: return "HordeAttackNugget";
		case WEAPON_NUGGET_DAMAGE_FIELD: return "DamageFieldNugget";
		case WEAPON_NUGGET_GRAB: return "GrabNugget";
		case WEAPON_NUGGET_FIRE_LOGIC: return "FireLogicNugget";
		case WEAPON_NUGGET_SPAWN_AND_FADE: return "SpawnAndFadeNugget";
		case WEAPON_NUGGET_EMOTION_WEAPON: return "EmotionWeaponNugget";
		case WEAPON_NUGGET_LUA_EVENT: return "LuaEventNugget";
		case WEAPON_NUGGET_OPEN_GATE: return "OpenGateNugget";
		case WEAPON_NUGGET_PARALYZE: return "ParalyzeNugget";
		case WEAPON_NUGGET_SPECIAL_MODEL_CONDITION: return "SpecialModelConditionNugget";
		case WEAPON_NUGGET_STEAL_MONEY: return "StealMoneyNugget";
	}
	return "Nugget";
}

Real nuggetDamage(const WeaponNugget& nugget, Real scalar, const WeaponBonus& bonus)
{
	const Real base = AttributeModifierRules::baseDamageWithAdd(nugget.m_damage * scalar, nugget.m_acceptDamageAdd,
		bonus.getNuggetDamageAdd());
	return base * bonus.getNuggetDamageMultiplier(nugget.m_damageType) * bonus.getField(WeaponBonus::DAMAGE);
}

}

Bool objectFilterMatches(const SpecialPowerObjectFilter& filter, const Object* source, const Object* target)
{
	return filter.matchesObject(source, target);
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

void WeaponNugget::parseSlaveAttackNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_SLAVE_ATTACK, nullptr);
}

void WeaponNugget::parseHordeAttackNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_HORDE_ATTACK, s_hordeAttackFields);
}

void WeaponNugget::parseDamageFieldNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_DAMAGE_FIELD, s_damageFieldFields);
}

void WeaponNugget::parseGrabNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_GRAB, s_grabFields);
}

void WeaponNugget::parseFireLogicNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_FIRE_LOGIC, s_damageFields, s_fireFields);
}

void WeaponNugget::parseSpawnAndFadeNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_SPAWN_AND_FADE, s_spawnAndFadeFields);
}

void WeaponNugget::parseEmotionWeaponNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_EMOTION_WEAPON, s_emotionFields);
}

void WeaponNugget::parseLuaEventNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_LUA_EVENT, s_luaEventFields);
}

void WeaponNugget::parseOpenGateNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_OPEN_GATE, s_openGateFields);
}

void WeaponNugget::parseParalyzeNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_PARALYZE, s_paralyzeFields);
}

void WeaponNugget::parseSpecialModelConditionNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_SPECIAL_MODEL_CONDITION, s_modelConditionFields);
}

void WeaponNugget::parseStealMoneyNugget(INI* ini, void*, void* store, const void*)
{
	parseNugget(ini, store, WEAPON_NUGGET_STEAL_MONEY, s_stealMoneyFields);
}

Bool WeaponNugget::isUtilityNugget() const
{
	switch (m_type)
	{
		case WEAPON_NUGGET_EMOTION_WEAPON:
		case WEAPON_NUGGET_LUA_EVENT:
		case WEAPON_NUGGET_OPEN_GATE:
		case WEAPON_NUGGET_PARALYZE:
		case WEAPON_NUGGET_SPECIAL_MODEL_CONDITION:
		case WEAPON_NUGGET_STEAL_MONEY:
			return TRUE;
		default:
			return FALSE;
	}
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
	if (!m_fieldWeaponName.isEmpty())
		m_fieldWeapon = TheWeaponStore->findWeaponTemplate(m_fieldWeaponName);
	if (!m_oclName.isEmpty())
		m_ocl = TheObjectCreationListStore->findObjectCreationList(m_oclName.str());
	if (!m_spawnedObjectName.isEmpty())
		m_spawnedObject = TheThingFactory->findTemplate(m_spawnedObjectName, FALSE);
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
	return !m_hasSpecialObjectFilter || m_specialObjectFilter.matchesObject(attacker, victim);
}

Real WeaponNugget::scalarFor(const Object* attacker, const Object* victim) const
{
	std::vector<WeaponNuggetRules::Scalar> scalars;
	scalars.reserve(m_damageScalars.size());
	for (const WeaponNuggetScalar& scalar : m_damageScalars)
		scalars.push_back({scalar.m_scalar, scalar.m_filter.matchesObject(attacker, victim)});
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

Bool WeaponTemplate::canGrabTarget(const Object* source, const Object* target) const
{
	const ContainModuleInterface* contain = source->getContain();
	return contain && std::any_of(m_nuggets.begin(), m_nuggets.end(), [](const WeaponNugget& nugget) {
		return nugget.m_type == WEAPON_NUGGET_GRAB;
	}) && contain->isValidContainerFor(target, TRUE);
}

Bool WeaponTemplate::nuggetsCanDamage(Int depth) const
{
	for (const WeaponNugget& nugget : m_nuggets)
	{
		if (nugget.m_type == WEAPON_NUGGET_SLAVE_ATTACK || nugget.m_type == WEAPON_NUGGET_HORDE_ATTACK ||
			(nugget.dealsDamage() && nugget.m_damage > 0.0f))
			return TRUE;
		if (nugget.m_type == WEAPON_NUGGET_PROJECTILE && nugget.m_warheadTemplate && depth < MAX_WARHEAD_DEPTH &&
			nugget.m_warheadTemplate->nuggetsCanDamage(depth + 1))
			return TRUE;
		if (nugget.m_type == WEAPON_NUGGET_DAMAGE_FIELD && nugget.m_fieldWeapon && depth < MAX_WARHEAD_DEPTH &&
			nugget.m_fieldWeapon->nuggetsCanDamage(depth + 1))
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
		if ((nugget.m_type == WEAPON_NUGGET_SLAVE_ATTACK || nugget.m_type == WEAPON_NUGGET_HORDE_ATTACK) &&
			nugget.affects(sourceObj, victimObj))
		{
			total += 1.0f;
			continue;
		}
		if (nugget.m_type == WEAPON_NUGGET_PROJECTILE)
		{
			if (nugget.m_warheadTemplate && depth < MAX_WARHEAD_DEPTH)
				total += nugget.m_warheadTemplate->estimateNuggetDamage(sourceObj, victimObj, bonus, depth + 1);
			continue;
		}
		if (nugget.m_type == WEAPON_NUGGET_DAMAGE_FIELD)
		{
			if (nugget.m_fieldWeapon && depth < MAX_WARHEAD_DEPTH && nugget.affects(sourceObj, victimObj))
				total += nugget.m_fieldWeapon->estimateNuggetDamage(sourceObj, victimObj, bonus, depth + 1);
			continue;
		}
		if (!nugget.dealsDamage() || nugget.m_damage <= 0.0f)
			continue;
		if (!victimObj)
		{
			total += nuggetDamage(nugget, 1.0f, bonus);
			continue;
		}
		if (!nugget.affects(sourceObj, victimObj))
			continue;
		DamageInfoInput damageInfo;
		damageInfo.m_damageType = nugget.m_damageType;
		damageInfo.m_deathType = nugget.m_deathType;
		damageInfo.m_sourceID = sourceObj->getID();
		damageInfo.m_amount = nuggetDamage(nugget, nugget.scalarFor(sourceObj, victimObj), bonus);
		total += victimObj->estimateDamage(damageInfo);
	}
	return total;
}

void WeaponTemplate::orderNuggetSlaves(const Object* source, Object* victim, const Coord3D* position) const
{
	if (std::none_of(m_nuggets.begin(), m_nuggets.end(), [&](const WeaponNugget& nugget)
		{
			return nugget.m_type == WEAPON_NUGGET_SLAVE_ATTACK && nugget.isEnabledFor(source) && nugget.affects(source, victim);
		}))
		return;

	if (auto* spawn = source->getSpawnBehaviorInterface())
	{
		if (victim)
			spawn->orderSlavesToAttackTarget(victim, 1, CMD_FROM_AI);
		else
			spawn->orderSlavesToAttackPosition(position, 1, CMD_FROM_AI);
	}

	static const NameKeyType creation = NAMEKEY("RetailObjectCreationUpgrade");
	for (BehaviorModule** module = source->getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() != creation)
			continue;
		Object* child = static_cast<const RetailObjectCreationUpgrade*>(*module)->getSlavedSpawn();
		AIUpdateInterface* controller = child ? child->getAI() : nullptr;
		if (!controller || child == victim)
			continue;
		if (victim)
		{
			if (!controller->isAttacking() || controller->getGoalObject() != victim)
				controller->aiForceAttackObject(victim, 1, CMD_FROM_AI);
		}
		else
		{
			const Coord3D* goal = controller->getGoalPosition();
			if (!controller->isAttacking() || controller->getGoalObject() || !goal ||
				goal->x != position->x || goal->y != position->y || goal->z != position->z)
				controller->aiAttackPosition(position, 1, CMD_FROM_AI);
		}
	}
}

void WeaponTemplate::orderNuggetHorde(const Object* source, Object* victim, const Coord3D* position) const
{
	const HordeContain* horde = HordeContain::get(source);
	if (!horde || (!victim && !position))
		return;
	for (const WeaponNugget& nugget : m_nuggets)
	{
		if (nugget.m_type != WEAPON_NUGGET_HORDE_ATTACK || !nugget.isEnabledFor(source) || !nugget.affects(source, victim))
			continue;
		for (Object* member : *horde->getContainedItemsList())
		{
			const Weapon* weapon = member->getWeaponInWeaponSlot(nugget.m_lockWeaponSlot);
			AIUpdateInterface* ai = member->getAI();
			if (!ai || !weapon || weapon->getTemplate() == this || member == victim || member->isEffectivelyDead())
				continue;
			ai->aiAttackWithRetailHordeWeapon(victim, position, nugget.m_lockWeaponSlot);
		}
	}
}

Object* WeaponTemplate::findContainedProjectile(const Object* source) const
{
	const ContainModuleInterface* contain = source->getContain();
	const ContainedItemsList* passengers = contain ? contain->getContainedItemsList() : nullptr;
	if (!passengers)
		return nullptr;
	for (Object* passenger : *passengers)
	{
		if (!passenger->isEffectivelyDead() && m_projectileFilterInContainer.matchesObject(source, passenger) &&
			projectileInterface(passenger))
			return passenger;
	}
	return nullptr;
}

Bool WeaponTemplate::hasRequiredProjectile(const Object* source) const
{
	if (m_projectileSelf || !m_hasProjectileFilterInContainer)
		return TRUE;
	for (const WeaponNugget& nugget : m_nuggets)
		if (nugget.m_type == WEAPON_NUGGET_PROJECTILE && !nugget.m_projectileTemplate && nugget.isEnabledFor(source))
			return findContainedProjectile(source) != nullptr;
	return TRUE;
}

Real WeaponTemplate::scatterProjectile(Coord3D& destination, Real radius, const Object* target)
{
	const Real distance = GameLogicRandomValueReal(0, radius);
	const Real angle = GameLogicRandomValueReal(0, 2 * PI);
	destination.x += distance * Cos(angle);
	destination.y += distance * Sin(angle);
	destination.z = TheTerrainLogic->getLayerHeight(destination.x, destination.y, target ? target->getLayer() : LAYER_GROUND);
	return distance;
}

void WeaponTemplate::launchNuggetProjectiles(const Object* sourceObj, WeaponSlotType wslot, Int specificBarrelToUse,
	Object* victimObj, const Coord3D& destination, Weapon* firingWeapon, ObjectID* projectileID,
	Real independentScatterRadius, const Object* scatterTarget) const
{
	Player* owner = sourceObj->getControllingPlayer();
	if (!owner)
		return;
	const VeterancyLevel veterancy = sourceObj->getVeterancyLevel();
	for (size_t index = 0; index < m_nuggets.size(); ++index)
	{
		const WeaponNugget& nugget = m_nuggets[index];
		if (nugget.m_type != WEAPON_NUGGET_PROJECTILE || !nugget.m_warheadTemplate)
			continue;
		if (!nugget.isEnabledFor(sourceObj) || (victimObj && !nugget.affects(sourceObj, victimObj)))
			continue;

		Object* projectile;
		if (m_projectileSelf)
		{
			auto* self = dynamic_cast<DumbProjectileBehavior*>(projectileInterface(sourceObj));
			if (!self || self->isSelfFlight() || sourceObj->isEffectivelyDead())
				continue;
			projectile = const_cast<Object*>(sourceObj);
		}
		else if (nugget.m_projectileTemplate)
		{
			projectile = TheThingFactory->newObject(nugget.m_projectileTemplate, owner->getDefaultTeam());
			projectile->setProducer(sourceObj);
		}
		else
		{
			projectile = m_hasProjectileFilterInContainer ? findContainedProjectile(sourceObj) : nullptr;
			if (!projectile)
				continue;
			sourceObj->getContain()->removeFromContain(projectile);
		}
		Coord3D aim = destination;
		if (independentScatterRadius > 0.0f)
			scatterProjectile(aim, independentScatterRadius, scatterTarget);
		if (projectileID)
			*projectileID = projectile->getID();
		if (!m_projectileSelf)
			firingWeapon->newProjectileFired(sourceObj, projectile, victimObj, &aim);

		if (ProjectileUpdateInterface* pui = projectileInterface(projectile))
		{
			pui->projectileSetLaunchWeapon(this);
			pui->projectileLaunchAtObjectOrPosition(victimObj, &aim, sourceObj, wslot, specificBarrelToUse,
				nugget.m_warheadTemplate, m_projectileExhausts[veterancy]);
		}
		else
			projectile->setPosition(&aim);

		if (RetailTestCommands::tracesCombat())
			std::fprintf(stderr, "TEST projectile frame=%u weapon=%s nugget=%u projectile=%s id=%u warhead=%s source=%u victim=%u dest=%.1f,%.1f\n",
				TheGameLogic->getFrame(), m_name.str(), static_cast<unsigned>(index), projectile->getTemplate()->getName().str(),
				static_cast<unsigned>(projectile->getID()), nugget.m_warheadTemplate->getName().str(),
				static_cast<unsigned>(sourceObj->getID()), static_cast<unsigned>(victimObj ? victimObj->getID() : INVALID_ID),
				aim.x, aim.y);
	}
}

void WeaponTemplate::applyModifierNugget(const WeaponNugget& nugget, const Object* attacker, ObjectID victimID, const Coord3D* pos) const
{
	const Int list = AttributeModifiers::findList(nugget.m_attributeModifier);
	if (list < 0 || !pos)
		return;
	UnsignedInt antiMask = 0;
	for (const AsciiString& name : nugget.m_antiCategories)
		antiMask |= AttributeModifierRules::categoryBit(AttributeModifierRules::indexOf(AttributeModifierRules::categoryNames(), name.str()));
	Object* primary = victimID != INVALID_ID ? TheGameLogic->findObjectByID(victimID) : nullptr;
	const auto affect = [&](Object* victim)
	{
		if (victim->isEffectivelyDead() || !nugget.affects(attacker, victim))
			return;
		AttributeModifiers::apply(victim, list, AttributeModifierHelper::USE_LIST_DURATION, antiMask);
		if (RetailTestCommands::tracesCombat())
			std::fprintf(stderr, "TEST modifier nugget frame=%u weapon=%s list=%s victim=%u(%s)\n", TheGameLogic->getFrame(), m_name.str(),
				nugget.m_attributeModifier.str(), static_cast<unsigned>(victim->getID()), victim->getTemplate()->getName().str());
	};
	if (nugget.m_radius <= 0.0f)
	{
		if (primary)
			affect(primary);
		return;
	}
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(pos, nugget.m_radius, FROM_BOUNDINGSPHERE_3D, nullptr, ITER_SORTED_BY_ID);
	MemoryPoolObjectHolder hold(iter);
	for (Object* victim = iter->first(); victim; victim = iter->next())
	{
		if (victim->isKindOf(KINDOF_PROJECTILE))
			continue;
		if (judgeRadiusDamageVictim(attacker, victim, primary, m_affectsMask) == RADIUS_DAMAGE_SKIP)
			continue;
		affect(victim);
	}
}

void WeaponTemplate::applyUtilityNugget(const WeaponNugget& nugget, const Object* source, Object* primary,
	const Coord3D* pos) const
{
	if (!source || !pos)
		return;

	const auto kind = nugget.m_type;
	const UnsignedInt now = TheGameLogic->getFrame();
	const Coord3D origin = *source->getPosition();
	const Coord3D facing = *source->getUnitDirectionVector2D();
	std::set<ObjectID> paralyzed;
	const auto paralyze = [&](const auto& visit, Object* victim) -> void
	{
		if (!victim || victim->isEffectivelyDead() || victim->isDestroyed() ||
			victim->getLogicModelConditions().test(MODELCONDITION_INVULNERABLE) ||
			!paralyzed.insert(victim->getID()).second)
			return;

		if (nugget.m_affectHordeMembers)
			if (const auto* horde = HordeContain::get(victim))
				if (const auto* members = horde->getContainedItemsList())
					for (Object* member : *members)
						visit(visit, member);

		const Coord3D& target = *victim->getPosition();
		if (nugget.m_paralyzeArc < PI &&
			!WeaponNuggetRules::withinArc(facing.x, facing.y, target.x - origin.x, target.y - origin.y,
				WWMath::CosTrig(nugget.m_paralyzeArc), FALSE, FALSE))
			return;

		const DisabledType disabled = nugget.m_freezeAnimation ? DISABLED_USER_FROZEN : DISABLED_USER_PARALYZED;
		if (nugget.m_effectDuration)
			victim->setDisabledUntil(disabled, now + nugget.m_effectDuration);
		if (nugget.m_paralyzeFX)
			FXList::doFXObj(nugget.m_paralyzeFX, victim);
	};

	const auto affect = [&](Object* victim)
	{
		if (victim->isEffectivelyDead() || victim->isDestroyed() || !nugget.affects(source, victim))
			return;
		if (kind != WEAPON_NUGGET_LUA_EVENT &&
			judgeRadiusDamageVictim(source, victim, primary, m_affectsMask) != RADIUS_DAMAGE_AFFECT)
			return;

		switch (kind)
		{
			case WEAPON_NUGGET_EMOTION_WEAPON:
				if (nugget.m_emotionType >= 0 && nugget.m_emotionSeconds)
					EmotionTrackerUpdate::signalWeaponEmotion(victim, static_cast<EmotionType>(nugget.m_emotionType),
						source, nugget.m_emotionSeconds * LOGICFRAMES_PER_SECOND);
				break;
			case WEAPON_NUGGET_LUA_EVENT:
			{
				const Relationship relation = victim->getRelationship(source);
				if ((relation == ENEMIES && nugget.m_sendToEnemies) ||
					(relation == ALLIES && nugget.m_sendToAllies) ||
					(relation == NEUTRAL && nugget.m_sendToNeutral))
					RetailBuildingEvents::weaponEvent(*victim, nugget.m_luaEvent.str(), source);
				break;
			}
			case WEAPON_NUGGET_OPEN_GATE:
				if (auto* gate = GateOpenAndCloseBehavior::resolveForControl(victim))
					gate->request(TRUE);
				break;
			case WEAPON_NUGGET_PARALYZE:
				paralyze(paralyze, victim);
				break;
			case WEAPON_NUGGET_SPECIAL_MODEL_CONDITION:
				for (Int condition = MODELCONDITION_FIRST; condition < MODELCONDITION_COUNT; ++condition)
					if (nugget.m_effectConditions.test(condition))
						victim->setTimedModelCondition(static_cast<ModelConditionFlagType>(condition), nugget.m_effectDuration);
				break;
			case WEAPON_NUGGET_STEAL_MONEY:
			{
				auto* from = victim->getControllingPlayer();
				auto* to = source->getControllingPlayer();
				if (!from || !to || from == to || !(nugget.m_amountStolen > 0.0f))
					break;
				const UnsignedInt balance = from->getMoney()->countMoney();
				const UnsignedInt amount = nugget.m_amountStolen >= static_cast<double>(balance) ?
					balance : static_cast<UnsignedInt>(nugget.m_amountStolen);
				to->getMoney()->deposit(from->getMoney()->withdraw(amount));
				break;
			}
			default:
				break;
		}
	};

	if (primary && kind != WEAPON_NUGGET_LUA_EVENT)
		affect(primary);
	if (kind == WEAPON_NUGGET_STEAL_MONEY || kind == WEAPON_NUGGET_SPECIAL_MODEL_CONDITION)
		return;
	if (primary && nugget.m_radius <= 0.0f)
	{
		if (kind == WEAPON_NUGGET_LUA_EVENT)
			affect(primary);
		return;
	}
	const Real radius = kind == WEAPON_NUGGET_LUA_EVENT ? nugget.m_radius : std::max(1.0f, nugget.m_radius);
	const DistanceCalculationType distance = kind == WEAPON_NUGGET_LUA_EVENT ? FROM_CENTER_3D : FROM_BOUNDINGSPHERE_3D;
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(pos, radius, distance, nullptr, ITER_SORTED_BY_ID);
	MemoryPoolObjectHolder holder(iter);
	for (Object* victim = iter->first(); victim; victim = iter->next())
		if (victim != primary || kind == WEAPON_NUGGET_LUA_EVENT)
			affect(victim);
}

const Object* WeaponTemplate::getNuggetAttacker(const Object* source)
{
	const ObjectID launcherID = launcherOf(source);
	const Object* launcher = launcherID != INVALID_ID ? TheGameLogic->findObjectByID(launcherID) : nullptr;
	return launcher ? launcher : source;
}

void WeaponTemplate::dealNuggetsAtImpact(ObjectID sourceID, ObjectID victimID, const Coord3D* pos, const WeaponBonus& bonus, Bool contactHit) const
{
	const Object* impactSource = TheGameLogic->findObjectByID(sourceID);
	const Object* attacker = getNuggetAttacker(impactSource);
	const ProjectileUpdateInterface* projectile = projectileInterface(impactSource);
	const WeaponTemplate* launchWeapon = projectile ? projectile->projectileGetLaunchWeapon() : nullptr;
	const RetailDamageFXTrigger launchTrigger = launchWeapon ? damageFXTrigger(launchWeapon->m_fxTrigger) : RETAIL_DAMAGE_FX_NONE;
	const PhysicsBehavior* physics = impactSource ? impactSource->getPhysics() : nullptr;
	Coord3D velocity;
	if (!projectile || !projectile->projectileGetImpactVelocity(velocity))
		velocity = physics ? *physics->getVelocity() : Coord3D{};
	const ObjectID nuggetSourceID = attacker ? attacker->getID() : sourceID;
	if (RetailTestCommands::tracesCombat())
		std::fprintf(stderr, "TEST impact frame=%u weapon=%s source=%u victim=%u pos=%.1f,%.1f bonus=%.2f,%.2f\n", TheGameLogic->getFrame(),
			m_name.str(), static_cast<unsigned>(nuggetSourceID), static_cast<unsigned>(victimID), pos->x, pos->y,
			bonus.getField(WeaponBonus::DAMAGE), bonus.getField(WeaponBonus::RADIUS));

	for (size_t index = 0; index < m_nuggets.size(); ++index)
	{
		const WeaponNugget& nugget = m_nuggets[index];
		if (nugget.m_type == WEAPON_NUGGET_ATTRIBUTE_MODIFIER)
		{
			if (nugget.isEnabledFor(attacker))
				applyModifierNugget(nugget, attacker, victimID, pos);
			continue;
		}
		if (!nugget.isImpactNugget() || !nugget.isEnabledFor(attacker))
			continue;
		if (nugget.m_delayFrames == 0)
			dealNuggetDamage(static_cast<Int>(index), nuggetSourceID, victimID, pos, bonus, FALSE, contactHit, &velocity, launchTrigger, launchWeapon);
		else if (RetailFileLayers::enabled() && TheWeaponStore && nugget.m_type == WEAPON_NUGGET_META_IMPACT)
		{
			const RetailDamageFXTrigger warheadTrigger = damageFXTrigger(m_fxTrigger);
			applyMetaImpactNugget(static_cast<Int>(index), attacker, TheGameLogic->findObjectByID(victimID), *pos,
				bonus, TheGameLogic->getFrame() + nugget.m_delayFrames,
				warheadTrigger != RETAIL_DAMAGE_FX_NONE ? warheadTrigger : launchTrigger);
		}
		else if (TheWeaponStore)
			TheWeaponStore->setDelayedDamage(this, pos, TheGameLogic->getFrame() + nugget.m_delayFrames, nuggetSourceID, victimID,
				bonus, static_cast<Int>(index), FALSE, contactHit, launchTrigger, &velocity, launchWeapon);
	}
}

void WeaponTemplate::applyMetaImpactNugget(Int nuggetIndex, const Object* source, Object* primaryVictim,
	const Coord3D& pos, const WeaponBonus& bonus, UnsignedInt impactFrame, RetailDamageFXTrigger trigger) const
{
	const WeaponNugget& nugget = m_nuggets[nuggetIndex];
	const Real radius = nugget.m_shockWaveRadius * bonus.getField(WeaponBonus::RADIUS);
	const Coord3D& origin = source ? *source->getPosition() : pos;
	Coord3D axis{pos.x - origin.x, pos.y - origin.y, 0.0f};
	if (axis.x == 0.0f && axis.y == 0.0f)
		axis = source ? *source->getUnitDirectionVector2D() : Coord3D{1.0f, 0.0f, 0.0f};
	const Bool fullArc = nugget.m_shockWaveArc <= 0.0f || nugget.m_shockWaveArc >= 360.0f;
	const Real cosHalfArc = fullArc ? -1.0f : WWMath::CosTrig(nugget.m_shockWaveArc * (PI / 360.0f));
	SimpleObjectIterator* iter = radius > 0.0f ? ThePartitionManager->iterateObjectsInRange(&pos, radius, FROM_CENTER_2D, nullptr,
		ITER_SORTED_BY_ID) : nullptr;
	MemoryPoolObjectHolder hold(iter);
	const auto affect = [&](Object* victim)
	{
		auto* physics = victim->getPhysics();
		if (!physics || HordeContain::get(victim) || victim->isKindOf(KINDOF_PROJECTILE) ||
			(victim->isEffectivelyDead() && (!nugget.m_onlyWhenJustDied || victim != primaryVictim)) ||
			(nugget.m_onlyWhenJustDied && !victim->isEffectivelyDead()) ||
			judgeRadiusDamageVictim(source, victim, primaryVictim, m_affectsMask) != RADIUS_DAMAGE_AFFECT ||
			!nugget.affects(source, victim))
			return;

		const Coord3D& targetPos = *victim->getPosition();
		const Real dx = targetPos.x - pos.x;
		const Real dy = targetPos.y - pos.y;
		if (victim != primaryVictim && !WeaponNuggetRules::withinArc(axis.x, axis.y, dx, dy, cosHalfArc, fullArc, nugget.m_shockWaveArcInverted))
			return;
		const Real falloff = radius > 0.0f ?
			std::clamp(1.0f - nugget.m_shockWaveTaperOff * WWMath::SqrtfOrigin(dx * dx + dy * dy) / radius, 0.0f, 1.0f) : 1.0f;
		const Real amount = nugget.m_shockWaveAmount * falloff;
		if (!(amount > victim->getTemplate()->getRotWKObjectData().shockwaveResistance))
			return;

		Coord3D direction{targetPos.x - origin.x, targetPos.y - origin.y, 0.0f};
		const Real distance = WWMath::SqrtfOrigin(direction.x * direction.x + direction.y * direction.y);
		if (distance > 0.0f)
		{
			direction.x /= distance;
			direction.y /= distance;
		}
		else
			direction = source ? *source->getUnitDirectionVector2D() : Coord3D{1.0f, 0.0f, 0.0f};
		if (nugget.m_invertShockWave != nugget.m_flipDirection)
		{
			direction.x = -direction.x;
			direction.y = -direction.y;
		}
		UnsignedInt arrivalFrame = impactFrame;
		const Real height = targetPos.z - origin.z;
		if (TheWeaponStore && nugget.m_shockWaveSpeed > 0.0f && (distance > 0.0f || height != 0.0f))
		{
			const Real pathLength = WWMath::SqrtfOrigin(distance * distance + height * height);
			const UnsignedInt tick = RotwkDuration::engineFrames(1, LOGICFRAMES_PER_SECOND);
			const Real travelTicks = WWMath::Ceil(pathLength / (nugget.m_shockWaveSpeed * tick));
			if (travelTicks > 0.0f)
			{
				const UnsignedInt baseFrame = impactFrame ? impactFrame : TheGameLogic->getFrame();
				if (baseFrame >= FOREVER || travelTicks >= static_cast<Real>((FOREVER - baseFrame) / tick))
					return;
				arrivalFrame = baseFrame + static_cast<UnsignedInt>(travelTicks) * tick;
			}
		}
		if (arrivalFrame)
			TheWeaponStore->setDelayedMetaImpact(this, nuggetIndex, arrivalFrame, victim->getID(), direction, amount, trigger);
		else
			physics->launchFromWeaponImpact(direction, amount, nugget.m_shockWaveZMult, trigger);
	};
	if (iter)
	{
		for (Object* victim = iter->first(); victim; victim = iter->next())
			affect(victim);
	}
	else if (primaryVictim)
		affect(primaryVictim);
}

void WeaponTemplate::dealNuggetDamage(Int nuggetIndex, ObjectID sourceID, ObjectID victimID, const Coord3D* pos, const WeaponBonus& bonus,
	Bool dotTick, Bool contactHit, const Coord3D* impactVelocity, RetailDamageFXTrigger launchTrigger,
	const WeaponTemplate* launchWeapon) const
{
	const WeaponNugget& nugget = m_nuggets[nuggetIndex];
	Object* source = TheGameLogic->findObjectByID(sourceID);
	if (nugget.m_type == WEAPON_NUGGET_FIRE_LOGIC)
	{
		if (RetailFileLayers::enabled() && TheTerrainLogic && pos)
		{
			auto edit = nugget.m_fireEdit;
			edit.amount = nugget.m_damage;
			edit.radius = nugget.m_radius;
			edit.arc = nugget.m_damageArc * PI / 180.0f;
			Coord3D direction{};
			if (source)
				source->getUnitDirectionVector3D(direction);
			TheTerrainLogic->retailFire().apply(*TheTerrainLogic, *pos, source ? &direction : nullptr, edit);
		}
		return;
	}
	Object* primaryVictim = victimID ? TheGameLogic->findObjectByID(victimID) : nullptr;
	if (nugget.isUtilityNugget())
	{
		applyUtilityNugget(nugget, source, primaryVictim, primaryVictim ? primaryVictim->getPosition() : pos);
		return;
	}
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

	if (nugget.m_type == WEAPON_NUGGET_SPAWN_AND_FADE)
	{
		if (!nugget.m_spawnedObject || !source || source->isEffectivelyDead() ||
			(primaryVictim && !nugget.affects(source, primaryVictim)))
			return;
		Coord3D location;
		source->transformPoint(&nugget.m_spawnOffset, &location);
		if (Object* spawned = TheThingFactory->newObject(nugget.m_spawnedObject, source->getTeam()))
		{
			spawned->setProducer(source);
			spawned->setPosition(&location);
			spawned->setOrientation(source->getOrientation());
			source->kill(DAMAGE_UNRESISTABLE, DEATH_FADED);
		}
		return;
	}

	if (nugget.m_type == WEAPON_NUGGET_GRAB)
	{
		if (!RetailFileLayers::enabled() || !source || source->isEffectivelyDead() || !primaryVictim ||
			primaryVictim == source || primaryVictim->isDestroyed() || !nugget.affects(source, primaryVictim))
			return;

		Object* previousContainer = primaryVictim->getContainedBy();
		if (previousContainer == source)
			return;
		ContainModuleInterface* contain = source->getContain();
		if (nugget.m_containTargetOnEffect && !primaryVictim->isEffectivelyDead() && contain &&
			contain->isValidContainerFor(primaryVictim, TRUE) &&
			(!previousContainer || nugget.m_removeTargetFromOtherContain || HordeContain::get(previousContainer)))
		{
			if (previousContainer)
				previousContainer->getContain()->removeFromContain(primaryVictim);
			contain->addToContain(primaryVictim);
			if (primaryVictim->getContainedBy() == source)
			{
				GiantBirdAIUpdate::trackGrabbedPassenger(*source, *primaryVictim);
				return;
			}
		}

		auto* physics = primaryVictim->getPhysics();
		if (nugget.m_impactTargetOnEffect && nugget.m_shockWaveRadius > 0.0f && physics && !primaryVictim->getEnclosingContainedBy())
		{
			const Coord3D& origin = *source->getPosition();
			const Coord3D& target = *primaryVictim->getPosition();
			Coord3D direction{target.x - origin.x, target.y - origin.y, 0.0f};
			const Real distance = WWMath::SqrtfOrigin(direction.x * direction.x + direction.y * direction.y);
			const Real dz = target.z - origin.z;
			const Real separation = WWMath::SqrtfOrigin(distance * distance + dz * dz);
			const Real fraction = std::min(separation / nugget.m_shockWaveRadius, 1.0f);
			const Real amount = nugget.m_shockWaveAmount * (1.0f - (1.0f - nugget.m_shockWaveTaperOff) * fraction);
			if (distance > 0.0f)
			{
				direction.x /= distance;
				direction.y /= distance;
			}
			if (amount > primaryVictim->getTemplate()->getRotWKObjectData().shockwaveResistance)
				physics->launchFromImpact(direction, amount, nugget.m_shockWaveZMult);
		}
		return;
	}

	if (nugget.m_type == WEAPON_NUGGET_DAMAGE_FIELD)
	{
		Player* owner = source ? source->getControllingPlayer() : nullptr;
		if (!RetailFileLayers::enabled() || !owner || !nugget.m_fieldWeapon || !nugget.affects(source, primaryVictim))
			return;
		const ThingTemplate* fieldTemplate = TheThingFactory->findTemplate(TheGlobalData->m_genericDamageFieldName, FALSE);
		if (!fieldTemplate)
			return;

		Object* field = TheThingFactory->newObject(fieldTemplate, owner->getDefaultTeam());
		static const NameKeyType fireKey = NAMEKEY("FireWeaponUpdate");
		static const NameKeyType deletionKey = NAMEKEY("DeletionUpdate");
		auto* fire = static_cast<FireWeaponUpdate*>(field->findUpdateModule(fireKey));
		auto* deletion = static_cast<DeletionUpdate*>(field->findUpdateModule(deletionKey));
		if (!fire || !deletion)
		{
			TheGameLogic->destroyObject(field);
			return;
		}

		const Coord3D target{pos->x, pos->y, TheTerrainLogic->getGroundHeight(pos->x, pos->y)};
		field->setPosition(&target);
		field->setProducer(source);
		fire->setRetailDamageFieldWeapon(nugget.m_fieldWeapon);
		deletion->setLifetimeRange(nugget.m_fieldDuration, nugget.m_fieldDuration);
		if (RetailTestCommands::tracesCombat())
			std::fprintf(stderr, "TEST damage field frame=%u source=%u field=%u weapon=%s duration=%u target=%.1f,%.1f\n",
				TheGameLogic->getFrame(), static_cast<UnsignedInt>(sourceID), static_cast<UnsignedInt>(field->getID()),
				nugget.m_fieldWeapon->getName().str(), nugget.m_fieldDuration, target.x, target.y);
		return;
	}

	if (nugget.m_type == WEAPON_NUGGET_OCL)
	{
		if (nugget.m_ocl && source)
			ObjectCreationList::create(nugget.m_ocl, source, pos, pos, source->getOrientation());
		return;
	}

	const RetailDamageFXTrigger warheadTrigger = damageFXTrigger(m_fxTrigger);
	const RetailDamageFXTrigger trigger = warheadTrigger != RETAIL_DAMAGE_FX_NONE ? warheadTrigger : launchTrigger;
	if (nugget.m_type == WEAPON_NUGGET_META_IMPACT)
	{
		applyMetaImpactNugget(nuggetIndex, source, primaryVictim, *pos, bonus, 0, trigger);
		return;
	}

	const auto damage = [&](Object* victim, Bool killSelf)
	{
		const Bool forceKill = nugget.m_hasForceKillObjectFilter && !victim->isEffectivelyDead() &&
			nugget.m_forceKillObjectFilter.matchesObject(source, victim);
		const Real scalar = nugget.scalarFor(source, victim);
		const Real amount = killSelf ? HUGE_DAMAGE_AMOUNT : nuggetDamage(nugget, scalar, bonus);
		if (amount <= 0.0f && !forceKill)
			return;

		DamageInfo damageInfo;
		damageInfo.in.m_damageType = nugget.m_damageType;
		damageInfo.in.m_damageFXOverride = nugget.m_damageFXType;
		damageInfo.in.m_deathType = nugget.m_deathType;
		damageInfo.in.m_sourceID = sourceID;
		damageInfo.in.m_sourcePlayerMask = source && source->getControllingPlayer() ? source->getControllingPlayer()->getPlayerMask() : 0;
		damageInfo.in.m_amount = amount;
		damageInfo.in.m_kill = forceKill;
		damageInfo.retailFXTrigger = trigger;
		if (RetailFileLayers::enabled())
			damageInfo.retailWeaponName = launchWeapon ? launchWeapon->m_name : m_name;
		damageInfo.retailDamageSubType = nugget.m_damageSubType;
		damageInfo.shouldPlayUnderAttackEvaEvent = m_shouldPlayUnderAttackEvaEvent;
		if (impactVelocity)
			damageInfo.impactVelocity = *impactVelocity;
		else if (source && source->getPhysics())
			damageInfo.impactVelocity = *source->getPhysics()->getVelocity();
		const Real healthBefore = victim->getBodyModule() ? victim->getBodyModule()->getHealth() : 0.0f;
		HordeContain::applyWeaponDamage(victim, damageInfo, contactHit);
		if (nugget.m_drainLife && source && !source->isEffectivelyDead() && !source->isKindOf(KINDOF_PROJECTILE))
		{
			const Real healing = damageInfo.out.m_actualDamageDealt * nugget.m_drainLifeMultiplier;
			if (healing > 0.0f)
				source->attemptHealing(healing, nullptr, TRUE);
		}

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
		iter = ThePartitionManager->iterateObjectsInRange(pos, radius, FROM_BOUNDINGSPHERE_3D, nullptr,
			ITER_SORTED_BY_ID);
		curVictim = iter->firstWithNumeric(&curVictimDistSqr);
	}
	else if (!curVictim && source && (m_affectsMask & WEAPON_AFFECTS_SELF) &&
		source->getPosition()->x == pos->x && source->getPosition()->y == pos->y)
		curVictim = source;
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
				curVictim->getID(), bonus, nuggetIndex, TRUE, FALSE, launchTrigger, impactVelocity, launchWeapon);
	}
}
