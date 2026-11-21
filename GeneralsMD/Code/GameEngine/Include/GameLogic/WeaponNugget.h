#pragma once

#include <vector>

#include "Common/AsciiString.h"
#include "Common/SpecialPowerINI.h"
#include "GameLogic/Damage.h"
#include "GameLogic/WeaponSetType.h"

class INI;
class Object;
class ObjectCreationList;
class ThingTemplate;
class UpgradeTemplate;
class WeaponTemplate;

enum WeaponNuggetType CPP_11(: Int)
{
	WEAPON_NUGGET_DAMAGE,
	WEAPON_NUGGET_DOT,
	WEAPON_NUGGET_PROJECTILE,
	WEAPON_NUGGET_META_IMPACT,
	WEAPON_NUGGET_OCL,
	WEAPON_NUGGET_ATTRIBUTE_MODIFIER,
	WEAPON_NUGGET_SLAVE_ATTACK,
	WEAPON_NUGGET_HORDE_ATTACK,
};

struct WeaponNuggetScalar
{
	Real m_scalar = 1.0f;
	SpecialPowerObjectFilter m_filter;
};

struct WeaponNugget
{
	explicit WeaponNugget(WeaponNuggetType type = WEAPON_NUGGET_DAMAGE) : m_type(type) {}

	Bool isEnabledFor(const Object* attacker) const;
	Bool affects(const Object* attacker, const Object* victim) const;
	Real scalarFor(const Object* attacker, const Object* victim) const;
	Bool isImpactNugget() const { return dealsDamage() || m_type == WEAPON_NUGGET_OCL || m_type == WEAPON_NUGGET_META_IMPACT; }
	Bool dealsDamage() const { return m_type == WEAPON_NUGGET_DAMAGE || m_type == WEAPON_NUGGET_DOT; }
	void postProcessLoad();

	static void parseDamageNugget(INI* ini, void* instance, void* store, const void* userData);
	static void parseDOTNugget(INI* ini, void* instance, void* store, const void* userData);
	static void parseProjectileNugget(INI* ini, void* instance, void* store, const void* userData);
	static void parseMetaImpactNugget(INI* ini, void* instance, void* store, const void* userData);
	static void parseWeaponOCLNugget(INI* ini, void* instance, void* store, const void* userData);
	static void parseAttributeModifierNugget(INI* ini, void* instance, void* store, const void* userData);
	static void parseSlaveAttackNugget(INI* ini, void* instance, void* store, const void* userData);
	static void parseHordeAttackNugget(INI* ini, void* instance, void* store, const void* userData);

	WeaponNuggetType m_type;
	std::vector<AsciiString> m_requiredUpgradeNames;
	std::vector<AsciiString> m_forbiddenUpgradeNames;
	std::vector<const UpgradeTemplate*> m_requiredUpgrades;
	std::vector<const UpgradeTemplate*> m_forbiddenUpgrades;
	SpecialPowerObjectFilter m_specialObjectFilter;
	Bool m_hasSpecialObjectFilter = FALSE;

	Real m_damage = 0.0f;
	Real m_radius = 0.0f;
	UnsignedInt m_delayFrames = 0;
	DamageType m_damageType = DAMAGE_EXPLOSION;
	DamageType m_damageFXType = DAMAGE_EXPLOSION;
	DeathType m_deathType = DEATH_NORMAL;
	AsciiString m_damageSubType;
	Real m_damageArc = 0.0f;
	Real m_cosHalfDamageArc = -1.0f;
	Bool m_damageArcInverted = FALSE;
	std::vector<WeaponNuggetScalar> m_damageScalars;
	Real m_flankingBonus = 0.0f;
	Bool m_acceptDamageAdd = TRUE;
	Real m_damageSpeed = 0.0f;
	Real m_damageMaxHeight = 0.0f;
	Real m_damageMaxHeightAboveTerrain = 0.0f;
	Real m_damageTaperOff = 0.0f;
	Real m_minRadius = 0.0f;
	Bool m_cylinderAOE = FALSE;
	Bool m_drainLife = FALSE;
	Real m_drainLifeMultiplier = 0.0f;
	SpecialPowerObjectFilter m_forceKillObjectFilter;
	UnsignedInt m_damageIntervalFrames = 0;
	UnsignedInt m_damageDurationFrames = 0;

	AsciiString m_projectileTemplateName;
	AsciiString m_warheadTemplateName;
	const ThingTemplate* m_projectileTemplate = nullptr;
	const WeaponTemplate* m_warheadTemplate = nullptr;
	Coord3D m_alwaysAttackHereOffset = {0.0f, 0.0f, 0.0f};
	Bool m_useAlwaysAttackOffset = FALSE;
	AsciiString m_launchBoneSlotOverride;

	Real m_shockWaveAmount = 0.0f;
	Real m_shockWaveRadius = 0.0f;
	Real m_shockWaveTaperOff = 0.0f;
	Real m_shockWaveArc = 0.0f;
	Bool m_shockWaveArcInverted = FALSE;
	Real m_shockWaveZMult = 1.0f;
	Real m_shockWaveSpeed = 0.0f;
	Real m_heroResist = 0.0f;
	Bool m_invertShockWave = FALSE;
	Bool m_flipDirection = FALSE;
	Bool m_onlyWhenJustDied = FALSE;
	SpecialPowerObjectFilter m_killObjectFilter;
	Bool m_shockWaveClearRadius = FALSE;
	Real m_shockWaveClearMult = 0.0f;
	Int m_shockWaveClearFlingHeight = 0;
	Real m_cyclonicFactor = 0.0f;
	Bool m_affectHordes = FALSE;

	AsciiString m_oclName;
	const ObjectCreationList* m_ocl = nullptr;

	AsciiString m_attributeModifier;
	std::vector<AsciiString> m_antiCategories;
	Bool m_affectHordeMembers = FALSE;
	WeaponSlotType m_lockWeaponSlot = PRIMARY_WEAPON;
};

Bool objectFilterMatches(const SpecialPowerObjectFilter& filter, const Object* source, const Object* target);
