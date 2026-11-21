#pragma once

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
	static Bool canPass(const Object* obstacle, const Object* unit, Bool planning);
	static Bool couldConnect(const Object* unit);
	void request(Bool open);
	Bool wantsOpen() const { return m_wantsOpen; }
	UpdateSleepTime update() override;

private:
	void playSound(const AsciiString& name) const;
	void applyModelState();
	void configureGeometry();
	Bool permits(const Object& unit, Bool planning) const;

	UnsignedInt m_pathFrame = 0;
	UnsignedInt m_soundFrame = 0;
	UnsignedInt m_endFrame = 0;
	Bool m_wantsOpen = FALSE;
	Bool m_passable = FALSE;
	Bool m_moving = FALSE;
	Bool m_soundPending = FALSE;
	Bool m_geometryReady = FALSE;
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
