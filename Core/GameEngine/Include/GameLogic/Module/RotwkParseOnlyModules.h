#pragma once

#include "Common/ModelState.h"
#include "Common/SpecialPowerINI.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/CollideModule.h"
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
	const AsciiString& getAppliedModifier() const { return m_appliedModifier; }
	static Bool holdsGround(Int stance);
#if !RTS_GENERALS
	const HordeContainModuleData::MeleeBehavior* getMeleeBehavior(Int stance) const;
#endif
	static const StancesBehavior* findForUnit(const Object* obj);
	void setStance(Int stance);
	void inheritStance(Int stance, const AsciiString& modifier, NameKeyType moduleTag);
	static StancesBehavior* find(const Object* obj);

private:
	Int m_stance;
	AsciiString m_appliedModifier;
};

struct LargeGroupAudioUpdateModuleData : public UpdateModuleData
{
	std::vector<AsciiString> m_keys;
	UnsignedShort m_unitWeight = 1;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class LargeGroupAudioUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( LargeGroupAudioUpdate, "LargeGroupAudioUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( LargeGroupAudioUpdate, LargeGroupAudioUpdateModuleData )

public:
	LargeGroupAudioUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
	UpdateSleepTime update() override { return UPDATE_SLEEP_FOREVER; }
	const LargeGroupAudioUpdateModuleData* audioData() const { return getLargeGroupAudioUpdateModuleData(); }
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

class NotifyTargetsOfImminentProbableCrushingUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(NotifyTargetsOfImminentProbableCrushingUpdate, "NotifyTargetsOfImminentProbableCrushingUpdate")
	MAKE_STANDARD_MODULE_MACRO(NotifyTargetsOfImminentProbableCrushingUpdate)

public:
	NotifyTargetsOfImminentProbableCrushingUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
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
	UpdateSleepTime update() override;
	void crcRetailState(Xfer* xfer);
};

struct HitReactionBehaviorModuleData : public UpdateModuleData
{
	UnsignedInt m_hitReactionLifeTimer1 = 0;
	UnsignedInt m_hitReactionLifeTimer2 = 0;
	UnsignedInt m_hitReactionLifeTimer3 = 0;
	Real m_hitReactionThreshold1 = 0.0f;
	Real m_hitReactionThreshold2 = 0.0f;
	Real m_hitReactionThreshold3 = 0.0f;
	Bool m_fastHitsResetReaction = FALSE;
	Bool m_hitsParalyze = FALSE;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class HitReactionBehavior final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( HitReactionBehavior, "HitReactionBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( HitReactionBehavior, HitReactionBehaviorModuleData )

public:
	HitReactionBehavior(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
	UpdateSleepTime update() override;
	DisabledMaskType getDisabledTypesToProcess() const override { return DISABLEDMASK_ALL; }

private:
	Real m_previousHealth = -1.0f;
	UnsignedInt m_remaining = 0;
	Int m_level = 0;
};

#if !RTS_GENERALS
struct OathbreakersFadeAwayBehaviorModuleData : public UpdateModuleData
{
	UnsignedInt m_frames = 0;
	static void buildFieldParse(MultiIniFieldParse& p);
};

class OathbreakersFadeAwayBehavior final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(OathbreakersFadeAwayBehavior, "OathbreakersFadeAwayBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(OathbreakersFadeAwayBehavior, OathbreakersFadeAwayBehaviorModuleData)

public:
	OathbreakersFadeAwayBehavior(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
	static Bool begin(Object* object);
	UpdateSleepTime update() override;
	DisabledMaskType getDisabledTypesToProcess() const override { return DISABLEDMASK_ALL; }

private:
	UnsignedInt m_endFrame = 0;
};
#endif

class HordeMemberCollide final : public CollideModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( HordeMemberCollide, "HordeMemberCollide" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( HordeMemberCollide, CollideModuleData )

public:
	HordeMemberCollide(Thing* thing, const ModuleData* data) : CollideModule(thing, data) {}
	void onCollide(Object* other, const Coord3D* location, const Coord3D* normal) override;
};

struct CastleMemberBehaviorModuleData : public BehaviorModuleData
{
	Bool m_countsForEvaCastleBreached = FALSE;
	Bool m_storeUpgradePrice = FALSE;
	AsciiString m_beingBuiltSound;
	AsciiString m_underAttackEvaEventIfKeep;
	AsciiString m_underAttackAllyEvaEventIfKeep;
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
	void onDelete() override;
	static CastleMemberBehavior* find(const Object* object);
	static void linkToCastle(Object& object, ObjectID castleID, ObjectID keepID, Bool isKeep, Bool ownedByCastle);
	static void recordUpgradePrice(Object& object, UnsignedInt cost);
	ObjectID getCastleID() const { return m_castleID; }
	ObjectID getKeepID() const { return m_keepID; }
	Bool isKeep() const { return m_isKeep; }
	Bool isOwnedByCastle() const { return m_ownedByCastle; }
	Bool countsForEvaCastleBreached() const { return getCastleMemberBehaviorModuleData()->m_countsForEvaCastleBreached; }
	const AsciiString& getBeingBuiltSound() const { return getCastleMemberBehaviorModuleData()->m_beingBuiltSound; }
	const AsciiString& getUnderAttackEvaEventIfKeep() const { return getCastleMemberBehaviorModuleData()->m_underAttackEvaEventIfKeep; }
	const AsciiString& getUnderAttackAllyEvaEventIfKeep() const { return getCastleMemberBehaviorModuleData()->m_underAttackAllyEvaEventIfKeep; }
	const AsciiString& getCampDestroyedAllyEvaEvent() const { return getCastleMemberBehaviorModuleData()->m_campDestroyedAllyEvaEvent; }
	const AsciiString& getCampDestroyedAttackerEvaEvent() const { return getCastleMemberBehaviorModuleData()->m_campDestroyedAttackerEvaEvent; }
	const AsciiString& getCampDestroyedOwnerEvaEvent() const { return getCastleMemberBehaviorModuleData()->m_campDestroyedOwnerEvaEvent; }
	UnsignedInt getStoredUpgradePrice() const { return m_storedUpgradePrice; }
	void onModelConditionChanged(const ModelConditionFlags& previous, const ModelConditionFlags& current);

private:
	void xferRetailBlocks(Xfer* xfer, Bool present);
	void stopBeingBuiltSound();

	ObjectID m_castleID = INVALID_ID;
	ObjectID m_keepID = INVALID_ID;
	Bool m_isKeep = FALSE;
	Bool m_ownedByCastle = FALSE;
	UnsignedInt m_storedUpgradePrice = 0;
	AudioHandle m_beingBuiltHandle = 0;
};
