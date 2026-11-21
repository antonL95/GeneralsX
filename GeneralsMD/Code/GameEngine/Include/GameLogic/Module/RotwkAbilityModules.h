#pragma once

#include <vector>

#include "Common/KindOf.h"
#include "Common/ObjectStatusTypes.h"
#include "Common/SpecialPowerINI.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/WeaponSet.h"
#include "GameLogic/WeaponSetFlags.h"

class FXList;
class Waypoint;
class ObjectCreationList;
class WeaponTemplate;

class WeaponFireSpecialAbilityUpdateModuleData : public SpecialAbilityUpdateModuleData
{
public:
	const WeaponTemplate *m_specialWeapon = nullptr;
	Int m_whichSpecialWeapon = 0;
	Bool m_skipContinue = FALSE;
	UnsignedInt m_busyForDuration = 0;
	Bool m_playWeaponPreFireFX = FALSE;
	Bool m_needLivingTargets = TRUE;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class WeaponFireSpecialAbilityUpdate : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( WeaponFireSpecialAbilityUpdate, "WeaponFireSpecialAbilityUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( WeaponFireSpecialAbilityUpdate, WeaponFireSpecialAbilityUpdateModuleData )

public:
	WeaponFireSpecialAbilityUpdate( Thing *thing, const ModuleData *moduleData );

protected:
	virtual void triggerAbilityEffect() override;
	virtual Bool skipsContinue() const override;
	virtual UnsignedInt holdFramesAfterTrigger() const override;
	virtual void addAbilityConditions( ModelConditionFlags &flags ) const override;
};

class ToggleMountedSpecialAbilityUpdateModuleData : public SpecialAbilityUpdateModuleData
{
public:
	Real m_opacityTarget = 1.0f;
	Bool m_triggerInstantlyOnCreate = FALSE;
	Bool m_cancelDisguiseWhenDismounting = FALSE;
	AsciiString m_mountedTemplate;
	std::vector<AsciiString> m_synchronizeTimerOnSpecialPower;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class ToggleMountedSpecialAbilityUpdate : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ToggleMountedSpecialAbilityUpdate, "ToggleMountedSpecialAbilityUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ToggleMountedSpecialAbilityUpdate, ToggleMountedSpecialAbilityUpdateModuleData )

public:
	ToggleMountedSpecialAbilityUpdate( Thing *thing, const ModuleData *moduleData );

	virtual void onObjectCreated() override;

	static Bool isMounted( const Object *obj );
	static void setMounted( Object *obj, Bool mounted );

protected:
	virtual void triggerAbilityEffect() override;
};

class SummonReplacementSpecialAbilityUpdate final : public ToggleMountedSpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SummonReplacementSpecialAbilityUpdate, "SummonReplacementSpecialAbilityUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(SummonReplacementSpecialAbilityUpdate, ToggleMountedSpecialAbilityUpdateModuleData)

public:
	SummonReplacementSpecialAbilityUpdate(Thing* thing, const ModuleData* data);
};

struct ActivateModuleEntry
{
	enum Position
	{
		POSITION_NONE,
		POSITION_OBJECT,
		POSITION_TARGET,
	};

	NameKeyType m_moduleTag = NAMEKEY_INVALID;
	AsciiString m_moduleTagName;
	Position m_position = POSITION_NONE;
};

class ActivateModuleSpecialPowerModuleData : public SpecialAbilityUpdateModuleData
{
public:
	std::vector<ActivateModuleEntry> m_triggers;

	static void buildFieldParse(MultiIniFieldParse& p);
	static void parseTriggerSpecialPower( INI *ini, void *instance, void *store, const void *userData );
};

class ActivateModuleSpecialPower : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ActivateModuleSpecialPower, "ActivateModuleSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ActivateModuleSpecialPower, ActivateModuleSpecialPowerModuleData )

public:
	ActivateModuleSpecialPower( Thing *thing, const ModuleData *moduleData );

protected:
	virtual void triggerAbilityEffect() override;
};

