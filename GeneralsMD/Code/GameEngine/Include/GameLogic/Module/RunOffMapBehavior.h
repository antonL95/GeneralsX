#pragma once

#include "GameLogic/Module/UpdateModule.h"

struct RunOffMapBehaviorModuleData : public UpdateModuleData
{
	AsciiString m_waypoint;
	Bool m_requiresSpecificTrigger = FALSE;
	Bool m_dieOnMap = FALSE;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class RunOffMapBehavior final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(RunOffMapBehavior, "RunOffMapBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(RunOffMapBehavior, RunOffMapBehaviorModuleData)

public:
	RunOffMapBehavior(Thing* thing, const ModuleData* data);
	static RunOffMapBehavior* find(const Object* object);
	void trigger();
	Bool isWaitingForTrigger() const { return m_phase == Phase::Waiting; }
	Bool isRunning() const { return m_phase == Phase::Running; }
	UpdateSleepTime update() override;
	void crcRetailState(Xfer* xfer);

private:
	enum class Phase : UnsignedByte { Waiting, Pending, Running, Finished };
	Phase m_phase;
};
