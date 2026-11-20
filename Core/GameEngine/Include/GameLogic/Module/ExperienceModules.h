#pragma once

#include "Common/SpecialPowerINI.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Module/UpgradeModule.h"

struct ExperienceLevelCreateModuleData : CreateModuleData
{
	Int m_levelToGrant = 1;
	Bool m_multiplayerOnly = FALSE;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class ExperienceLevelCreate final : public CreateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ExperienceLevelCreate, "ExperienceLevelCreate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(ExperienceLevelCreate, ExperienceLevelCreateModuleData)

public:
	ExperienceLevelCreate(Thing* thing, const ModuleData* data) : CreateModule(thing, data) {}
	void onCreate() override;
};

struct LevelUpUpgradeModuleData : UpgradeModuleData
{
	Int m_levelsToGain = 1;
	Int m_levelCap = 0;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class LevelUpUpgrade final : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(LevelUpUpgrade, "LevelUpUpgrade")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(LevelUpUpgrade, LevelUpUpgradeModuleData)

public:
	LevelUpUpgrade(Thing* thing, const ModuleData* data) : UpgradeModule(thing, data) {}

protected:
	void upgradeImplementation() override;
	Bool isSubObjectsUpgrade() override { return FALSE; }
};

struct ShareExperienceBehaviorModuleData : BehaviorModuleData
{
	SpecialPowerObjectFilter m_filter;
	Real m_radius = 0.0f;
	Real m_dropOff = 0.0f;

	static void buildFieldParse(MultiIniFieldParse& p);
};

struct SharedExperience
{
	ObjectID object;
	Real fraction;
};

class ShareExperienceBehavior final : public BehaviorModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ShareExperienceBehavior, "ShareExperienceBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(ShareExperienceBehavior, ShareExperienceBehaviorModuleData)

public:
	ShareExperienceBehavior(Thing* thing, const ModuleData* data) : BehaviorModule(thing, data) {}
	void collectRecipients(std::vector<SharedExperience>& recipients) const;
};

struct LevelGrantSpecialPowerModuleData : SpecialAbilityUpdateModuleData
{
	Int m_experience = 0;
	Real m_radius = 0.0f;
	SpecialPowerObjectFilter m_acceptanceFilter;
	std::vector<AsciiString> m_effects;
	Bool m_useKindOf = FALSE;
	KindOfMaskType m_affectsKindOf;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class LevelGrantSpecialPower final : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(LevelGrantSpecialPower, "LevelGrantSpecialPower")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(LevelGrantSpecialPower, LevelGrantSpecialPowerModuleData)

public:
	LevelGrantSpecialPower(Thing* thing, const ModuleData* data) : SpecialAbilityUpdate(thing, data) {}

protected:
	void triggerAbilityEffect() override;
};