class HeroModeSpecialAbilityUpdateModuleData : public SpecialAbilityUpdateModuleData
{
public:
	UnsignedInt m_heroEffectDuration = 0;
	AsciiString m_heroAttributeModifier;
	Bool m_stopUnitBeforeActivating = FALSE;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class HeroModeSpecialAbilityUpdate : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( HeroModeSpecialAbilityUpdate, "HeroModeSpecialAbilityUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( HeroModeSpecialAbilityUpdate, HeroModeSpecialAbilityUpdateModuleData )

public:
	HeroModeSpecialAbilityUpdate( Thing *thing, const ModuleData *moduleData );

protected:
	virtual void triggerAbilityEffect() override;
};

class WeaponModeSpecialPowerUpdateModuleData : public UpdateModuleData
{
public:
	SpecialPowerModuleData m_power;
	AsciiString m_attributeModifier;
	UnsignedInt m_duration = 0;
	WeaponSlotType m_lockWeaponSlot = (WeaponSlotType)-1;
	WeaponSetFlags m_weaponSetFlags;
	AudioEventRTS m_initiateSound;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class WeaponModeSpecialPowerUpdate : public SpecialPowerUpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( WeaponModeSpecialPowerUpdate, "WeaponModeSpecialPowerUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( WeaponModeSpecialPowerUpdate, WeaponModeSpecialPowerUpdateModuleData )

public:
	WeaponModeSpecialPowerUpdate( Thing *thing, const ModuleData *moduleData );

	static Int getInterfaceMask() { return MODULEINTERFACE_UPDATE | MODULEINTERFACE_SPECIAL_POWER; }
	void resolveCastOwner();
	void onDelete() override;
	SpecialPowerModuleInterface* getSpecialPower() override { return m_power; }
	Bool isModuleForPower(const SpecialPowerTemplate* power) const;
	WeaponSlotType getModeWeaponSlot() const { return getWeaponModeSpecialPowerUpdateModuleData()->m_lockWeaponSlot; }
	virtual SpecialPowerUpdateInterface* getSpecialPowerUpdateInterface() override { return this; }
	virtual Bool initiateIntentToDoSpecialPower( const SpecialPowerTemplate *specialPowerTemplate, const Object *targetObj, const Coord3D *targetPos, const Waypoint *way, UnsignedInt commandOptions ) override;
	virtual Bool isSpecialAbility() const override { return FALSE; }
	virtual Bool isSpecialPower() const override { return TRUE; }
	virtual Bool isActive() const override { return m_endFrame != 0; }
	virtual CommandOption getCommandOption() const override { return (CommandOption)0; }
	virtual Bool doesSpecialPowerHaveOverridableDestinationActive() const override { return FALSE; }
	virtual Bool doesSpecialPowerHaveOverridableDestination() const override { return FALSE; }
	virtual void setSpecialPowerOverridableDestination( const Coord3D * ) override {}
	virtual Bool isPowerCurrentlyInUse( const CommandButton * = nullptr ) const override { return m_endFrame != 0; }
	virtual UpdateSleepTime update() override;

private:
	void endMode();

	SpecialPowerModule* m_power = nullptr;
	UnsignedInt m_endFrame = 0;
	Int m_condition = -1;
	UnsignedInt m_conditionEndFrame = 0;
	std::vector<ObjectID> m_lockedMembers;
};

class PlayerUpgradeSpecialPowerModuleData : public SpecialPowerModuleData
{
public:
	std::vector<AsciiString> m_upgrades;

	static void buildFieldParse(MultiIniFieldParse& fields);
};

class PlayerUpgradeSpecialPower : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( PlayerUpgradeSpecialPower, "PlayerUpgradeSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO( PlayerUpgradeSpecialPower )

public:
	using SpecialPowerModule::SpecialPowerModule;
	static ModuleData* friend_newModuleData(INI* ini);
	void onScienceUnlocked() override;
	const std::vector<AsciiString>& getGrantedUpgradeNames() const;

protected:
	void applyPowerEffect(const Coord3D* position) override;
};

class CloudBreakSpecialPowerModuleData : public SpecialPowerModuleData
{
public:
	AsciiString m_sunbeam;
	Int m_spacing = 0;
	const FXList* m_antiFX = nullptr;

	static void buildFieldParse(MultiIniFieldParse& fields);
};

class CloudBreakSpecialPower : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( CloudBreakSpecialPower, "CloudBreakSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO( CloudBreakSpecialPower )

public:
	using SpecialPowerModule::SpecialPowerModule;
	static ModuleData* friend_newModuleData(INI* ini);

protected:
	void applyPowerEffect(const Coord3D* position) override;
};

class TaintSpecialPowerModuleData : public SpecialPowerModuleData
{
public:
	AsciiString m_land;
	Real m_radius = 0.0f;
	const FXList* m_fx = nullptr;
	const ObjectCreationList* m_ocl = nullptr;

