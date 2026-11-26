#pragma once

#include "Common/HordeFormation.h"
#include "Common/ModelState.h"
#include "Common/ObjectStatusTypes.h"
#include "Common/SpecialPowerINI.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/TransportContain.h"
#include "GameLogic/WeaponSetFlags.h"

#include <deque>
#include <map>
#include <vector>

enum CanAttackResult CPP_11(: Int);

class HordeContainModuleData : public TransportContainModuleData
{
public:
	struct RankInfo
	{
		HordeRankInfo rank;
		Int revokedWeaponCondition = -1;
		Int grantedWeaponCondition = -1;
	};

	struct MeleeBehavior
	{
		AsciiString name;
		Real facingBonus = 0.0f;
		Real angleLimitCos = 0.0f;
		Real innerRange = 0.0f;
		Real outerRange = 0.0f;
		Real outerRangeBuildings = 0.0f;
		ModelConditionFlags idleModelConditions;
		UnsignedInt delayUntilIdle = 0;
		UnsignedInt delayRandomActivateMin = 0;
		UnsignedInt delayRandomActivateMax = 0;
	};

	HordeContainModuleData();

	static void buildFieldParse(MultiIniFieldParse& p);
	static void parseMeleeBehavior(INI* ini, void* instance, void* store, const void* userData);

	virtual const HordeLayout* getAsHordeLayout() const override { return &m_layout; }

	HordeLayout m_layout;
	std::vector<RankInfo> m_rankInfos;
	std::vector<HordeBannerCarrierPosition> m_bannerCarrierPositions;
	std::vector<HordeSplit> m_splitHordes;
	std::vector<HordeCombo> m_comboHordes;
	std::vector<Int> m_ranksThatStopAdvance;
	std::vector<Int> m_ranksToReleaseWhenAttacking;
	std::vector<Int> m_ranksToJustFreeWhenAttacking;
	std::vector<AsciiString> m_bannerCarriersAllowed;
	std::vector<AsciiString> m_leadersAllowed;
	std::vector<AsciiString> m_attributeModifiers;
	SpecialPowerObjectFilter m_manualPickUpFilter;
	MeleeBehavior m_meleeBehavior;
	AsciiString m_alternateFormation;
	AsciiString m_machineType;
	AsciiString m_evaEventLastMemberDeath;
	AsciiString m_livingWorldOverloadTemplate;
	Coord2D m_randomOffset;
	Coord3D m_leaderPosition;
	DeathTypeFlags m_bannerCarrierHordeDeathType;
	Real m_frontAngle;
	Real m_meleeAttackLeashDistance;
	Real m_backUpMinDistance;
	Real m_backUpMaxDistance;
	Real m_backupPercentage;
	Real m_cowerRadius;
	Real m_visionRearOverride;
	Real m_visionSideOverride;
	UnsignedInt m_backUpMinDelayTime;
	UnsignedInt m_backUpMaxDelayTime;
	UnsignedInt m_flankedDelay;
	UnsignedInt m_flankedDuration;
	UnsignedInt m_minimumHordeSize;
	Int m_leaderRank;
	Int m_splitHordeNumber;
	Int m_forcedLocomotorSet;
	UnsignedByte m_bannerCarrierMinLevel;
	Bool m_thisFormationIsTheMainFormation;
	Bool m_bannerCarrierDestroyHordeOnDeath;
	Bool m_isPorcupineFormation;
	Bool m_machineAllowed;
	Bool m_useSlowHordeMovement;
	Bool m_rankSplit;
	Bool m_notComboFormation;
	Bool m_useMarchingAnims;

private:
	static void parseInitialPayload(INI* ini, void* instance, void* store, const void* userData);
	static void parseSlots(INI* ini, void* instance, void* store, const void* userData);
	static void parseRankInfo(INI* ini, void* instance, void* store, const void* userData);
	static void parseBannerCarrierPosition(INI* ini, void* instance, void* store, const void* userData);
	static void parseSplitHorde(INI* ini, void* instance, void* store, const void* userData);
	static void parseComboHorde(INI* ini, void* instance, void* store, const void* userData);
	static void parseRankList(INI* ini, void* instance, void* store, const void* userData);
};

class BannerCarrierUpdateModuleData;

