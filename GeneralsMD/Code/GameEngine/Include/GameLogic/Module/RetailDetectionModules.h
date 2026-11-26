#pragma once

#include "Common/SpecialPowerINI.h"
#include "Common/NameKeyGenerator.h"

#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/WeaponSetFlags.h"


class SpecialEnemySenseUpdateModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& fields);
	SpecialPowerObjectFilter m_filter;
	Real m_range = 0.0f;
	UnsignedInt m_interval = LOGICFRAMES_PER_SECOND;
};

class SpecialEnemySenseUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SpecialEnemySenseUpdate, "SpecialEnemySenseUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(SpecialEnemySenseUpdate, SpecialEnemySenseUpdateModuleData)

public:
	SpecialEnemySenseUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
	UpdateSleepTime update() override;
};

namespace RetailDetection
{
Bool nearTrees(const Coord3D& position);
Bool nearTrees(const Object* object);
Bool hasUpgrade(const Object* object, const AsciiString& name);
Bool hasNearbyObserver(const Object* object, Real range, Bool friendliesOnly = FALSE,
	const SpecialPowerObjectFilter* filter = nullptr);
}

struct RetailInvisibilityNugget
{
	static void parse(INI* ini, void* instance, void* store, const void* userData);
	enum Type { CAMOUFLAGE, STEALTH };
	enum Option
	{
		DETECTED_BY_FRIENDLIES = 1 << 0,
		UNTOGGLE_HIDDEN_WHEN_LEAVING_STEALTH = 1 << 1,
		ALLOW_NEAR_TREES = 1 << 2
	};
	ModelConditionFlags m_forbiddenModels;
	WeaponSetFlags m_forbiddenWeapons;
	ObjectStatusMaskType m_hintDetected;
	const FXList* m_enterFX = nullptr;
	const FXList* m_exitFX = nullptr;
	Real m_detectionRange = 0.0f;
	UnsignedInt m_options = 0;
	Int m_type = CAMOUFLAGE;
	Bool m_awayFromTrees = FALSE;
	Bool m_takingDamage = FALSE;
	Bool m_usingAbility = FALSE;
	Bool m_moving = FALSE;
	Bool m_firingAny = FALSE;
	Bool canConceal(const Object* object) const;
};

class InvisibilityUpdateModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& fields);
	RetailInvisibilityNugget m_nugget;
	std::vector<AsciiString> m_required;
	std::vector<AsciiString> m_forbidden;
	SpecialPowerObjectFilter m_filter;
	AsciiString m_moveVoice;
	AsciiString m_arrivalVoice;
	UnsignedInt m_period = LOGICFRAMES_PER_SECOND;
	Real m_range = 0.0f;
	Bool m_startsActive = FALSE;
	Bool m_broadcast = FALSE;
};

class InvisibilityUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(InvisibilityUpdate, "InvisibilityUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(InvisibilityUpdate, InvisibilityUpdateModuleData)

public:
	InvisibilityUpdate(Thing* thing, const ModuleData* data);
	static InvisibilityUpdate* find(const Object* object, NameKeyType tag = NAMEKEY_INVALID);
	void toggle(UnsignedInt duration);
	Bool isAvailable() const;
	Bool isEnabled() const;
	Real timerProgress(UnsignedInt duration) const;
	const RetailInvisibilityNugget& getNugget() const;
	UpdateSleepTime update() override;

private:
	void transferState(Xfer* xfer);
	void deactivate();
	UnsignedInt m_end = 0;
	UnsignedInt m_nextPulse = 0;
	Bool m_active = FALSE;
	Bool m_wasConcealed = FALSE;
	Bool m_wasNearTrees = FALSE;
	Bool m_wasMovingToTrees = FALSE;
};

class ToggleHiddenSpecialAbilityUpdateModuleData : public SpecialAbilityUpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& fields);
	Bool m_showTimer = FALSE;
};

class ToggleHiddenSpecialAbilityUpdate final : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ToggleHiddenSpecialAbilityUpdate, "ToggleHiddenSpecialAbilityUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(ToggleHiddenSpecialAbilityUpdate, ToggleHiddenSpecialAbilityUpdateModuleData)

public:
	ToggleHiddenSpecialAbilityUpdate(Thing* thing, const ModuleData* data) : SpecialAbilityUpdate(thing, data) {}
	static Real timerProgress(const Object* object, const SpecialPowerTemplate* power);

protected:
	Bool acceptsIntent(const Object* target, const Coord3D* position) const override;
	void triggerAbilityEffect() override;
};