	static void buildFieldParse(MultiIniFieldParse& fields);
};

class TaintSpecialPower : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( TaintSpecialPower, "TaintSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO( TaintSpecialPower )

public:
	using SpecialPowerModule::SpecialPowerModule;
	static ModuleData* friend_newModuleData(INI* ini);

protected:
	void applyPowerEffect(const Coord3D* position) override;
};

class UntamedAllegianceSpecialPower : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( UntamedAllegianceSpecialPower, "UntamedAllegianceSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO( UntamedAllegianceSpecialPower )

public:
	using SpecialPowerModule::SpecialPowerModule;
	Bool isValidPowerTarget(const Object* target) const override;

protected:
	void applyPowerEffect(const Coord3D* position) override;
};

class MonitorConditionUpdateModuleData : public UpdateModuleData
{
public:
	WeaponSetFlags m_weaponSetFlags;
	AsciiString m_weaponToggleCommandSet;
	ModelConditionFlags m_modelConditionFlags;
	AsciiString m_modelConditionCommandSet;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class MonitorConditionUpdate : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( MonitorConditionUpdate, "MonitorConditionUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( MonitorConditionUpdate, MonitorConditionUpdateModuleData )

public:
	MonitorConditionUpdate( Thing *thing, const ModuleData *moduleData );

	virtual UpdateSleepTime update() override;
	const AsciiString& getActiveCommandSet() const;

private:
	AsciiString m_lastCommandSet;
	AsciiString m_savedWeaponSet;
	AsciiString m_savedModelSet;
};

struct AutoAbilityQuery
{
	Int m_count = 1;
	SpecialPowerObjectFilter m_filter;
};

class AutoAbilityBehaviorModuleData : public UpdateModuleData
{
public:
	const SpecialPowerTemplate *m_specialAbility = nullptr;
	std::vector<AutoAbilityQuery> m_queries;
	ObjectStatusMaskType m_forbiddenStatus;
	Real m_maxScanRange = 0.0f;
	Real m_minScanRange = 0.0f;
	UnsignedInt m_idleFrames = 0;
	Bool m_startsActive = FALSE;
	Bool m_allowSelf = TRUE;
	Bool m_adjustAttackMeleePosition = FALSE;
	Bool m_baseMaxRangeFromStartPos = FALSE;

	static void buildFieldParse(MultiIniFieldParse& p);
	static void parseQuery( INI *ini, void *instance, void *store, const void *userData );
	static void parseStatusNames( INI *ini, void *instance, void *store, const void *userData );
	static void parseIdleSeconds( INI *ini, void *instance, void *store, const void *userData );
};

class AutoAbilityBehavior : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AutoAbilityBehavior, "AutoAbilityBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AutoAbilityBehavior, AutoAbilityBehaviorModuleData )

public:
	AutoAbilityBehavior( Thing *thing, const ModuleData *moduleData );

	virtual UpdateSleepTime update() override;

	const SpecialPowerTemplate *getSpecialAbility() const;
	Bool isAutoActive() const;
	void setToggledState( Int state ) { m_toggledState = state; }

	static AutoAbilityBehavior *find( Object *obj, const SpecialPowerTemplate *power );

private:
	Bool queryMatches( const AutoAbilityQuery &query, UnsignedInt options, Object *&target );
	Bool objectMatches( const SpecialPowerObjectFilter &filter, const Object *other ) const;
	Bool cast( UnsignedInt options, Object *target );

	Int m_toggledState = 0;
	UnsignedInt m_idleSinceFrame = 0;
};

class PlayerHealSpecialPowerModuleData : public SpecialPowerModuleData
{
public:
	KindOfMaskType m_healAffects;
	Real m_healAmount = 0.0f;
	Real m_healRadius = 0.0f;
	const FXList *m_healFX = nullptr;
	const ObjectCreationList *m_healOCL = nullptr;
	Bool m_healAsPercent = TRUE;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class PlayerHealSpecialPower : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( PlayerHealSpecialPower, "PlayerHealSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( PlayerHealSpecialPower, PlayerHealSpecialPowerModuleData )

public:
	PlayerHealSpecialPower( Thing *thing, const ModuleData *moduleData );

	virtual void doSpecialPower( UnsignedInt commandOptions ) override;
	virtual void doSpecialPowerAtObject( Object *obj, UnsignedInt commandOptions ) override;
	virtual void doSpecialPowerAtLocation( const Coord3D *loc, Real angle, UnsignedInt commandOptions ) override;
	virtual void markSpecialPowerTriggered( const Coord3D *location ) override;

private:
	void heal( const Coord3D *center );
	void healObject( Object *other );

	Coord3D m_pendingCenter;
	Bool m_hasPendingCenter = FALSE;
};