class HordeContain : public TransportContain
{
	friend class HordeAIUpdate;
	friend class BannerCarrierUpdate;
	friend struct RetailHordeSnapshotFixture;
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( HordeContain, "HordeContain" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( HordeContain, HordeContainModuleData )

public:
	HordeContain( Thing *thing, const ModuleData* moduleData );

	virtual Bool isValidContainerFor( const Object* obj, Bool checkCapacity ) const override;
	virtual Bool isEnclosingContainerFor( const Object *obj ) const override { return FALSE; }
	virtual Bool isDisplayedOnControlBar() const override { return FALSE; }
	Bool isHordeContain() const override { return TRUE; }
	void clientVisibleContainedFlashAsSelected() override;
#if RTS_GENERALS
	void onContaining(Object* obj) override;
	Bool isPassengerAllowedToFire() const override;
#else
	void onContaining(Object* obj, Bool wasSelected) override;
	Bool isPassengerAllowedToFire(ObjectID = INVALID_ID) const override;
#endif
	UpdateSleepTime update() override;
	DisabledMaskType getDisabledTypesToProcess() const override;
	virtual void onRemoving( Object *obj ) override;
	virtual void onCapture( Player *oldOwner, Player *newOwner ) override;
	virtual void addToContainList( Object *obj ) override;

	Int getMemberSlot( ObjectID id ) const;
	Bool rankCanAttack(const Object* member) const;
	Bool rankStopsAdvance(const Object* member) const;
	Real directionalVisionScale(const Object* target) const;
	Real maximumVisionScale() const;
	Object* findAttackTarget(const Object* attacker, AbleToAttackType attackType, CommandSourceType source,
		WeaponSlotType slot = (WeaponSlotType)-1, CanAttackResult* attackResult = nullptr) const;
	Object* findNearestLivingMember(const Coord3D& fromPos) const;
	static Bool canAddCrateBanner(const Object* anchor);
	static Bool addCrateBanner(Object* anchor);
	void onRankAdvanced(Int rank, Bool feedback = FALSE);
	Bool isBannerCarrier(const Object* member) const;
	struct FormationGoal
	{
		Coord3D slot;
		Coord3D position;
		Real speed;
		Real distance;
		Real error;
		Real facing;
		Bool hordeMoving;
		Bool backing;
		ObjectID backingSource;
	};
	Coord3D getMemberPosition(ObjectID id, Bool followLeader) const;
	Bool getFormationGoal(const Object* member, FormationGoal& goal) const;
	static Bool canSteerToFormationGoal(const Object* member, const FormationGoal& goal);
	static void constrainGroundMovement(const Object* member, Coord3D& movement);
	static void separateGroundMovement(const Object* member, Coord3D& movement);
	Real getFormationSpeedScale() const;
#if RTS_ZEROHOUR
	Real getFormationWheelRate(Real radius) const;
	UnsignedInt getFormationReformTravelFrames() const;
#endif
	void reassignSlotsByPosition();
	Bool canSetAlternateFormation(Bool alternate) const;
	Bool isAlternateFormation() const;
	void setAlternateFormation(Bool alternate);
	void toggleWeaponSet(const WeaponSetFlags& flags, Bool enabled);
#if !RTS_GENERALS
	Bool splitHorde();
	static Bool allowsBannerSpawn(const Object* anchor);
#endif
	static const HordeContain* get(const Object* object);
	Bool isPayloadCreated() const { return m_payloadCreated; }
	void suppressInitialPayload();
	void finishInitialPayload();
	Int getInitialPayloadCount() const;
	const ThingTemplate* getInitialPayloadTemplate(Int index) const;
	void addProducedMember(Object* member);
	UnsignedInt getLastMemberDamageFrame() const { return m_lastMemberDamageFrame; }
	void recordMemberDamage(UnsignedInt frame) { m_lastMemberDamageFrame = frame; }
	const HordeContainModuleData* getActiveFormation() const;
	const std::vector<AsciiString>& getFormationModifiers() const { return getActiveFormation()->m_attributeModifiers; }
	static Object* getAnchor(Object* member);
	static Bool isFlanked(const Object* victim, const Object* attacker, Bool record = TRUE);
	Bool beginBacking(const Object* source, Bool uncontrollable);
	enum class BackingPhase { Available, Backing, Recovered };
	BackingPhase updateBacking(Object* member);
	void xferRetailFormationCombat(Xfer* xfer, Bool includeBackingSource = TRUE);
	void xferRetailBlocks(Xfer* xfer, Bool present);
	static void applyWeaponDamage(Object* victim, DamageInfo& damage, Bool contactHit);
	static Drawable* resolveSelection(Drawable* draw);

protected:
	void onObjectCreated() override;
	void onDrawableBoundToObject() override;
	virtual void createPayload() override;
	void redeployOccupants() override {}

private:
	const Object* getBannerCarrier() const;
	const HordeBannerCarrierPosition* getBannerFormationPosition() const;
	HordePoint getBannerOffset() const;
	AsciiString getBannerUnitType() const;
	Bool addBannerCarrier(Bool feedback);
	void assignMemberSlot(Object* member);
	void updateReplenishment();
	void updateFlankState();
	Object* replenishMember(const BannerCarrierUpdateModuleData& banner);
	void rebuildMemberSlots();
	const HordeContainModuleData* findAlternateFormation() const;
	void updateFormationModelConditions(Object* object) const;
	void updateRankWeaponConditions(Object* member) const;
	std::vector<ObjectID> m_slotMembers;
	std::map<ObjectID, Int> m_memberSlots;
	std::vector<Coord2D> m_slotOffsets;
	const HordeContainModuleData* m_formationOverride = nullptr;
	Bool m_hadMembers = FALSE;
	Bool m_initialPayloadPending = FALSE;
	Bool m_restoreHadMembers = FALSE;
	Bool m_restoreBaseFormation = FALSE;
	AsciiString m_replenishmentBanner;
	UnsignedInt m_bannerRespawnFrame = 0;
	UnsignedInt m_unitSpawnFrame = 0;
	UnsignedInt m_lastMeleeFrame = 0;
	UnsignedInt m_lastMemberDamageFrame = 0;
	std::deque<Real> m_flankHeadings;
	std::map<ObjectID, UnsignedInt> m_flankedUntil;
	struct BackingMove
	{
		Coord3D goal;
		UnsignedInt start;
		UnsignedInt deadline;
	};
	std::map<ObjectID, BackingMove> m_backingMoves;
	ObjectID m_backingSource = INVALID_ID;
};

class BannerCarrierUpdateModuleData : public UpdateModuleData
{
public:
	struct Morph
	{
		AsciiString unitType;
		ModelConditionFlags conditions;
		Int locomotor = -1;
	};
	static void buildFieldParse(MultiIniFieldParse& p);
	std::vector<Morph> m_morphs;
	AsciiString m_morphFX;
	AsciiString m_unitSpawnFX;
	AsciiString m_upgradeRequired;
	UnsignedInt m_idleSpawnRate = 0;
	UnsignedInt m_meleeFreeUnitSpawnTime = 0;
	UnsignedInt m_diedRespawnTime = 0;
	UnsignedInt m_meleeFreeBannerRespawnTime = 0;
	Real m_scanHordeDistance = 0.0f;
	Bool m_replenishNearbyHorde = FALSE;
	Bool m_replenishAllNearbyHordes = FALSE;

private:
	static void parseMorph(INI* ini, void* instance, void* store, const void* userData);
};

class BannerCarrierUpdate : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(BannerCarrierUpdate, "BannerCarrierUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(BannerCarrierUpdate, BannerCarrierUpdateModuleData)

public:
	BannerCarrierUpdate(Thing* thing, const ModuleData* data);
	UpdateSleepTime update() override;
	void applyMorph(const AsciiString& unitType);
	void playMorphFX(const Object* horde) const;
	static BannerCarrierUpdate* find(Object* carrier);

private:
	const BannerCarrierUpdateModuleData::Morph* m_morph = nullptr;
};

struct HorseHordeContainModuleData : public HordeContainModuleData
{
};

struct AODHordeContainModuleData : public HordeContainModuleData
{
	static void buildFieldParse(MultiIniFieldParse& p);

