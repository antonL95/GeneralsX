#pragma once

#include <vector>

#include "Common/SpecialPowerINI.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/UpgradeModule.h"

class AttributeModifierAuraUpdateModuleData : public UpdateModuleData
{
public:
	UpgradeMuxData m_upgradeMuxData;
	Bool m_startsActive = FALSE;
	AsciiString m_bonusName;
	UnsignedInt m_refreshDelay = 0;
	Real m_range = 0.0f;
	SpecialPowerObjectFilter m_objectFilter;
	Bool m_hasObjectFilter = FALSE;
	Bool m_targetEnemy = FALSE;
	Bool m_allowPowerWhenAttacking = TRUE;
	UnsignedInt m_requiredConditions = 0;
	UnsignedInt m_antiCategories = 0;
	AsciiString m_antiFX;
	Bool m_affectGood = TRUE;
	Bool m_affectEvil = TRUE;
	Bool m_runWhileDead = FALSE;
	Bool m_allowSelf = FALSE;
	Bool m_affectContainedOnly = FALSE;
	Int m_maxActiveRank = 0;
	Bool m_requiresAllConflictingTriggers = FALSE;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class AttributeModifierAuraUpdate : public UpdateModule, public UpgradeMux
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AttributeModifierAuraUpdate, "AttributeModifierAuraUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AttributeModifierAuraUpdate, AttributeModifierAuraUpdateModuleData )

public:
	AttributeModifierAuraUpdate( Thing *thing, const ModuleData *moduleData );

	static Int getInterfaceMask() { return UpdateModule::getInterfaceMask() | MODULEINTERFACE_UPGRADE; }
	virtual UpgradeModuleInterface* getUpgrade() override { return this; }
	virtual UpdateSleepTime update() override;
	virtual void onObjectCreated() override;
	virtual Bool resetUpgrade( const UpgradeMaskType& keyMask ) override;
	Bool isAuraActive() const;
	Int getTargetsLastPulse() const { return m_targetsLastPulse; }

protected:
	virtual void upgradeImplementation() override { setWakeFrame(getObject(), UPDATE_SLEEP_NONE); }
	virtual void getUpgradeActivationMasks(UpgradeMaskType& activation, UpgradeMaskType& conflicting) const override
	{
		getAttributeModifierAuraUpdateModuleData()->m_upgradeMuxData.getUpgradeActivationMasks(activation, conflicting);
	}
	virtual void performUpgradeFX() override { getAttributeModifierAuraUpdateModuleData()->m_upgradeMuxData.performUpgradeFX(getObject()); }
	virtual void processUpgradeRemoval() override { getAttributeModifierAuraUpdateModuleData()->m_upgradeMuxData.muxDataProcessUpgradeRemoval(getObject()); }
	virtual Bool requiresAllActivationUpgrades() const override { return getAttributeModifierAuraUpdateModuleData()->m_upgradeMuxData.m_requiresAllTriggers; }
	virtual Bool isSubObjectsUpgrade() override { return FALSE; }

private:
	void pulse();

	Int m_targetsLastPulse;
};

class PassiveAreaEffectBehaviorModuleData : public UpdateModuleData
{
public:
	Real m_effectRadius = 0.0f;
	UnsignedInt m_pingDelay = 0;
	Real m_healPercentPerSecond = 0.0f;
	std::vector<AsciiString> m_modifierNames;
	SpecialPowerObjectFilter m_allowFilter;
	Bool m_hasAllowFilter = FALSE;
	AsciiString m_upgradeRequired;
	Bool m_nonStackable = FALSE;
	UnsignedInt m_antiCategories = 0;
	AsciiString m_antiFX;
	AsciiString m_healFX;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class PassiveAreaEffectBehavior : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( PassiveAreaEffectBehavior, "PassiveAreaEffectBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( PassiveAreaEffectBehavior, PassiveAreaEffectBehaviorModuleData )

public:
	PassiveAreaEffectBehavior( Thing *thing, const ModuleData *moduleData );
	virtual UpdateSleepTime update() override;
};

class AttributeModifierUpgradeModuleData : public UpgradeModuleData
{
public:
	AsciiString m_attributeModifier;
	SpecialPowerAnimation m_customAnimAndDuration;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class AttributeModifierUpgrade : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AttributeModifierUpgrade, "AttributeModifierUpgrade" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AttributeModifierUpgrade, AttributeModifierUpgradeModuleData )

public:
	AttributeModifierUpgrade( Thing *thing, const ModuleData *moduleData );
	virtual Bool resetUpgrade( const UpgradeMaskType& keyMask ) override;

protected:
	virtual void upgradeImplementation() override;
	virtual Bool isSubObjectsUpgrade() override { return FALSE; }
};

namespace RotwkModifierModules
{
void applyHordeFormationModifiers( Object* horde );
void removeHordeFormationModifiers( Object* horde );
}
