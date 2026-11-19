#pragma once

#include "Common/HordeFormation.h"
#include "Common/ModelState.h"
#include "Common/ObjectStatusTypes.h"
#include "Common/SpecialPowerINI.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/TransportContain.h"

#include <vector>

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
	SpecialPowerObjectFilter m_passengerFilter;
	SpecialPowerObjectFilter m_manualPickUpFilter;
	ObjectStatusMaskType m_objectStatusOfContained;
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
	Bool m_showPips;
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
	static void parseMeleeBehavior(INI* ini, void* instance, void* store, const void* userData);
};

class HordeContain : public TransportContain
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( HordeContain, "HordeContain" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( HordeContain, HordeContainModuleData )

public:
	HordeContain( Thing *thing, const ModuleData* moduleData );

	virtual Bool isValidContainerFor( const Object* obj, Bool checkCapacity ) const override;
	virtual Bool isEnclosingContainerFor( const Object *obj ) const override { return FALSE; }
	virtual Bool isDisplayedOnControlBar() const override { return FALSE; }
	virtual void onContaining( Object *obj, Bool wasSelected ) override;
	virtual void onRemoving( Object *obj ) override;
	virtual void onCapture( Player *oldOwner, Player *newOwner ) override;
	virtual void addToContainList( Object *obj ) override;

	Int getMemberSlot( ObjectID id ) const;

protected:
	virtual void createPayload() override;
	virtual void redeployOccupants() override;

private:
	void placeMember( Object *member, Int slot ) const;

	std::vector<ObjectID> m_slotMembers;
};
