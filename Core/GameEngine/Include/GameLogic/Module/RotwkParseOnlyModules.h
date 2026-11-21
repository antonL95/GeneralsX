#pragma once

#include "Common/SpecialPowerINI.h"
#include "GameLogic/Module/UpdateModule.h"

struct StancesBehaviorModuleData : public BehaviorModuleData
{
	AsciiString m_stanceTemplate;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class StancesBehavior final : public BehaviorModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( StancesBehavior, "StancesBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( StancesBehavior, StancesBehaviorModuleData )

public:
	StancesBehavior(Thing* thing, const ModuleData* data);

	virtual void onObjectCreated() override;
	Int getStance() const { return m_stance; }
	static Bool holdsGround(Int stance);
	static const StancesBehavior* findForUnit(const Object* obj);
	void setStance(Int stance);
	static StancesBehavior* find(const Object* obj);

private:
	Int m_stance;
	AsciiString m_appliedModifier;
};

struct LargeGroupAudioUpdateModuleData : public UpdateModuleData
{
	std::vector<AsciiString> m_keys;
	Int m_unitWeight = 0;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class LargeGroupAudioUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( LargeGroupAudioUpdate, "LargeGroupAudioUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( LargeGroupAudioUpdate, LargeGroupAudioUpdateModuleData )

public:
	LargeGroupAudioUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
	UpdateSleepTime update() override { return UPDATE_SLEEP_FOREVER; }
};

struct HordeNotifyTargetsOfImminentProbableCrushingUpdateModuleData : public UpdateModuleData
{
	Real m_scanWidth = 0.0f;
	static void buildFieldParse(MultiIniFieldParse& p);
};

class HordeNotifyTargetsOfImminentProbableCrushingUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(HordeNotifyTargetsOfImminentProbableCrushingUpdate, "HordeNotifyTargetsOfImminentProbableCrushingUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(HordeNotifyTargetsOfImminentProbableCrushingUpdate, HordeNotifyTargetsOfImminentProbableCrushingUpdateModuleData)

public:
	HordeNotifyTargetsOfImminentProbableCrushingUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
	UpdateSleepTime update() override;
};

struct PickupStuffUpdateModuleData : public UpdateModuleData
{
	Bool m_skirmishAIOnly = FALSE;
	SpecialPowerObjectFilter m_stuffToPickUp;
	Real m_scanRange = 0.0f;
	Real m_scanIntervalSeconds = 0.0f;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class PickupStuffUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( PickupStuffUpdate, "PickupStuffUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( PickupStuffUpdate, PickupStuffUpdateModuleData )

public:
	PickupStuffUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
	UpdateSleepTime update() override { return UPDATE_SLEEP_FOREVER; }
};

struct HitReactionBehaviorModuleData : public BehaviorModuleData
{
	Int m_hitReactionLifeTimer1 = 0;
	Int m_hitReactionLifeTimer2 = 0;
	Int m_hitReactionLifeTimer3 = 0;
	Real m_hitReactionThreshold1 = 0.0f;
	Real m_hitReactionThreshold2 = 0.0f;
	Real m_hitReactionThreshold3 = 0.0f;
	Bool m_fastHitsResetReaction = FALSE;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class HitReactionBehavior final : public BehaviorModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( HitReactionBehavior, "HitReactionBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( HitReactionBehavior, HitReactionBehaviorModuleData )

public:
	HitReactionBehavior(Thing* thing, const ModuleData* data) : BehaviorModule(thing, data) {}
};

class HordeMemberCollide final : public BehaviorModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( HordeMemberCollide, "HordeMemberCollide" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( HordeMemberCollide, BehaviorModuleData )

public:
	HordeMemberCollide(Thing* thing, const ModuleData* data) : BehaviorModule(thing, data) {}
};

struct CastleMemberBehaviorModuleData : public BehaviorModuleData
{
	Bool m_countsForEvaCastleBreached = FALSE;
	Bool m_storeUpgradePrice = FALSE;
	AsciiString m_beingBuiltSound;
	AsciiString m_campDestroyedAllyEvaEvent;
	AsciiString m_campDestroyedAttackerEvaEvent;
	AsciiString m_campDestroyedOwnerEvaEvent;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class CastleMemberBehavior final : public BehaviorModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( CastleMemberBehavior, "CastleMemberBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( CastleMemberBehavior, CastleMemberBehaviorModuleData )

public:
	CastleMemberBehavior(Thing* thing, const ModuleData* data) : BehaviorModule(thing, data) {}
};