	Real m_frequencyScale = 0.0f;
	Real m_frequencyRandomness = 0.0f;
	Real m_amplitudeScale = 0.0f;
	Real m_amplitudeRandomness = 0.0f;
	Real m_stillAmplitude = 0.0f;
	Real m_frequencyScaleZ = 0.0f;
	Real m_frequencyRandomnessZ = 0.0f;
	Real m_amplitudeScaleZ = 0.0f;
	Real m_amplitudeRandomnessZ = 0.0f;
	Real m_stillAmplitudeZ = 0.0f;
	Real m_oathFulfilledZFactor = 0.0f;
	Real m_largeUnitHeightFactor = 0.0f;
	Real m_largeUnitMinHeight = 0.0f;
	Real m_largeUnitMaxHeight = 0.0f;
	UnsignedInt m_largeUnitTimeout = 0;
	Real m_largeUnitTailOff = 0.0f;
	Real m_scatterSpeedFactor = 0.0f;
	Real m_scatterRandomness = 0.0f;
};

class HorseHordeContain final : public HordeContain
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( HorseHordeContain, "HorseHordeContain" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( HorseHordeContain, HorseHordeContainModuleData )

public:
	HorseHordeContain(Thing* thing, const ModuleData* data) : HordeContain(thing, data) {}

private:
	Bool m_legacySnapshot = FALSE;
};

class AODHordeContain final : public BehaviorModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AODHordeContain, "AODHordeContain" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AODHordeContain, AODHordeContainModuleData )

public:
	AODHordeContain(Thing* thing, const ModuleData* data) : BehaviorModule(thing, data) {}
};
