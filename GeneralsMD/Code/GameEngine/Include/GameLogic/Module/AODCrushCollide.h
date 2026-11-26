#pragma once

#include "Common/SpecialPowerINI.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/CollideModule.h"

class FXList;
class ObjectCreationList;

struct AODCrushCollideModuleData : public CollideModuleData
{
	const FXList* m_smallFX = nullptr;
	const FXList* m_mediumFX = nullptr;
	const FXList* m_largeFX = nullptr;
	const ObjectCreationList* m_smallOCL = nullptr;
	const ObjectCreationList* m_mediumOCL = nullptr;
	const ObjectCreationList* m_largeOCL = nullptr;
	SpecialPowerObjectFilter m_specialObject{.m_rules = SpecialPowerObjectFilter::RULE_NONE};
	Real m_damage = 0.0f;
	Real m_specialDamage = 0.0f;
	Real m_selfDamage = 0.0f;
	DamageType m_damageType = DAMAGE_EXPLOSION;
	DamageType m_specialDamageType = DAMAGE_EXPLOSION;
	DamageType m_selfDamageType = DAMAGE_EXPLOSION;
	DeathType m_deathType = DEATH_NORMAL;
	DeathType m_specialDeathType = DEATH_NORMAL;
	DeathType m_selfDeathType = DEATH_NORMAL;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class AODCrushCollide final : public CollideModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(AODCrushCollide, "AODCrushCollide")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(AODCrushCollide, AODCrushCollideModuleData)

public:
	AODCrushCollide(Thing* thing, const ModuleData* data) : CollideModule(thing, data) {}
	void onCollide(Object* other, const Coord3D* loc, const Coord3D* normal) override;
};
