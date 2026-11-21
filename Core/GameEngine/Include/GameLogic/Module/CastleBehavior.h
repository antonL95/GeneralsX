#pragma once

class UpgradeTemplate;

#include "Common/SpecialPowerINI.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Module/UpdateModule.h"

class CastleBehaviorModuleData : public UpdateModuleData
{
public:
	struct CampEntry
	{
		AsciiString faction;
		AsciiString camp;
		UnsignedInt cost = 0;
		UnsignedInt extraValue = 0;
	};

	struct DecalEntry
	{
		AsciiString faction;
		AsciiString decal;
		Real size;
	};

	struct PreBuiltEntry
	{
		AsciiString name;
		Int count;
	};

	CastleBehaviorModuleData();
	static void buildFieldParse(MultiIniFieldParse& p);

	std::vector<CampEntry> m_castles;
	std::vector<DecalEntry> m_decals;
	std::vector<PreBuiltEntry> m_preBuiltList;
	SpecialPowerObjectFilter m_ownedFilter;
	SpecialPowerObjectFilter m_crewFilter;
	AsciiString m_preBuiltPlayer;
	AsciiString m_decalName;
	AsciiString m_crewPrepareFX;
	AsciiString m_crewReleaseFX;
	AsciiString m_enemySightedEvent;
	Real m_decalSize = 1.0f;
	Real m_fadeTime = 2.0f;
	Real m_unpackDelayTime = 2.0f;
	Real m_buildTime = 5.0f;
	Real m_maxCastleRadius = 0.0f;
	Real m_scanDistance = 100.0f;
	UnsignedInt m_crewPrepareTime = 18;
	UnsignedInt m_crewPrepareInterval = 10;
	Bool m_instantUnpack = FALSE;
	Bool m_keepDeathKillsEverything = FALSE;
	Bool m_summoned = FALSE;
	Bool m_disableStructureRotation = FALSE;
	Bool m_transferFoundationHealth = TRUE;

private:
	static void parseCampEntry(INI* ini, void*, void* store, const void*);
	static void parseDecalEntry(INI* ini, void*, void* store, const void*);
	static void parsePreBuiltEntry(INI* ini, void*, void* store, const void*);
};

class CastleBehavior final : public UpdateModule, public CreateModuleInterface
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(CastleBehavior, "CastleBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(CastleBehavior, CastleBehaviorModuleData)

public:
	CastleBehavior(Thing* thing, const ModuleData* data);
	static Int getInterfaceMask() { return MODULEINTERFACE_UPDATE | MODULEINTERFACE_CREATE; }

	CreateModuleInterface* getCreate() override { return this; }
	void onCreate() override;
	void onBuildComplete() override;
	Bool shouldDoOnBuildComplete() const override { return !m_buildComplete; }
	UpdateSleepTime update() override;
	Bool isUnpacked() const { return m_unpacked; }
	static CastleBehavior* find(const Object* object);
	Object* getKeep() const;
	Bool unpackFree();
	Bool orderUnpack(Player* player);
	Real getMaxCastleRadius() const { return getCastleBehaviorModuleData()->m_maxCastleRadius; }
	static CastleBehavior* findForMember(const Object& member);
	void grantMemberUpgrade(const Object& source, const UpgradeTemplate& upgrade);
	void applyKeepUpgrades(Object& member) const;

private:
	void unpack(Bool charge = FALSE);
	void checkKeep();
	void killEverything();
	void repack();
	void scanForCapture();
	const CastleBehaviorModuleData::CampEntry* findCamp(const Player& player) const;

	Bool m_buildComplete = FALSE;
	Bool m_pendingUnpack = FALSE;
	Bool m_unpacked = FALSE;
	Bool m_keepLost = FALSE;
	Bool m_orderedUnpack = FALSE;
	UnsignedInt m_unpackFrame = 0;
	ObjectID m_keep = INVALID_ID;
	std::vector<ObjectID> m_members;
};
