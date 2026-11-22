#pragma once

#include "Common/BuildAssistant.h"
#include "Common/SpecialPowerINI.h"
#include "GameLogic/Module/UpdateModule.h"

class WallHubBehaviorModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	UnsignedInt m_option = 0;
	Real m_maxDistance = 0.0f;
	Int m_staggerFactor = 0;
	Real m_builderRadius = 0.0f;
	std::vector<AsciiString> m_segments;
	AsciiString m_defaultSegment;
	AsciiString m_hubCap;
	AsciiString m_cliffCap;
	AsciiString m_borderCap;
	AsciiString m_shoreCap;
	AsciiString m_elevatedSegment;

private:
	static void parseOption(INI* ini, void*, void* store, const void*);
	static void parseSegment(INI* ini, void*, void* store, const void*);
};

class WallHubBehavior final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(WallHubBehavior, "WallHubBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(WallHubBehavior, WallHubBehaviorModuleData)

public:
	struct Piece
	{
		const ThingTemplate* type;
		Coord3D position;
		Real facing;
		Int cost;
	};

	struct Span
	{
		std::vector<Piece> pieces;
		UnsignedInt cost = 0;
		LegalBuildCode legal = LBC_OK;
		Bool affordable = FALSE;
	};

	WallHubBehavior(Thing* thing, const ModuleData* data);
	static WallHubBehavior* find(const Object* object, UnsignedInt options);
	Span plan(const ThingTemplate* requested, const Coord3D& target) const;
	Bool construct(const ThingTemplate* requested, const Coord3D& target, Player* player) const;
	static Bool canCancelSpan(const Object* hub);
	static Bool cancelSpan(const Object* hub, Player* player);
	UpdateSleepTime update() override { return UPDATE_SLEEP_FOREVER; }

private:
	Span plan(const ThingTemplate* requested, const Coord3D& target, UnsignedInt overlapChecks) const;
};

class GettingBuiltBehaviorModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	AsciiString m_worker;
	AsciiString m_evilWorker;
	AsciiString m_buildLoop;
	AsciiString m_repairLoop;
	AsciiString m_rebuildLoop;
	Real m_spawnTimer = 30.0f;
	Real m_rebuildTime = 60.0f;
	Bool m_timerWithoutWorker = FALSE;
	Bool m_rebuildWhenDead = FALSE;
	Bool m_testFaction = FALSE;
	Real m_disallowRebuildRange = 0.0f;
	SpecialPowerObjectFilter m_disallowRebuildFilter;
};

class GettingBuiltBehavior final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(GettingBuiltBehavior, "GettingBuiltBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(GettingBuiltBehavior, GettingBuiltBehaviorModuleData)

public:
	GettingBuiltBehavior(Thing* thing, const ModuleData* data);
	static const GettingBuiltBehavior* find(const Object& structure);
	static GettingBuiltBehavior* findMutable(Object& structure);
	static Object* start(const ThingTemplate& type, Object& builder, const Coord3D& position,
		Real facing, UnsignedInt delay, Int cost, ObjectID span);
	static Bool startRebuild(Object& structure);
	static Bool canOrderRebuild(const Object* structure, const Player* owner);
	static void onRubbleEntered(Object& structure);
	static Bool beginConstruction(Object& structure);
	static Bool takeOverConstruction(Object& structure);
	static Bool hasDelegatedConstruction(ObjectID builder);
	static Bool isAuthoredWorker(const Object& structure, const Object& worker);
	static Int getConstructionFrames(const Object& structure, Int fallback);
	Bool managesConstruction() const { return m_running; }
	const AsciiString* getActiveLoop() const { return m_currentLoop; }
	void onObjectCreated() override;
	void onDelete() override;
	static UnsignedInt constructionRefund(const Object& structure, const Player& player);
	ObjectID getSpan() const { return m_span; }
	UnsignedInt getRebuildFrames() const;
	Bool isRebuildingFromRubble() const { return m_rebuildingFromRubble; }
	Bool isWaitingToRebuild() const { return m_waitingToRebuild; }
	UnsignedInt getRebuildReadyFrame() const { return m_rebuildReadyFrame; }
	UpdateSleepTime update() override;

private:
	Bool canRebuildNow() const;
	UnsignedInt getSpawnDelayFrames() const;
	void xferRetailBlocks(Xfer* xfer, Bool present);
	Object* spawnBuilder(Bool repair);
	void releaseBuilder();
	UpdateSleepTime updateRepair();
	void playLoop(const AsciiString& name);
	void stopLoop();

	UnsignedInt m_startFrame = 0;
	Bool m_running = FALSE;
	Int m_paidCost = -1;
	ObjectID m_span = INVALID_ID;
	Bool m_rebuildingFromRubble = FALSE;
	Bool m_waitingToRebuild = FALSE;
	Bool m_keptAsRubble = FALSE;
	UnsignedInt m_rebuildReadyFrame = 0;
	ObjectID m_workerID = INVALID_ID;
	Bool m_workerSpawned = FALSE;
	UnsignedInt m_repairFrame = 0;
	const AsciiString* m_currentLoop = nullptr;
	UnsignedInt m_loopHandle = 0;
};

class WallUpgradeUpdateModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);
};

class WallUpgradeUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(WallUpgradeUpdate, "WallUpgradeUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(WallUpgradeUpdate, WallUpgradeUpdateModuleData)

public:
	WallUpgradeUpdate(Thing* thing, const ModuleData* data);
	static WallUpgradeUpdate* find(const Object* object);
	ObjectID getSupportingWallID() const { return m_supportingWall; }
	Bool isWaitingForWall() const { return m_waitingForWall; }
	UpdateSleepTime update() override;

private:
	void xferRetailBlocks(Xfer* xfer, Bool present);

	ObjectID m_supportingWall = INVALID_ID;
	Bool m_waitingForWall = FALSE;
};
