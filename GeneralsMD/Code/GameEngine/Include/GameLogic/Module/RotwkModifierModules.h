#pragma once

#include <vector>

#include "Common/ModelState.h"
#include "Common/SpecialPowerINI.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/UpgradeModule.h"

class EnragedBehaviorModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);
	UnsignedInt m_life = 0;
};

class EnragedBehavior final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(EnragedBehavior, "EnragedBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(EnragedBehavior, EnragedBehaviorModuleData)

public:
	EnragedBehavior(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
	void setEnraged(Bool enabled);
	static void setScriptEnraged(Object& object, Bool enabled);
	UpdateSleepTime update() override;

private:
	void xferState(Xfer* xfer);
	UnsignedInt m_end = 0;
	Bool m_user45 = FALSE;
	Bool m_aflame = FALSE;
};

class EntEnragedUpdateModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);
	SpecialPowerObjectFilter m_hated;
	SpecialPowerObjectFilter m_friendlyDead;
	Real m_radius = 300.0f;
	UnsignedInt m_duration = LOGICFRAMES_PER_SECOND;
	UnsignedInt m_cooldown = 0;
	UnsignedInt m_transition = 0;
	const FXList* m_transitionFX = nullptr;
	const FXList* m_onFX = nullptr;
	const FXList* m_offFX = nullptr;
};

class EntEnragedUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(EntEnragedUpdate, "EntEnragedUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(EntEnragedUpdate, EntEnragedUpdateModuleData)

public:
	EntEnragedUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
	UpdateSleepTime update() override;
	DisabledMaskType getDisabledTypesToProcess() const override { return MAKE_DISABLED_MASK(DISABLED_PARALYZED); }

private:
	void xferState(Xfer* xfer);
	UnsignedInt m_end = 0;
	UnsignedInt m_cooldownEnd = 0;
	Bool m_wasBurning = FALSE;
};

class OneRingPenaltyUpdateModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	AsciiString m_specialObject;
	AsciiString m_discoveredSound;
	UnsignedInt m_beforeSpawn = 0;
	UnsignedInt m_roaming = 0;
	UnsignedInt m_suppression = 0;
	UnsignedInt m_freeze = 0;
	Real m_distance = 0.0f;
};

class OneRingPenaltyUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(OneRingPenaltyUpdate, "OneRingPenaltyUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(OneRingPenaltyUpdate, OneRingPenaltyUpdateModuleData)

public:
	OneRingPenaltyUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
	UpdateSleepTime update() override;
	void onDelete() override;
	DisabledMaskType getDisabledTypesToProcess() const override;

private:
	void clearEye();
	void transferState(Xfer* xfer);
	ObjectID m_eye = INVALID_ID;
	UnsignedInt m_wornSince = 0;
	UnsignedInt m_eyeSince = 0;
	Real m_angle = 0.0f;
};

class BloodthirstyUpdateModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	SpecialPowerObjectFilter m_sacrificeFilter;
	AsciiString m_initiateVoice;
	AsciiString m_initiateVoice2;
	Int m_numToSacrifice = 0;
	Real m_experienceModifier = 1.0f;
};

class BloodthirstyUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(BloodthirstyUpdate, "BloodthirstyUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(BloodthirstyUpdate, BloodthirstyUpdateModuleData)

public:
	BloodthirstyUpdate(Thing* thing, const ModuleData* data);
	static BloodthirstyUpdate* find(const Object* source);
	Bool canSacrifice(const Object* donor) const;
	void orderSacrifice(Object* donor);
	UpdateSleepTime update() override;

private:
	ObjectID m_donor = INVALID_ID;
	UnsignedInt m_deadline = 0;
};

enum EmotionType
{
	EMOTION_TAUNT, EMOTION_CHEER, EMOTION_HERO_CHEER, EMOTION_POINT, EMOTION_FEAR,
	EMOTION_UNCONTROLLABLE_FEAR, EMOTION_TERROR, EMOTION_DOOM, EMOTION_ALERT,
	EMOTION_QUARRELSOME, EMOTION_OVERRIDE, EMOTION_PRE_CRUSH, EMOTION_BRACE
};

struct RetailEmotionEntry
{
	AsciiString m_name;
	AsciiString m_modifier;
	Int m_type = -1;
	Int m_duration = -1;
	Int m_aiLock = -1;
};

class EmotionTrackerUpdateModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	std::vector<RetailEmotionEntry> m_emotions;
	SpecialPowerObjectFilter m_afraidOf;
	SpecialPowerObjectFilter m_alwaysAfraidOf;
	SpecialPowerObjectFilter m_pointAt;
	SpecialPowerObjectFilter m_tauntExcluded;
	Real m_tauntRadius = 0.0f;
	Real m_fearRadius = 0.0f;
	Real m_heroRadius = 0.0f;
	Real m_quarrelChance = 0.0f;
	UnsignedInt m_tauntInterval = LOGICFRAMES_PER_SECOND;
	Int m_immuneRank = 0;
	Bool m_ignoreVeterancy = FALSE;
};

class EmotionTrackerUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(EmotionTrackerUpdate, "EmotionTrackerUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(EmotionTrackerUpdate, EmotionTrackerUpdateModuleData)

public:
	EmotionTrackerUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
	static void parseEmotionNugget(INI* ini);
	static void parseEmotionType(INI* ini, void* instance, void* store, const void* userData);
	static void clearEmotionNuggets();
	static Bool signal(Object* victim, EmotionType type, const Object* source);
	static void queuePromotionEmotion(Object* object, const AsciiString& name);
	static void force(Object* victim, Int type, Real seconds);
	static Bool signalWeaponEmotion(Object* victim, EmotionType type, const Object* source, UnsignedInt duration);
	static Bool canSufferFear(const Object* object);
	static Bool ownsCrushBraceCondition(const Object* object);
	static Bool blocksPlayerCommands(const Object* object);
	UpdateSleepTime update() override;
	void xferEmotionState(Xfer* xfer);
	Bool hasPromotionEmotions() const { return m_promotionEmotions != 0; }
	void xferPromotionEmotions(Xfer* xfer);
	void clearPromotionEmotions() { m_promotionEmotions = 0; }

private:
	Bool startEmotion(EmotionType type, const Object* source, Real enemyThreat, UnsignedInt forcedDuration = 0, UnsignedInt weaponDuration = 0);
	void finishEmotion();
	void xferRetailBlocks(Xfer* xfer, Bool present);

	Int m_activeEntry = -1;
	UnsignedInt m_endFrame = 0;
	UnsignedInt m_recheckFrame = 0;
	UnsignedInt m_modifierFrame = 0;
	UnsignedInt m_promotionEmotions = 0;
	ModelConditionFlags m_appliedConditions;
	Int m_pendingForcedType = -1;
	Bool m_forced = FALSE;
};

class LargeGroupBonusUpdateModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	SpecialPowerObjectFilter m_memberFilter;
	AsciiString m_modifier;
	UnsignedInt m_updateRate = LOGICFRAMES_PER_SECOND;
	Int m_count = 0;
	Real m_radius = 0.0f;
	Real m_rubOffRadius = 0.0f;
	Bool m_alliesOnly = FALSE;
};

class LargeGroupBonusUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(LargeGroupBonusUpdate, "LargeGroupBonusUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(LargeGroupBonusUpdate, LargeGroupBonusUpdateModuleData)

public:
	LargeGroupBonusUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
	UpdateSleepTime update() override;

private:
	Bool m_meetsThreshold = FALSE;
};

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
	Bool m_affectGood = FALSE;
	Bool m_affectEvil = FALSE;
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

struct RadiateFearUpdateModuleData : public UpdateModuleData
{
	UpgradeMuxData m_upgrades;
	SpecialPowerObjectFilter m_victimFilter;
	Bool m_initiallyActive = FALSE;
	Bool m_generateFear = FALSE;
	Bool m_generateTerror = FALSE;
	Bool m_generateUncontrollableFear = FALSE;
	Int m_whichSpecialPower = 0;
	Real m_pulseRadius = 0.0f;
	UnsignedInt m_pulseInterval = 0;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class RadiateFearUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(RadiateFearUpdate, "RadiateFearUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(RadiateFearUpdate, RadiateFearUpdateModuleData)

public:
	RadiateFearUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
	UpdateSleepTime update() override;
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

class AllowBannerSpawnUpgrade final : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(AllowBannerSpawnUpgrade, "AllowBannerSpawnUpgrade")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(AllowBannerSpawnUpgrade, UpgradeModuleData)

public:
	using UpgradeModule::UpgradeModule;

protected:
	void upgradeImplementation() override;
	Bool isSubObjectsUpgrade() override { return FALSE; }
};
