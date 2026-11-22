#pragma once

#include <vector>

#include "Common/KindOf.h"
#include "Common/SpecialPowerINI.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Module/UpdateModule.h"

class GateOpenAndCloseBehaviorModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	UnsignedInt m_resetTime = 0;
	UnsignedInt m_openSoundTime = 0;
	UnsignedInt m_closeSoundTime = 0;
	UnsignedInt m_pathingPercent = 50;
	Bool m_openByDefault = FALSE;
	Bool m_repelUnits = FALSE;
	AsciiString m_openingSound;
	AsciiString m_closingSound;
	AsciiString m_openedSound;
	AsciiString m_closedSound;
	AsciiString m_proxy;
	std::vector<AsciiString> m_closedGeometry;
	std::vector<AsciiString> m_openGeometry;

private:
	static void parseGeometryList(INI* ini, void*, void* store, const void*);
};

class GateOpenAndCloseBehavior final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(GateOpenAndCloseBehavior, "GateOpenAndCloseBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(GateOpenAndCloseBehavior, GateOpenAndCloseBehaviorModuleData)

public:
	GateOpenAndCloseBehavior(Thing* thing, const ModuleData* data);
	static GateOpenAndCloseBehavior* find(const Object* object);
	static GateOpenAndCloseBehavior* resolveForControl(const Object* object);
	static Bool canPass(const Object* obstacle, const Object* unit, Bool planning);
	static Bool couldConnect(const Object* unit);
	void request(Bool open);
	Bool wantsOpen() const { return m_wantsOpen; }
	void setProxyID(ObjectID proxyID) { m_proxyID = proxyID; }
	ObjectID getProxyID() const { return m_proxyID; }
	const AsciiString& getProxyName() const { return getGateOpenAndCloseBehaviorModuleData()->m_proxy; }
	UpdateSleepTime update() override;

private:
	void playSound(const AsciiString& name) const;
	void applyModelState();
	void configureGeometry();
	void resolveProxy();
	void xferRetailBlocks(Xfer* xfer, Bool present);
	Bool permits(const Object& unit, Bool planning) const;

	UnsignedInt m_pathFrame = 0;
	UnsignedInt m_soundFrame = 0;
	UnsignedInt m_endFrame = 0;
	Bool m_wantsOpen = FALSE;
	Bool m_passable = FALSE;
	Bool m_moving = FALSE;
	Bool m_soundPending = FALSE;
	Bool m_geometryReady = FALSE;
	ObjectID m_proxyID = INVALID_ID;
};

class GateProxyBehaviorModuleData : public BehaviorModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);
};

class GateProxyBehavior final : public BehaviorModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(GateProxyBehavior, "GateProxyBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(GateProxyBehavior, GateProxyBehaviorModuleData)

public:
	GateProxyBehavior(Thing* thing, const ModuleData* data);
	static GateProxyBehavior* find(const Object* object);
	static GateOpenAndCloseBehavior* resolveGate(const Object* object);
	void setGateID(ObjectID gateID) { m_gateID = gateID; }
	ObjectID getGateID() const { return m_gateID; }

private:
	void xferRetailBlocks(Xfer* xfer, Bool present);

	ObjectID m_gateID = INVALID_ID;
};

class AIGateUpdateModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);
	Real m_triggerWidthX = 0.0f;
	Real m_triggerWidthY = 0.0f;
};

class AIGateUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(AIGateUpdate, "AIGateUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(AIGateUpdate, AIGateUpdateModuleData)

public:
	AIGateUpdate(Thing* thing, const ModuleData* data);
	UpdateSleepTime update() override;
};

class FakePathfindPortalBehaviourModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);
	Bool m_allowEnemies = FALSE;
	Bool m_allowNonSkirmishAI = FALSE;
};

class FakePathfindPortalBehaviour final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(FakePathfindPortalBehaviour, "FakePathfindPortalBehaviour")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(FakePathfindPortalBehaviour, FakePathfindPortalBehaviourModuleData)

public:
	FakePathfindPortalBehaviour(Thing* thing, const ModuleData* data);
	Bool permits(const Object& unit) const;
	UpdateSleepTime update() override { return UPDATE_SLEEP_FOREVER; }
};

class DynamicPortalBehaviourModuleData : public UpdateModuleData
{
public:
	struct WayPoint
	{
		Int index = 0;
		Int type = 0;
	};

	struct Link
	{
		Int from = 0;
		Int to = 0;
		std::vector<Int> vias;
	};

	static void buildFieldParse(MultiIniFieldParse& p);

	Bool m_generateNow = FALSE;
	Bool m_allowEnemies = FALSE;
	KindOfMaskType m_allowKindOf;
	KindOfMaskType m_rejectKindOf;
	AsciiString m_bonePrefix;
	Int m_numberOfBones = 0;
	std::vector<WayPoint> m_wayPoints;
	std::vector<Link> m_links;
	std::vector<AsciiString> m_triggeredBy;
	std::vector<AsciiString> m_conflictsWith;
	SpecialPowerAnimation m_customAnim;
	Real m_activationDelaySeconds = 0.0f;
	Int m_aboveWall = 0;
	Coord3D m_topAttackPos = {0.0f, 0.0f, 0.0f};
	Real m_topAttackRadius = 0.0f;
	SpecialPowerObjectFilter m_objectFilter;

private:
	static void parseWayPoint(INI* ini, void*, void* store, const void*);
	static void parseLink(INI* ini, void*, void* store, const void*);
};

class DynamicPortalBehaviour final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(DynamicPortalBehaviour, "DynamicPortalBehaviour")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(DynamicPortalBehaviour, DynamicPortalBehaviourModuleData)

public:
	DynamicPortalBehaviour(Thing* thing, const ModuleData* data);
	static DynamicPortalBehaviour* find(const Object* object);
	Bool isActive() const { return m_active; }
	Bool permits(const Object& unit) const;
	UpdateSleepTime update() override;

private:
	Bool shouldActivate() const;
	void xferRetailBlocks(Xfer* xfer, Bool present);

	Bool m_active = FALSE;
	Bool m_activationScheduled = FALSE;
	UnsignedInt m_activationFrame = 0;
};
