#pragma once

#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/RespawnModules.h"
#include "GameLogic/Module/StructureBody.h"
#include "GameLogic/Module/UpgradeModule.h"

class FXList;
class WeaponTemplate;

class DelayedDeathBodyModuleData : public RespawnBodyModuleData
{
public:
	UnsignedInt m_delayedDeathTime = 0;
	Bool m_immortalUntilDeathTime = FALSE;
	Bool m_doHealthCheck = FALSE;
	const FXList* m_invulnerableFX = nullptr;
	AsciiString m_delayedDeathPrerequisiteUpgrade;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class DelayedDeathBody : public RespawnBody
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(DelayedDeathBody, "DelayedDeathBody")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(DelayedDeathBody, DelayedDeathBodyModuleData)

public:
	DelayedDeathBody(Thing* thing, const ModuleData* moduleData);
};

class PorcupineFormationBodyModuleData : public ActiveBodyModuleData
{
public:
	const WeaponTemplate* m_damageWeaponTemplate = nullptr;
	const WeaponTemplate* m_crushDamageWeaponTemplate = nullptr;
	Int m_crusherLevelResisted = 0;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class PorcupineFormationBodyModule : public ActiveBody
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(PorcupineFormationBodyModule, "PorcupineFormationBodyModule")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(PorcupineFormationBodyModule, PorcupineFormationBodyModuleData)

public:
	PorcupineFormationBodyModule(Thing* thing, const ModuleData* moduleData);
	const PorcupineFormationBodyModuleData* getFormationBodyData() const { return getPorcupineFormationBodyModuleData(); }
};

class SymbioticStructuresBodyModuleData : public StructureBodyModuleData
{
public:
	AsciiString m_symbiote;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class SymbioticStructuresBody : public StructureBody
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SymbioticStructuresBody, "SymbioticStructuresBody")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(SymbioticStructuresBody, SymbioticStructuresBodyModuleData)

public:
	SymbioticStructuresBody(Thing* thing, const ModuleData* moduleData);
};

class DetachableRiderBodyModuleData : public ActiveBodyModuleData
{
public:
	UpgradeMuxData m_upgradeMuxData;
	Real m_healthPercentageWhenRiderDies = 0.0f;
	Bool m_startsActive = FALSE;
	Real m_riderlessDeathChance = 0.0f;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class DetachableRiderBody : public ActiveBody
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(DetachableRiderBody, "DetachableRiderBody")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(DetachableRiderBody, DetachableRiderBodyModuleData)

public:
	DetachableRiderBody(Thing* thing, const ModuleData* moduleData);
};
